#include "traffic/state/state_manager.h"

#include <utility>

namespace traffic::state {
namespace {

using RegisterStatus = core::Status<RegistrationRejection>;
using UpdateStatus = core::Status<UpdateRejection>;

}  // namespace

std::string_view to_string(RegistrationRejection rejection) noexcept {
    switch (rejection) {
        case RegistrationRejection::empty_robot_id:
            return "EMPTY_ROBOT_ID";
        case RegistrationRejection::already_registered:
            return "ALREADY_REGISTERED";
    }
    return "UNKNOWN";
}

StateManager::StateManager(StallPolicy policy) : policy_(policy) {}

// ------------------------------------------------------------------ register

core::Status<RegistrationRejection> StateManager::register_robot(domain::Robot robot,
                                                                 core::TimePoint at) {
    if (robot.id.empty()) {
        return RegisterStatus::failure(RegistrationRejection::empty_robot_id);
    }
    if (robots_.contains(robot.id)) {
        // Re-registering would silently reset whatever the robot was doing.
        // A fleet that registers the same id twice has a configuration
        // problem, and hiding it behind an update makes it harder to find.
        return RegisterStatus::failure(RegistrationRejection::already_registered);
    }

    RobotRecord record;
    record.snapshot.robot_id = robot.id;
    record.snapshot.state = domain::RobotState::idle;
    record.snapshot.observed_at = at;
    record.progress.at = at;
    record.controller_timestamp = at;
    record.robot = std::move(robot);

    const core::RobotId key = record.robot.id;
    touch(record, at);
    robots_.emplace(key, std::move(record));

    return RegisterStatus::success();
}

bool StateManager::deregister_robot(const core::RobotId& robot_id) {
    const bool existed = robots_.erase(robot_id) > 0;
    if (existed) {
        // The fleet is different from what a plan made a moment ago assumed.
        version_ = version_.next();
    }
    return existed;
}

bool StateManager::is_registered(const core::RobotId& robot_id) const {
    return robots_.contains(robot_id);
}

std::size_t StateManager::robot_count() const noexcept {
    return robots_.size();
}

// -------------------------------------------------------------------- update

core::Status<UpdateRejection> StateManager::apply(const RobotStateUpdate& update) {
    if (const auto shape = validate(update); !shape.has_value()) {
        return shape;
    }

    RobotRecord* record = find(update.robot_id);
    if (record == nullptr) {
        return UpdateStatus::failure(UpdateRejection::unknown_robot);
    }

    // Equal timestamps are accepted: a robot may send position and battery in
    // separate messages taken from the same measurement. Older ones are not —
    // applying them would move the fleet's picture backwards, and a decision
    // taken on the result would describe a world that has already changed.
    if (update.timestamp < record->snapshot.observed_at) {
        return UpdateStatus::failure(UpdateRejection::stale_timestamp);
    }

    const domain::RobotState next = update.reported_state.value_or(record->snapshot.state);
    if (!domain::is_transition_allowed(record->snapshot.state, next)) {
        return UpdateStatus::failure(UpdateRejection::invalid_transition);
    }

    // The mark only advances when the robot actually got somewhere. Advancing
    // it on every observation would make a robot pressed against a pallet look
    // like it had just moved, every hundred milliseconds, forever.
    if (has_progressed(policy_, record->progress.position, update.position)) {
        record->progress.position = update.position;
        record->progress.at = update.timestamp;
    }

    record->snapshot.state = next;
    record->snapshot.position = update.position;
    record->snapshot.velocity = update.velocity;
    record->snapshot.current_node = update.current_node;
    record->snapshot.current_edge = update.current_edge;
    record->snapshot.battery = update.battery;
    record->snapshot.observed_at = update.timestamp;

    if (update.last_ack_command_id.has_value()) {
        record->last_ack_command = update.last_ack_command_id;
    }

    // A robot that is no longer waiting is not waiting for anything. Leaving
    // the reason behind would have it show up in the next report of who is
    // held and why.
    if (next != domain::RobotState::waiting) {
        record->waiting.reset();
    }

    touch(*record, update.timestamp);
    return UpdateStatus::success();
}

core::Status<UpdateRejection> StateManager::assign_state(const core::RobotId& robot_id,
                                                         domain::RobotState state,
                                                         core::TimePoint at,
                                                         std::optional<WaitingContext> waiting) {
    RobotRecord* record = find(robot_id);
    if (record == nullptr) {
        return UpdateStatus::failure(UpdateRejection::unknown_robot);
    }
    if (!domain::is_transition_allowed(record->snapshot.state, state)) {
        return UpdateStatus::failure(UpdateRejection::invalid_transition);
    }

    // A hold with no reason cannot be explained afterwards, and a reason on a
    // robot that is not waiting is a leftover that will be read as current.
    // Both are refused rather than tidied up, because tidying loses the fact
    // that the caller had the wrong idea.
    const bool wants_waiting = state == domain::RobotState::waiting;
    if (wants_waiting != waiting.has_value()) {
        return UpdateStatus::failure(UpdateRejection::waiting_reason_mismatch);
    }

    record->snapshot.state = state;
    record->snapshot.observed_at = at;
    record->waiting = std::move(waiting);

    touch(*record, at);
    return UpdateStatus::success();
}

core::Status<UpdateRejection> StateManager::assign_route(const core::RobotId& robot_id,
                                                         std::optional<core::RouteId> route,
                                                         core::TimePoint at) {
    RobotRecord* record = find(robot_id);
    if (record == nullptr) {
        return UpdateStatus::failure(UpdateRejection::unknown_robot);
    }
    record->snapshot.current_route = std::move(route);
    touch(*record, at);
    return UpdateStatus::success();
}

core::Status<UpdateRejection> StateManager::assign_task(const core::RobotId& robot_id,
                                                        std::optional<core::TaskId> task,
                                                        core::TimePoint at) {
    RobotRecord* record = find(robot_id);
    if (record == nullptr) {
        return UpdateStatus::failure(UpdateRejection::unknown_robot);
    }
    record->snapshot.current_task = std::move(task);
    touch(*record, at);
    return UpdateStatus::success();
}

// --------------------------------------------------------------------- query

const RobotRecord* StateManager::record(const core::RobotId& robot_id) const {
    return find(robot_id);
}

const domain::RobotStateSnapshot* StateManager::snapshot(const core::RobotId& robot_id) const {
    const RobotRecord* found = find(robot_id);
    return found == nullptr ? nullptr : &found->snapshot;
}

std::vector<domain::RobotStateSnapshot> StateManager::snapshots() const {
    std::vector<domain::RobotStateSnapshot> result;
    result.reserve(robots_.size());
    for (const auto& [id, record] : robots_) {
        result.push_back(record.snapshot);
    }
    return result;
}

std::vector<core::RobotId> StateManager::robots_in_state(domain::RobotState state) const {
    std::vector<core::RobotId> result;
    for (const auto& [id, record] : robots_) {
        if (record.snapshot.state == state) {
            result.push_back(id);
        }
    }
    return result;
}

domain::StateVersion StateManager::version() const noexcept {
    return version_;
}

// ----------------------------------------------------------------- staleness

std::vector<core::RobotId> StateManager::stale_robots(core::TimePoint now,
                                                      core::Duration max_age) const {
    std::vector<core::RobotId> result;
    for (const auto& [id, record] : robots_) {
        if (domain::is_stale(record.snapshot, now, max_age)) {
            result.push_back(id);
        }
    }
    return result;
}

core::Duration StateManager::time_since_progress(const core::RobotId& robot_id,
                                                 core::TimePoint now) const {
    const RobotRecord* found = find(robot_id);
    if (found == nullptr) {
        return core::Duration::zero();
    }
    return stalled_for(found->progress, now);
}

// ------------------------------------------------------------------ internals

RobotRecord* StateManager::find(const core::RobotId& robot_id) {
    const auto found = robots_.find(robot_id);
    return found == robots_.end() ? nullptr : &found->second;
}

const RobotRecord* StateManager::find(const core::RobotId& robot_id) const {
    const auto found = robots_.find(robot_id);
    return found == robots_.end() ? nullptr : &found->second;
}

void StateManager::touch(RobotRecord& record, core::TimePoint at) {
    // One counter for the fleet, stamped onto whichever record changed.
    // docs/23_SYSTEM_ARCHITECTURE.md §2.4 asks "was this plan computed against
    // a world that has since moved", and that is a question about the fleet,
    // not about one robot.
    version_ = version_.next();
    record.snapshot.version = version_;
    record.controller_timestamp = at;
}

}  // namespace traffic::state
