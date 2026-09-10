#include "traffic/domain/robot.h"

#include <utility>

namespace traffic::domain {
namespace {

using RobotResult = core::Result<Robot, DomainError>;
using SnapshotResult = core::Result<RobotStateSnapshot, DomainError>;

}  // namespace

bool is_stale(const RobotStateSnapshot& snapshot,
              core::TimePoint now,
              core::Duration max_age) noexcept {
    if (now <= snapshot.observed_at) {
        // A snapshot from the future is not stale. It happens when a robot's
        // clock runs ahead; treating it as stale would drop good data.
        return false;
    }
    return (now - snapshot.observed_at) > max_age;
}

core::Result<RobotStateSnapshot, DomainError> with_state(const RobotStateSnapshot& snapshot,
                                                         RobotState next,
                                                         core::TimePoint observed_at) {
    if (!is_transition_allowed(snapshot.state, next)) {
        return SnapshotResult::failure(DomainError::invalid_transition);
    }

    RobotStateSnapshot updated = snapshot;
    updated.state = next;
    updated.observed_at = observed_at;
    updated.version = snapshot.version.next();
    return SnapshotResult::success(std::move(updated));
}

core::Result<Robot, DomainError> make_robot(core::RobotId id, RobotCapabilities capabilities) {
    if (id.empty()) {
        return RobotResult::failure(DomainError::empty_id);
    }
    if (capabilities.width < 0.0 || capabilities.length < 0.0 || capabilities.max_speed < 0.0 ||
        capabilities.payload_capacity < 0.0) {
        return RobotResult::failure(DomainError::negative_value);
    }

    Robot robot;
    robot.id = std::move(id);
    robot.capabilities = capabilities;
    return RobotResult::success(std::move(robot));
}

core::Result<RobotStateSnapshot, DomainError> make_robot_state_snapshot(
    core::RobotId robot_id, RobotState state, core::TimePoint observed_at) {
    if (robot_id.empty()) {
        return SnapshotResult::failure(DomainError::empty_id);
    }

    RobotStateSnapshot snapshot;
    snapshot.robot_id = std::move(robot_id);
    snapshot.state = state;
    snapshot.observed_at = observed_at;
    return SnapshotResult::success(std::move(snapshot));
}

}  // namespace traffic::domain
