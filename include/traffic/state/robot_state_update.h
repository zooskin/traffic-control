#pragma once

/// \file
/// What a robot tells us about itself. docs/04_ROBOT_TASK_MODEL.md §20, §22.
///
/// This is the system's boundary with the fleet, and the one rule that matters
/// here is that **a robot does not get to name its own traffic state**.
///
/// docs/00_INDEX.md D-001 fixes nine states, and three of them —
/// `reserving`, `waiting`, `replanning` — are decisions traffic control made,
/// not facts a robot can observe. A robot reporting `waiting` would be
/// asserting that we granted it something. Accepting that would let a
/// misbehaving or replayed message talk the controller into believing a
/// reservation exists that was never issued, which is precisely the failure
/// the reservation system exists to prevent. So updates carrying a
/// controller-owned state are refused rather than clamped: silently rewriting
/// them would hide a fleet that is sending nonsense.
///
/// The consistency fields of §22 are here for the same reason. A robot's
/// timestamp and the controller's are different clocks, and the last command
/// it acknowledged is how we tell "it has not started yet" from "it did not
/// hear us".

#include <optional>
#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/robot_state.h"
#include "traffic/domain/values.h"

namespace traffic::state {

/// True when \p state is one a robot can legitimately report about itself.
///
/// False for `reserving`, `waiting` and `replanning`: those are things traffic
/// control did to the robot, and only traffic control may set them.
[[nodiscard]] bool is_robot_reportable(domain::RobotState state) noexcept;

/// One telemetry message from a robot. docs/04_ROBOT_TASK_MODEL.md §20.
struct RobotStateUpdate {
    core::RobotId robot_id;

    /// When the robot took the measurement, on the traffic timeline.
    ///
    /// §22's `robot_timestamp`. Translating a robot's own clock onto this
    /// timeline is the adapter's job (Phase 14); by the time an update reaches
    /// here it is already in our units.
    core::TimePoint timestamp{core::kTimeOrigin};

    domain::Position position;
    domain::Velocity velocity;

    /// Radians. docs/04_ROBOT_TASK_MODEL.md §3's theta. Empty when the robot
    /// does not report one.
    std::optional<double> heading;

    /// The node the robot is standing at.
    std::optional<core::NodeId> current_node;

    /// The edge it is part-way along.
    std::optional<core::EdgeId> current_edge;

    /// Percent, 0-100.
    double battery{0.0};

    /// What the robot says its state is. Empty for a pure telemetry message,
    /// which is the common case — position and battery change constantly, the
    /// state rarely does.
    std::optional<domain::RobotState> reported_state;

    /// The last command the robot confirms it has received. §22.
    std::optional<core::CommandId> last_ack_command_id;

    [[nodiscard]] friend bool operator==(const RobotStateUpdate&,
                                         const RobotStateUpdate&) = default;
};

/// Why an update was not applied.
enum class UpdateRejection {
    /// The message does not say which robot it is about.
    empty_robot_id,

    /// No robot with that id is registered. An update for a robot the fleet
    /// register has never heard of is not a state change, it is a
    /// configuration problem.
    unknown_robot,

    /// The update is older than what is already held.
    ///
    /// Applying it would move the fleet's picture backwards, and a decision
    /// taken on it would be based on a world that has already changed.
    stale_timestamp,

    /// The robot reported a state only traffic control may assign.
    not_robot_reportable,

    /// The state machine refuses the move.
    /// docs/24_DOMAIN_MODEL.md §5.
    invalid_transition,

    /// Battery outside 0-100.
    battery_out_of_range,

    /// The robot claims to be at a node *and* part-way along an edge.
    ///
    /// It is one or the other. Accepting both makes the robot's resource
    /// occupancy ambiguous, and an ambiguous occupancy is how two robots end
    /// up granted the same corridor.
    at_a_node_and_on_an_edge,
};

[[nodiscard]] std::string_view to_string(UpdateRejection rejection) noexcept;

/// Checks what can be checked without knowing the fleet.
///
/// Shape only: an empty id, an impossible battery, a contradictory position.
/// Whether the robot exists, whether the timestamp moves forwards and whether
/// the transition is legal all need the state the manager holds, so they are
/// checked there.
[[nodiscard]] core::Status<UpdateRejection> validate(const RobotStateUpdate& update);

}  // namespace traffic::state
