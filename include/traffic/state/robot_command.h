#pragma once

/// \file
/// What traffic control tells a robot to do.
/// docs/04_ROBOT_TASK_MODEL.md §23.
///
/// Six actions, and a deliberate absence. §23 ends with "Emergency Stop은 일반
/// Traffic Command와 분리한다", and CLAUDE.md draws the same line harder:
/// Emergency Stop, Protective Stop, Safety Zones and Safety Interlocks belong
/// to a separate system. There is no emergency-stop action here and there will
/// not be one. A traffic controller that could issue one would be a traffic
/// controller people started relying on for safety, and it is not built to that
/// standard — its decisions depend on state that can be stale, on a planner
/// that can time out, and on a fleet that can fall silent.
///
/// A command is a value: built, validated, and then immutable. Sending it is
/// the adapter's job (Phase 14), and recording that we sent it is the state
/// manager's (§22). This file only says what a well-formed one looks like.

#include <optional>
#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"

namespace traffic::state {

/// docs/04_ROBOT_TASK_MODEL.md §23.
enum class CommandAction {
    /// Drive the route it has been given.
    move,

    /// Hold where you are. Traffic control's own decision, and the counterpart
    /// of `RobotState::waiting`.
    wait,

    /// Come to a controlled stop. Not an emergency stop — this is a traffic
    /// instruction, obeyed at the robot's normal deceleration.
    stop,

    /// Carry on after a `wait` or a `stop`.
    resume,

    /// Abandon the current route and take this one instead.
    reroute,

    /// Pull into a waiting bay so someone else can pass.
    ///
    /// docs/03_MAP_GRAPH.md §13: in a single-lane corridor with no alternative
    /// route this is often the only way out of a head-on deadlock, which is
    /// why it is an action of its own and not a `move` that happens to end at
    /// a bay.
    go_to_waiting_bay,
};

[[nodiscard]] std::string_view to_string(CommandAction action) noexcept;
[[nodiscard]] std::optional<CommandAction> command_action_from_string(
    std::string_view name) noexcept;

/// True when the action starts or resumes motion.
///
/// The distinction the reservation manager cares about: these are the commands
/// that must not be sent before the resources ahead are held.
[[nodiscard]] bool sets_a_robot_moving(CommandAction action) noexcept;

/// One instruction to one robot. docs/04_ROBOT_TASK_MODEL.md §23.
struct RobotCommand {
    core::CommandId id;
    core::RobotId robot_id;

    CommandAction action{CommandAction::stop};

    /// The route to drive. Required by `move` and `reroute`.
    std::optional<core::RouteId> route;

    /// Where to end up. Required by `go_to_waiting_bay`.
    std::optional<core::NodeId> target_node;

    core::TimePoint created_at{core::kTimeOrigin};

    /// Ties this command back to the event that caused it.
    /// docs/23_SYSTEM_ARCHITECTURE.md §24.
    core::CorrelationId correlation_id;

    [[nodiscard]] friend bool operator==(const RobotCommand& lhs,
                                         const RobotCommand& rhs) noexcept {
        // docs/24_DOMAIN_MODEL.md §33: entities compare by identity.
        return lhs.id == rhs.id;
    }
};

/// Builds a command, refusing one that cannot be carried out.
///
/// Rejects empty ids, a `move` or `reroute` with no route, and a
/// `go_to_waiting_bay` with nowhere to go. A robot receiving one of those has
/// no way to comply and no way to say so usefully; the failure belongs here,
/// where the caller still has the context to fix it.
[[nodiscard]] core::Result<RobotCommand, domain::DomainError> make_robot_command(
    core::CommandId id,
    core::RobotId robot_id,
    CommandAction action,
    std::optional<core::RouteId> route,
    std::optional<core::NodeId> target_node,
    core::TimePoint created_at);

}  // namespace traffic::state
