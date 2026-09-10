#include "traffic/reservation/reservation_manager.h"

#include <algorithm>
#include <set>
#include <utility>

#include "traffic/domain/reservation_state.h"

namespace traffic::reservation {
namespace {

using ReservationStatus = core::Status<ReservationError>;

[[nodiscard]] DenialReason reason_for(AdmissionResult result) noexcept {
    switch (result) {
        case AdmissionResult::admitted:
            return DenialReason::none;
        case AdmissionResult::capacity_exceeded:
            return DenialReason::resource_occupied;
        case AdmissionResult::opposing_direction:
            return DenialReason::opposing_direction;
        case AdmissionResult::movement_conflict:
            return DenialReason::movement_conflict;
    }
    return DenialReason::resource_occupied;
}

/// The robots behind a set of claims, de-duplicated and in id order.
[[nodiscard]] std::vector<core::RobotId> robots_of(
    const std::vector<domain::Reservation>& reservations) {
    std::set<core::RobotId> robots;
    for (const domain::Reservation& reservation : reservations) {
        robots.insert(reservation.robot_id);
    }
    return {robots.begin(), robots.end()};
}

}  // namespace

std::string_view to_string(ReservationError error) noexcept {
    switch (error) {
        case ReservationError::none:
            return "NONE";
        case ReservationError::unknown_reservation:
            return "UNKNOWN_RESERVATION";
        case ReservationError::already_terminal:
            return "ALREADY_TERMINAL";
        case ReservationError::invalid_transition:
            return "INVALID_TRANSITION";
        case ReservationError::not_an_extension:
            return "NOT_AN_EXTENSION";
        case ReservationError::extension_blocked:
            return "EXTENSION_BLOCKED";
        case ReservationError::committed:
            return "COMMITTED";
    }
    return "UNKNOWN";
}

ReservationManager::ReservationManager(ReservationTable& table, ReservationPolicy policy)
    : table_(table), policy_(policy) {}

// ------------------------------------------------------------------ decide

ReservationDecision ReservationManager::request(const ReservationRequest& request,
                                                core::TimePoint now) {
    const core::Duration waited = waited_for(request.robot_id, request.resource_id, now);
    const domain::Priority effective = effective_priority(request.priority, waited, policy_);

    ReservationDecision decision;
    decision.status = DecisionStatus::rejected;
    decision.reservation_id = request.reservation_id;
    decision.robot_id = request.robot_id;
    decision.resource_id = request.resource_id;
    decision.window_start = request.requested_start;
    decision.window_end = request.requested_start + request.estimated_duration;
    decision.effective_priority = effective;
    decision.decided_at = now;

    if (request.reservation_id.empty() || request.robot_id.empty() || request.resource_id.empty()) {
        decision.reason = DenialReason::empty_id;
        ++metrics_.rejected;
        return journal(std::move(decision));
    }

    // §35. The id is the idempotency key, so a repeated request is answered
    // from the reservation it already made rather than making a second one.
    if (const ReservationRecord* existing = table_.find(request.reservation_id);
        existing != nullptr) {
        return journal(replay(*existing, request, std::move(decision)));
    }

    const auto window =
        effective_window(request.requested_start, request.estimated_duration, policy_);
    if (!window.has_value()) {
        decision.reason = DenialReason::invalid_window;
        ++metrics_.rejected;
        return journal(std::move(decision));
    }

    // The buffered window, not the requested one, is what everyone else is
    // kept out of — §13. A buffer nobody is excluded from protects nothing.
    const domain::TimeWindow granted = window.value();
    decision.window_start = granted.start();
    decision.window_end = granted.end();

    if (request.requested_start > now + policy_.horizon) {
        // §28~29: reserving further ahead than the horizon locks resources
        // nobody is near yet, which reads on the floor as traffic stopping for
        // robots that are not there.
        decision.reason = DenialReason::beyond_horizon;
        ++metrics_.rejected;
        return journal(std::move(decision));
    }

    const AdmissionVerdict verdict =
        table_.check(request.resource_id, request.robot_id, granted, request.usage);
    if (!is_admitted(verdict)) {
        decision.status = DecisionStatus::wait;
        decision.reason = reason_for(verdict.result);
        decision.next_available = verdict.next_available;
        decision.blocking_robots = verdict.blocking_robots;
        note_wait(request, verdict, now);
        ++metrics_.waited;
        return journal(std::move(decision));
    }

    // The check and the write are one step, with nothing between them that
    // could grant the same window to somebody else — §34.
    auto reservation = domain::make_reservation(
        request.reservation_id, request.robot_id, request.resource_id, granted, effective, now);
    if (!reservation.has_value()) {
        decision.reason = DenialReason::invalid_window;
        ++metrics_.rejected;
        return journal(std::move(decision));
    }

    const auto inserted = table_.insert(make_record(std::move(reservation).value(),
                                                    request.usage,
                                                    request.requested_start,
                                                    request.estimated_duration,
                                                    now,
                                                    request.route_id,
                                                    request.task_id));
    if (!inserted.has_value()) {
        decision.reason = DenialReason::duplicate_request;
        ++metrics_.rejected;
        return journal(std::move(decision));
    }

    clear_wait(request.robot_id);
    decision.status = DecisionStatus::granted;
    decision.reason = DenialReason::none;
    ++metrics_.granted;
    return journal(std::move(decision));
}

std::vector<ReservationDecision> ReservationManager::request_all(
    std::vector<ReservationRequest> requests, core::TimePoint now) {
    // docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §16, in order: priority,
    // waiting time, task id, robot id — then reservation id, so that two
    // requests from one robot still have a fixed order. Without that last
    // step the comparator is not a total order and the outcome would depend on
    // the order the caller happened to build the vector in.
    std::sort(requests.begin(),
              requests.end(),
              [this, now](const ReservationRequest& lhs, const ReservationRequest& rhs) {
                  const core::Duration lhs_waited = waited_for(lhs.robot_id, lhs.resource_id, now);
                  const core::Duration rhs_waited = waited_for(rhs.robot_id, rhs.resource_id, now);

                  const domain::Priority lhs_priority =
                      effective_priority(lhs.priority, lhs_waited, policy_);
                  const domain::Priority rhs_priority =
                      effective_priority(rhs.priority, rhs_waited, policy_);

                  if (lhs_priority != rhs_priority) {
                      return lhs_priority > rhs_priority;
                  }
                  if (lhs_waited != rhs_waited) {
                      return lhs_waited > rhs_waited;
                  }
                  if (lhs.task_id != rhs.task_id) {
                      return lhs.task_id < rhs.task_id;
                  }
                  if (lhs.robot_id != rhs.robot_id) {
                      return lhs.robot_id < rhs.robot_id;
                  }
                  return lhs.reservation_id < rhs.reservation_id;
              });

    std::vector<ReservationDecision> decisions;
    decisions.reserve(requests.size());
    for (const ReservationRequest& one : requests) {
        decisions.push_back(request(one, now));
    }
    return decisions;
}

// --------------------------------------------------------------- lifecycle

core::Status<ReservationError> ReservationManager::activate(const core::ReservationId& id,
                                                            core::TimePoint now) {
    const ReservationRecord* record = table_.find(id);
    if (record == nullptr) {
        return ReservationStatus::failure(ReservationError::unknown_reservation);
    }
    if (domain::is_terminal(record->reservation.state)) {
        return ReservationStatus::failure(ReservationError::already_terminal);
    }
    if (!table_.set_state(id, domain::ReservationState::active, now).has_value()) {
        return ReservationStatus::failure(ReservationError::invalid_transition);
    }
    return ReservationStatus::success();
}

ExtensionOutcome ReservationManager::extend(const core::ReservationId& id,
                                            core::TimePoint new_end,
                                            core::TimePoint now) {
    ExtensionOutcome outcome;
    outcome.reservation_id = id;
    outcome.decided_at = now;

    const ReservationRecord* record = table_.find(id);
    if (record == nullptr) {
        outcome.error = ReservationError::unknown_reservation;
        return outcome;
    }
    if (domain::is_terminal(record->reservation.state)) {
        outcome.error = ReservationError::already_terminal;
        return outcome;
    }

    const domain::TimeWindow current = record->reservation.window;
    const core::ResourceId resource = record->reservation.resource_id;
    const core::RobotId robot = record->reservation.robot_id;
    outcome.new_end = current.end();

    if (new_end <= current.end()) {
        outcome.error = ReservationError::not_an_extension;
        return outcome;
    }

    // Only the added span needs checking. The robot already owns everything up
    // to its current end.
    const domain::TimeWindow addition{current.end(), new_end};

    if (is_admitted(table_.check(resource, robot, addition, record->usage))) {
        // Room for it as things stand. Nobody is displaced, and a wider
        // resource is not emptied to make way for a robot it had space for.
        if (!table_.set_window_end(id, new_end).has_value()) {
            outcome.error = ReservationError::invalid_transition;
            return outcome;
        }
        outcome.granted = true;
        outcome.new_end = new_end;
        ++metrics_.extended;
        return outcome;
    }

    const std::vector<domain::Reservation> displaced =
        table_.find_contenders(resource, addition, robot);

    for (const domain::Reservation& other : displaced) {
        const ReservationRecord* blocker = table_.find(other.id);
        const bool inside = other.state == domain::ReservationState::active;
        const bool going_in = blocker != nullptr && blocker->committed;

        if (inside || going_in) {
            // Another robot is in the resource, or committed to entering it
            // and past being called back (§30). §20's answer is that the
            // second robot waits or replans, and that is a decision about two
            // moving robots — not something to settle by rewriting a window.
            outcome.error = ReservationError::extension_blocked;
            outcome.affected_robots = robots_of(displaced);
            return outcome;
        }
    }
    if (table_.capacity_of(resource) == 0) {
        // Unreachable through a validated map, and checked anyway: it is what
        // makes "cancelling every contender leaves room" true rather than
        // merely likely.
        outcome.error = ReservationError::extension_blocked;
        outcome.affected_robots = robots_of(displaced);
        return outcome;
    }

    // Every contender is a grant nobody has acted on yet. The robot holding
    // the resource is physically still in it, so the honest move is to take
    // the queued grants back and let the controller replan them — §20.
    for (const domain::Reservation& other : displaced) {
        if (!table_.set_state(other.id, domain::ReservationState::cancelled, now).has_value()) {
            continue;
        }
        outcome.revoked.push_back(other.id);
        ++metrics_.revoked;
        ++metrics_.cancelled;
    }
    outcome.affected_robots = robots_of(displaced);

    if (!table_.set_window_end(id, new_end).has_value()) {
        outcome.error = ReservationError::invalid_transition;
        return outcome;
    }

    outcome.granted = true;
    outcome.new_end = new_end;
    ++metrics_.extended;
    return outcome;
}

core::Status<ReservationError> ReservationManager::release(const core::ReservationId& id,
                                                           core::TimePoint now) {
    const ReservationRecord* record = table_.find(id);
    if (record == nullptr) {
        return ReservationStatus::failure(ReservationError::unknown_reservation);
    }
    if (domain::is_terminal(record->reservation.state)) {
        return ReservationStatus::failure(ReservationError::already_terminal);
    }
    if (!table_.set_state(id, domain::ReservationState::released, now).has_value()) {
        // A grant the robot never entered is withdrawn, not given back: the
        // lifecycle only allows active -> released, and `cancel` is the door
        // for the other case.
        return ReservationStatus::failure(ReservationError::invalid_transition);
    }
    ++metrics_.released;
    return ReservationStatus::success();
}

std::vector<core::ReservationId> ReservationManager::release_all_for(const core::RobotId& robot,
                                                                     core::TimePoint now) {
    std::vector<core::ReservationId> gone;

    for (const core::ReservationId& id : table_.holding_ids_for(robot)) {
        const ReservationRecord* record = table_.find(id);
        if (record == nullptr) {
            continue;
        }

        const bool entered = record->reservation.state == domain::ReservationState::active;
        const domain::ReservationState next =
            entered ? domain::ReservationState::released : domain::ReservationState::cancelled;

        if (!table_.set_state(id, next, now).has_value()) {
            continue;
        }
        gone.push_back(id);
        if (entered) {
            ++metrics_.released;
        } else {
            ++metrics_.cancelled;
        }
    }

    clear_wait(robot);
    return gone;
}

core::Status<ReservationError> ReservationManager::cancel(const core::ReservationId& id,
                                                          core::TimePoint now) {
    const ReservationRecord* record = table_.find(id);
    if (record == nullptr) {
        return ReservationStatus::failure(ReservationError::unknown_reservation);
    }
    if (record->committed) {
        // §30. Past the commit point the robot is going in; taking the
        // reservation away only removes the record that it is occupied.
        return ReservationStatus::failure(ReservationError::committed);
    }
    if (domain::is_terminal(record->reservation.state)) {
        return ReservationStatus::failure(ReservationError::already_terminal);
    }
    if (!table_.set_state(id, domain::ReservationState::cancelled, now).has_value()) {
        // An active claim cannot be cancelled — the robot is inside. Release
        // it or expire it.
        return ReservationStatus::failure(ReservationError::invalid_transition);
    }
    ++metrics_.cancelled;
    return ReservationStatus::success();
}

core::Status<ReservationError> ReservationManager::commit(const core::ReservationId& id) {
    const auto marked = table_.mark_committed(id);
    if (marked.has_value()) {
        return ReservationStatus::success();
    }
    if (marked.error() == TableError::unknown_reservation) {
        return ReservationStatus::failure(ReservationError::unknown_reservation);
    }
    return ReservationStatus::failure(ReservationError::already_terminal);
}

core::Status<ReservationError> ReservationManager::expire(const core::ReservationId& id,
                                                          core::TimePoint now) {
    const ReservationRecord* record = table_.find(id);
    if (record == nullptr) {
        return ReservationStatus::failure(ReservationError::unknown_reservation);
    }
    if (domain::is_terminal(record->reservation.state)) {
        return ReservationStatus::failure(ReservationError::already_terminal);
    }
    if (!table_.set_state(id, domain::ReservationState::expired, now).has_value()) {
        return ReservationStatus::failure(ReservationError::invalid_transition);
    }
    ++metrics_.expired;
    return ReservationStatus::success();
}

SweepReport ReservationManager::sweep(core::TimePoint now) {
    SweepReport report;

    // A snapshot of the ids first: the loop changes states, and an index being
    // walked is not a safe thing to change.
    for (const core::ReservationId& id : table_.holding_ids()) {
        const ReservationRecord* record = table_.find(id);
        if (record == nullptr) {
            continue;
        }

        if (record->reservation.state == domain::ReservationState::active) {
            if (domain::is_past_window(record->reservation, now)) {
                // Reported, not expired. §22: nothing is released on a timer
                // without checking the real state, and an active claim means a
                // robot may be standing in the resource right now.
                report.overrunning.push_back(id);
            }
            continue;
        }

        const bool window_gone = domain::is_past_window(record->reservation, now);
        const bool never_taken_up = now >= record->decided_at + policy_.grant_timeout;
        if (!window_gone && !never_taken_up) {
            continue;
        }
        if (table_.set_state(id, domain::ReservationState::expired, now).has_value()) {
            report.expired.push_back(id);
            ++metrics_.expired;
        }
    }

    return report;
}

RecoveryReport ReservationManager::recover(const std::vector<ReservationRecord>& records,
                                           core::TimePoint now) {
    RecoveryReport report;

    // Earliest claim first, ties broken by id. Restoring in the order the
    // claims were made is what makes recovery reproducible, and it gives the
    // resource back to whoever had it first if the persisted set turns out to
    // be inconsistent.
    std::vector<ReservationRecord> ordered = records;
    std::sort(ordered.begin(),
              ordered.end(),
              [](const ReservationRecord& lhs, const ReservationRecord& rhs) {
                  if (lhs.reservation.window.start() != rhs.reservation.window.start()) {
                      return lhs.reservation.window.start() < rhs.reservation.window.start();
                  }
                  return lhs.reservation.id < rhs.reservation.id;
              });

    for (const ReservationRecord& record : ordered) {
        const core::ReservationId id = record.reservation.id;

        if (table_.contains(id)) {
            report.rejected.push_back(id);
            continue;
        }

        if (domain::is_terminal(record.reservation.state)) {
            // History. It withholds nothing, so there is nothing to check.
            if (table_.insert(record).has_value()) {
                report.restored.push_back(id);
            } else {
                report.rejected.push_back(id);
            }
            continue;
        }

        const AdmissionVerdict verdict = table_.check(record.reservation.resource_id,
                                                      record.reservation.robot_id,
                                                      record.reservation.window,
                                                      record.usage);
        if (!is_admitted(verdict)) {
            // Two persisted claims that cannot both be true. Neither is
            // trusted over the other beyond the ordering above; §32 says to
            // re-derive these from where the robots actually are.
            report.rejected.push_back(id);
            continue;
        }
        if (!table_.insert(record).has_value()) {
            report.rejected.push_back(id);
            continue;
        }

        const bool stale = record.reservation.state == domain::ReservationState::pending &&
                           domain::is_past_window(record.reservation, now);
        if (stale && table_.set_state(id, domain::ReservationState::expired, now).has_value()) {
            report.expired.push_back(id);
            ++metrics_.expired;
            continue;
        }
        report.restored.push_back(id);
    }

    return report;
}

// ------------------------------------------------------------------- query

const ReservationRecord* ReservationManager::get_reservation(const core::ReservationId& id) const {
    return table_.find(id);
}

std::vector<domain::Reservation> ReservationManager::active_reservations() const {
    return table_.active_reservations();
}

std::vector<domain::Reservation> ReservationManager::active_reservations(
    const core::ResourceId& resource) const {
    return table_.active_reservations(resource);
}

std::vector<domain::Reservation> ReservationManager::find_conflicts(
    const core::ResourceId& resource, const domain::TimeWindow& window) const {
    return table_.find_conflicts(resource, window);
}

std::vector<core::RobotId> ReservationManager::affected_robots(
    const core::ResourceId& resource, const core::ReservationId& id) const {
    const ReservationRecord* record = table_.find(id);
    if (record == nullptr) {
        return {};
    }
    return robots_of(
        table_.find_contenders(resource, record->reservation.window, record->reservation.robot_id));
}

std::vector<WaitEdge> ReservationManager::wait_for_edges() const {
    std::vector<WaitEdge> edges;
    for (const auto& [robot, entry] : waits_) {
        for (const core::RobotId& holder : entry.blocked_by) {
            WaitEdge edge;
            edge.waiting_robot = robot;
            edge.holding_robot = holder;
            edge.resource = entry.resource;
            edge.since = entry.since;
            edges.push_back(edge);
        }
    }
    return edges;
}

std::span<const ReservationDecision> ReservationManager::decision_log() const noexcept {
    return {journal_};
}

const ReservationMetrics& ReservationManager::metrics() const noexcept {
    return metrics_;
}

const ReservationTable& ReservationManager::table() const noexcept {
    return table_;
}

core::Duration ReservationManager::waited_for(const core::RobotId& robot,
                                              const core::ResourceId& resource,
                                              core::TimePoint now) const {
    const auto found = waits_.find(robot);
    if (found == waits_.end() || found->second.resource != resource) {
        return core::Duration::zero();
    }
    if (now <= found->second.since) {
        return core::Duration::zero();
    }
    return now - found->second.since;
}

// ----------------------------------------------------------------- private

ReservationDecision ReservationManager::journal(ReservationDecision decision) {
    if (policy_.decision_log_capacity == 0) {
        return decision;
    }
    if (journal_.size() >= policy_.decision_log_capacity) {
        journal_.erase(journal_.begin());
    }
    journal_.push_back(decision);
    return decision;
}

ReservationDecision ReservationManager::replay(const ReservationRecord& existing,
                                               const ReservationRequest& request,
                                               ReservationDecision decision) {
    decision.window_start = existing.reservation.window.start();
    decision.window_end = existing.reservation.window.end();

    if (existing.reservation.robot_id != request.robot_id ||
        existing.reservation.resource_id != request.resource_id) {
        // The same key for a different claim is not a retry, it is a
        // collision, and answering it with the other robot's grant would hand
        // one robot's resource to another.
        decision.status = DecisionStatus::rejected;
        decision.reason = DenialReason::duplicate_request;
        ++metrics_.rejected;
        return decision;
    }

    if (domain::is_terminal(existing.reservation.state)) {
        // docs/24_DOMAIN_MODEL.md §30. A retransmitted request must not bring
        // a finished claim back: by now another robot may be in the resource.
        decision.status = DecisionStatus::rejected;
        decision.reason = DenialReason::already_terminal;
        ++metrics_.rejected;
        return decision;
    }

    decision.status = DecisionStatus::granted;
    decision.reason = DenialReason::none;
    decision.replayed = true;
    ++metrics_.replayed;
    return decision;
}

void ReservationManager::note_wait(const ReservationRequest& request,
                                   const AdmissionVerdict& verdict,
                                   core::TimePoint now) {
    const auto found = waits_.find(request.robot_id);
    if (found != waits_.end() && found->second.resource == request.resource_id) {
        // Still waiting on the same thing: the start stands, which is what
        // makes the aging of §17 add up across retries instead of resetting on
        // every ask.
        found->second.blocked_by = verdict.blocking_robots;
        return;
    }

    WaitEntry entry;
    entry.resource = request.resource_id;
    entry.since = now;
    entry.blocked_by = verdict.blocking_robots;
    waits_.insert_or_assign(request.robot_id, std::move(entry));
}

void ReservationManager::clear_wait(const core::RobotId& robot) {
    waits_.erase(robot);
}

}  // namespace traffic::reservation
