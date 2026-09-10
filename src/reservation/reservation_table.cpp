#include "traffic/reservation/reservation_table.h"

#include <algorithm>
#include <utility>

#include "traffic/domain/resources.h"

namespace traffic::reservation {
namespace {

using TableStatus = core::Status<TableError>;

/// The most robots inside \p window at any one instant, given the claims that
/// contend for it.
///
/// Occupancy is a step function that only ever rises at the start of a claim,
/// so its maximum is reached at one of those starts — or at the start of the
/// requested window, where claims that began earlier are already inside.
/// Sampling those instants is exact, not an approximation.
///
/// Robots are counted, not reservations. One robot holding two overlapping
/// claims on a resource occupies one place in it.
[[nodiscard]] std::uint32_t peak_occupancy(const std::vector<const ReservationRecord*>& candidates,
                                           const domain::TimeWindow& window) {
    if (candidates.empty()) {
        return 0;
    }

    std::vector<core::TimePoint> instants;
    instants.reserve(candidates.size() + 1);
    instants.push_back(window.start());
    for (const ReservationRecord* record : candidates) {
        const core::TimePoint start = record->reservation.window.start();
        if (window.contains(start)) {
            instants.push_back(start);
        }
    }
    std::sort(instants.begin(), instants.end());
    instants.erase(std::unique(instants.begin(), instants.end()), instants.end());

    std::uint32_t peak = 0;
    for (const core::TimePoint instant : instants) {
        std::set<core::RobotId> robots;
        for (const ReservationRecord* record : candidates) {
            if (record->reservation.window.contains(instant)) {
                robots.insert(record->reservation.robot_id);
            }
        }
        peak = std::max(peak, static_cast<std::uint32_t>(robots.size()));
    }
    return peak;
}

[[nodiscard]] std::vector<domain::Reservation> to_reservations(
    const std::vector<const ReservationRecord*>& records) {
    std::vector<domain::Reservation> out;
    out.reserve(records.size());
    for (const ReservationRecord* record : records) {
        out.push_back(record->reservation);
    }
    return out;
}

}  // namespace

ReservationRecord make_record(domain::Reservation reservation,
                              ResourceUsage usage,
                              core::TimePoint requested_start,
                              core::Duration requested_duration,
                              core::TimePoint decided_at,
                              std::optional<core::RouteId> route_id,
                              std::optional<core::TaskId> task_id) {
    return ReservationRecord{
        .reservation = std::move(reservation),
        .usage = std::move(usage),
        .requested_start = requested_start,
        .requested_duration = requested_duration,
        .decided_at = decided_at,
        .last_transition_at = decided_at,
        .committed = false,
        .route_id = std::move(route_id),
        .task_id = std::move(task_id),
    };
}

std::string_view to_string(AdmissionResult result) noexcept {
    switch (result) {
        case AdmissionResult::admitted:
            return "ADMITTED";
        case AdmissionResult::capacity_exceeded:
            return "CAPACITY_EXCEEDED";
        case AdmissionResult::opposing_direction:
            return "OPPOSING_DIRECTION";
        case AdmissionResult::movement_conflict:
            return "MOVEMENT_CONFLICT";
    }
    return "UNKNOWN";
}

bool is_admitted(const AdmissionVerdict& verdict) noexcept {
    return verdict.result == AdmissionResult::admitted;
}

std::string_view to_string(TableError error) noexcept {
    switch (error) {
        case TableError::duplicate_id:
            return "DUPLICATE_ID";
        case TableError::unknown_reservation:
            return "UNKNOWN_RESERVATION";
        case TableError::invalid_transition:
            return "INVALID_TRANSITION";
        case TableError::invalid_window:
            return "INVALID_WINDOW";
    }
    return "UNKNOWN";
}

ReservationTable::ReservationTable(const map::Map& map) : map_(map) {}

// --------------------------------------------------------------- admission

std::uint32_t ReservationTable::capacity_of(const core::ResourceId& resource) const {
    if (const domain::Corridor* corridor = map_.find_corridor(resource); corridor != nullptr) {
        return corridor->capacity;
    }
    if (const domain::Intersection* crossing = map_.find_intersection(resource);
        crossing != nullptr) {
        return crossing->capacity;
    }
    if (const domain::WaitingBay* bay = map_.find_waiting_bay(resource); bay != nullptr) {
        return bay->capacity;
    }

    // An edge or node that no larger resource claims is its own resource, and
    // `Map::resource_for_edge` builds that resource id from the element's own
    // id. Looking it back up the same way is what makes those resources
    // capacity-aware rather than silently one.
    if (const domain::Edge* edge = map_.find_edge(core::EdgeId{resource.value()});
        edge != nullptr) {
        return edge->capacity;
    }
    if (const domain::Node* node = map_.find_node(core::NodeId{resource.value()});
        node != nullptr) {
        return node->capacity;
    }

    // Nothing in the map knows this resource. One robot at a time is the only
    // answer that cannot be wrong in the dangerous direction.
    return 1;
}

AdmissionVerdict ReservationTable::check(const core::ResourceId& resource,
                                         const core::RobotId& requester,
                                         const domain::TimeWindow& window,
                                         const ResourceUsage& usage) const {
    AdmissionVerdict verdict;
    verdict.capacity = capacity_of(resource);

    const std::vector<const ReservationRecord*> candidates =
        contenders(resource, window, requester);

    // Direction and movement exclusion are not capacity discounts: they refuse
    // claims that capacity alone would admit, and they are checked first so
    // the reported reason names the real obstacle.
    std::vector<const ReservationRecord*> opposing;
    std::vector<const ReservationRecord*> crossing;
    for (const ReservationRecord* record : candidates) {
        if (opposes_direction(resource, usage, record->usage)) {
            opposing.push_back(record);
        } else if (opposes_movement(resource, usage, record->usage)) {
            crossing.push_back(record);
        }
    }

    if (!opposing.empty()) {
        verdict.result = AdmissionResult::opposing_direction;
        explain(verdict, opposing);
        return verdict;
    }
    if (!crossing.empty()) {
        verdict.result = AdmissionResult::movement_conflict;
        explain(verdict, crossing);
        return verdict;
    }

    verdict.peak_holders = peak_occupancy(candidates, window);
    if (verdict.peak_holders >= verdict.capacity) {
        // Comparing `peak >= capacity` rather than `peak + 1 > capacity` says
        // the same thing and cannot overflow, and it is also the right answer
        // for a capacity of zero.
        verdict.result = AdmissionResult::capacity_exceeded;
        explain(verdict, candidates);
        return verdict;
    }

    verdict.result = AdmissionResult::admitted;
    return verdict;
}

// ------------------------------------------------------------------- write

core::Status<TableError> ReservationTable::insert(ReservationRecord record) {
    if (records_.contains(record.reservation.id)) {
        return TableStatus::failure(TableError::duplicate_id);
    }

    const core::ReservationId id = record.reservation.id;
    const core::RobotId robot = record.reservation.robot_id;

    const auto inserted = records_.emplace(id, std::move(record));
    ReservationRecord& stored = inserted.first->second;

    by_robot_[robot].insert(id);
    if (domain::holds_resource(stored.reservation.state)) {
        index(stored);
    }
    return TableStatus::success();
}

core::Status<TableError> ReservationTable::set_state(const core::ReservationId& id,
                                                     domain::ReservationState next,
                                                     core::TimePoint at) {
    const auto found = records_.find(id);
    if (found == records_.end()) {
        return TableStatus::failure(TableError::unknown_reservation);
    }

    ReservationRecord& record = found->second;
    auto moved = domain::with_state(record.reservation, next);
    if (!moved.has_value()) {
        // Includes released -> active, the invariant of
        // docs/24_DOMAIN_MODEL.md §30. The lifecycle table is the one place
        // that rule is written down, and this is what defers to it.
        return TableStatus::failure(TableError::invalid_transition);
    }

    if (domain::holds_resource(record.reservation.state)) {
        unindex(record);
    }
    record.reservation = std::move(moved).value();
    record.last_transition_at = at;
    if (domain::holds_resource(record.reservation.state)) {
        index(record);
    }
    return TableStatus::success();
}

core::Status<TableError> ReservationTable::set_window_end(const core::ReservationId& id,
                                                          core::TimePoint new_end) {
    const auto found = records_.find(id);
    if (found == records_.end()) {
        return TableStatus::failure(TableError::unknown_reservation);
    }

    ReservationRecord& record = found->second;
    if (domain::is_terminal(record.reservation.state)) {
        return TableStatus::failure(TableError::invalid_transition);
    }
    if (new_end <= record.reservation.window.start()) {
        return TableStatus::failure(TableError::invalid_window);
    }

    record.reservation.window = domain::TimeWindow{record.reservation.window.start(), new_end};
    return TableStatus::success();
}

core::Status<TableError> ReservationTable::mark_committed(const core::ReservationId& id) {
    const auto found = records_.find(id);
    if (found == records_.end()) {
        return TableStatus::failure(TableError::unknown_reservation);
    }
    if (domain::is_terminal(found->second.reservation.state)) {
        return TableStatus::failure(TableError::invalid_transition);
    }
    found->second.committed = true;
    return TableStatus::success();
}

std::size_t ReservationTable::purge_terminal_before(core::TimePoint cutoff) {
    std::size_t removed = 0;
    for (auto it = records_.begin(); it != records_.end();) {
        const ReservationRecord& record = it->second;
        if (!domain::is_terminal(record.reservation.state) ||
            record.reservation.window.end() >= cutoff) {
            ++it;
            continue;
        }

        const auto robot = by_robot_.find(record.reservation.robot_id);
        if (robot != by_robot_.end()) {
            robot->second.erase(record.reservation.id);
            if (robot->second.empty()) {
                by_robot_.erase(robot);
            }
        }
        it = records_.erase(it);
        ++removed;
    }
    return removed;
}

// ------------------------------------------------------------------- query

const ReservationRecord* ReservationTable::find(const core::ReservationId& id) const {
    const auto found = records_.find(id);
    return found == records_.end() ? nullptr : &found->second;
}

bool ReservationTable::contains(const core::ReservationId& id) const {
    return records_.contains(id);
}

std::size_t ReservationTable::size() const noexcept {
    return records_.size();
}

std::size_t ReservationTable::holding_count() const noexcept {
    return holding_count_;
}

std::vector<core::ReservationId> ReservationTable::holding_ids() const {
    std::vector<core::ReservationId> out;
    for (const auto& [id, record] : records_) {
        if (domain::holds_resource(record.reservation.state)) {
            out.push_back(id);
        }
    }
    return out;
}

std::vector<domain::Reservation> ReservationTable::find_conflicts(
    const core::ResourceId& resource, const domain::TimeWindow& window) const {
    // An empty robot id excludes nobody: `make_reservation` refuses an empty
    // robot id, so no stored claim can match it.
    return to_reservations(contenders(resource, window, core::RobotId{}));
}

std::vector<domain::Reservation> ReservationTable::find_contenders(
    const core::ResourceId& resource,
    const domain::TimeWindow& window,
    const core::RobotId& requester) const {
    return to_reservations(contenders(resource, window, requester));
}

std::vector<domain::Reservation> ReservationTable::holders(const core::ResourceId& resource) const {
    std::vector<domain::Reservation> out;
    const auto bucket = holding_by_resource_.find(resource);
    if (bucket == holding_by_resource_.end()) {
        return out;
    }
    for (const auto& [key, id] : bucket->second) {
        if (const ReservationRecord* record = find(id); record != nullptr) {
            out.push_back(record->reservation);
        }
    }
    return out;
}

std::vector<domain::Reservation> ReservationTable::active_reservations(
    const core::ResourceId& resource) const {
    std::vector<domain::Reservation> out;
    for (const domain::Reservation& reservation : holders(resource)) {
        if (reservation.state == domain::ReservationState::active) {
            out.push_back(reservation);
        }
    }
    return out;
}

std::vector<domain::Reservation> ReservationTable::active_reservations() const {
    std::vector<domain::Reservation> out;
    for (const auto& [id, record] : records_) {
        if (record.reservation.state == domain::ReservationState::active) {
            out.push_back(record.reservation);
        }
    }
    return out;
}

std::vector<core::RobotId> ReservationTable::owners_of(const core::ResourceId& resource) const {
    std::set<core::RobotId> owners;
    for (const domain::Reservation& reservation : holders(resource)) {
        owners.insert(reservation.robot_id);
    }
    return {owners.begin(), owners.end()};
}

std::vector<domain::Reservation> ReservationTable::reservations_for(
    const core::RobotId& robot) const {
    std::vector<domain::Reservation> out;
    const auto found = by_robot_.find(robot);
    if (found == by_robot_.end()) {
        return out;
    }
    for (const core::ReservationId& id : found->second) {
        if (const ReservationRecord* record = find(id); record != nullptr) {
            out.push_back(record->reservation);
        }
    }
    return out;
}

std::vector<core::ReservationId> ReservationTable::holding_ids_for(
    const core::RobotId& robot) const {
    std::vector<core::ReservationId> out;
    const auto found = by_robot_.find(robot);
    if (found == by_robot_.end()) {
        return out;
    }
    for (const core::ReservationId& id : found->second) {
        const ReservationRecord* record = find(id);
        if (record != nullptr && domain::holds_resource(record->reservation.state)) {
            out.push_back(id);
        }
    }
    return out;
}

std::optional<core::TimePoint> ReservationTable::next_available(
    const core::ResourceId& resource,
    const core::RobotId& requester,
    const domain::TimeWindow& window) const {
    return check(resource, requester, window, ResourceUsage{}).next_available;
}

// ----------------------------------------------------------------- private

ReservationTable::Key ReservationTable::key_of(const ReservationRecord& record) {
    return Key{.start = record.reservation.window.start(), .id = record.reservation.id};
}

void ReservationTable::index(const ReservationRecord& record) {
    holding_by_resource_[record.reservation.resource_id].emplace(key_of(record),
                                                                 record.reservation.id);
    ++holding_count_;
}

void ReservationTable::unindex(const ReservationRecord& record) {
    const auto bucket = holding_by_resource_.find(record.reservation.resource_id);
    if (bucket == holding_by_resource_.end()) {
        return;
    }
    if (bucket->second.erase(key_of(record)) > 0) {
        --holding_count_;
    }
    if (bucket->second.empty()) {
        holding_by_resource_.erase(bucket);
    }
}

std::vector<const ReservationRecord*> ReservationTable::contenders(
    const core::ResourceId& resource,
    const domain::TimeWindow& window,
    const core::RobotId& requester) const {
    std::vector<const ReservationRecord*> out;

    const auto bucket = holding_by_resource_.find(resource);
    if (bucket == holding_by_resource_.end()) {
        return out;
    }

    for (const auto& [key, id] : bucket->second) {
        if (key.start >= window.end()) {
            // Ordered by start: nothing further along can reach back into the
            // window, so the scan stops here rather than running to the end.
            break;
        }
        const ReservationRecord* record = find(id);
        if (record == nullptr) {
            continue;
        }
        if (record->reservation.robot_id == requester) {
            continue;
        }
        if (!record->reservation.window.overlaps(window)) {
            continue;
        }
        out.push_back(record);
    }
    return out;
}

bool ReservationTable::opposes_direction(const core::ResourceId& resource,
                                         const ResourceUsage& lhs,
                                         const ResourceUsage& rhs) const {
    if (map_.find_corridor(resource) == nullptr) {
        return false;
    }
    // Applied whatever the corridor's capacity. `can_deadlock_head_on` is true
    // only at capacity one, where capacity already refuses the second robot; a
    // wider bidirectional corridor still has to hold one direction at a time
    // (docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §7~8), and capacity cannot say
    // that.
    if (lhs.direction.has_value() && rhs.direction.has_value()) {
        return lhs.direction.value() != rhs.direction.value();
    }
    return false;
}

bool ReservationTable::opposes_movement(const core::ResourceId& resource,
                                        const ResourceUsage& lhs,
                                        const ResourceUsage& rhs) const {
    const domain::Intersection* crossing = map_.find_intersection(resource);
    if (crossing == nullptr) {
        return false;
    }
    if (lhs.movement.has_value() && rhs.movement.has_value()) {
        return domain::movements_conflict(*crossing, lhs.movement.value(), rhs.movement.value());
    }
    return false;
}

void ReservationTable::explain(AdmissionVerdict& verdict,
                               const std::vector<const ReservationRecord*>& blockers) {
    std::set<core::RobotId> robots;
    std::optional<core::TimePoint> earliest;

    for (const ReservationRecord* record : blockers) {
        verdict.blockers.push_back(record->reservation.id);
        robots.insert(record->reservation.robot_id);

        const core::TimePoint end = record->reservation.window.end();
        if (!earliest.has_value() || end < earliest.value()) {
            earliest = end;
        }
    }

    verdict.blocking_robots.assign(robots.begin(), robots.end());
    verdict.next_available = earliest;
}

}  // namespace traffic::reservation
