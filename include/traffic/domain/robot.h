#pragma once

/// \file
/// Robot identity and observed state.
///
/// The split between the two types here is decision D-005 in docs/00_INDEX.md.
/// docs/24_DOMAIN_MODEL.md §3 and §4 both list state, current_task and
/// current_route; holding them in two places guarantees the two will disagree,
/// and docs/23_SYSTEM_ARCHITECTURE.md §22 assigns the live state to
/// StateManager. So:
///
///   Robot               identity and fixed capabilities — the fleet register
///   RobotStateSnapshot  what was last observed, and when
///
/// The snapshot is a value: it is copied, compared and superseded, never
/// mutated in place. That is what makes "the decision was made against state
/// version 104" a statement you can check afterwards.

#include <cstdint>
#include <optional>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/robot_state.h"
#include "traffic/domain/values.h"

namespace traffic::domain {

/// What a robot is physically able to do.
///
/// Used to decide which tasks it can be given and which edges it fits through.
struct RobotCapabilities {
    /// Metres. An edge narrower than this cannot carry the robot.
    double width{0.0};

    /// Metres.
    double length{0.0};

    /// Metres per second. The robot's own ceiling, independent of edge limits.
    double max_speed{0.0};

    /// Kilograms.
    double payload_capacity{0.0};

    [[nodiscard]] friend bool operator==(const RobotCapabilities&,
                                         const RobotCapabilities&) = default;
};

/// A member of the fleet.
///
/// Identity and fixed attributes only. Everything that changes as the robot
/// drives lives in RobotStateSnapshot.
struct Robot {
    core::RobotId id;
    RobotCapabilities capabilities;

    [[nodiscard]] friend bool operator==(const Robot& lhs, const Robot& rhs) noexcept {
        // docs/24_DOMAIN_MODEL.md §33: entities compare by identity.
        return lhs.id == rhs.id;
    }
};

/// What was last observed about a robot. docs/24_DOMAIN_MODEL.md §4.
///
/// `observed_at` and `version` are not decoration. A traffic decision taken on
/// a stale snapshot is how two robots get sent into the same corridor, so both
/// the age and the version have to travel with the data
/// (docs/23_SYSTEM_ARCHITECTURE.md §2.4).
struct RobotStateSnapshot {
    core::RobotId robot_id;

    RobotState state{RobotState::unknown};

    Position position;
    Velocity velocity;

    /// The node the robot is at, when it is at one rather than between two.
    std::optional<core::NodeId> current_node;

    /// The edge the robot is traversing, when it is between nodes.
    std::optional<core::EdgeId> current_edge;

    std::optional<core::TaskId> current_task;
    std::optional<core::RouteId> current_route;

    /// Percent, 0-100.
    double battery{0.0};

    /// When this observation was taken, on the traffic timeline.
    core::TimePoint observed_at{core::kTimeOrigin};

    /// Increases with every accepted update.
    StateVersion version;

    [[nodiscard]] friend bool operator==(const RobotStateSnapshot&,
                                         const RobotStateSnapshot&) = default;
};

/// True when the observation is older than \p max_age at time \p now.
///
/// docs/23_SYSTEM_ARCHITECTURE.md §5 requires stale state to be detectable;
/// §23 lists "robot state timeout" as a failure with its own recovery policy.
[[nodiscard]] bool is_stale(const RobotStateSnapshot& snapshot,
                            core::TimePoint now,
                            core::Duration max_age) noexcept;

/// The result of asking a snapshot to move to a new state.
///
/// Returns the updated snapshot, or `invalid_transition` when the state machine
/// refuses. The original is left untouched either way.
[[nodiscard]] core::Result<RobotStateSnapshot, DomainError> with_state(
    const RobotStateSnapshot& snapshot, RobotState next, core::TimePoint observed_at);

/// Builds a Robot, rejecting an empty id.
[[nodiscard]] core::Result<Robot, DomainError> make_robot(core::RobotId id,
                                                          RobotCapabilities capabilities);

/// Builds the first snapshot for a robot, in the state it starts in.
[[nodiscard]] core::Result<RobotStateSnapshot, DomainError> make_robot_state_snapshot(
    core::RobotId robot_id, RobotState state, core::TimePoint observed_at);

}  // namespace traffic::domain
