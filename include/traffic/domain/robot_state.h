#pragma once

/// \file
/// Robot state and its transition rules.
///
/// The state set is fixed by decision D-001 in docs/00_INDEX.md: the seven
/// states of docs/24_DOMAIN_MODEL.md §4 plus `reserving` and `replanning`.
///
/// The distinction that matters most here is `temporarily_stopped` against
/// `blocked` and `failed`. A person walking past stops a robot several times an
/// hour. Treating that as a failure triggers replanning across the fleet for
/// something that resolves itself in seconds — docs/23_SYSTEM_ARCHITECTURE.md
/// §20 and docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §17~18.
///
/// docs/20_CODING_GUIDELINES.md §37 and docs/24_DOMAIN_MODEL.md §5 require
/// transitions to be explicit and invalid ones to be rejected rather than
/// silently applied.

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace traffic::domain {

/// Where a robot is in its traffic lifecycle.
enum class RobotState {
    /// No task. Available for assignment.
    idle,

    /// Holds a task and is acquiring the reservations its next move needs.
    reserving,

    /// Moving along its route.
    moving,

    /// Stopped by traffic control, waiting for a resource it asked for.
    /// This is a decision of ours, not an external event.
    waiting,

    /// Stopped by something outside traffic control — a person, an obstacle —
    /// and expected to resume shortly. Not a failure.
    temporarily_stopped,

    /// Cannot proceed on its current route and needs a new one. A stop that
    /// outlived its timeout arrives here.
    blocked,

    /// A new route is being computed for it.
    replanning,

    /// Out of service. Its resources must be released and traffic routed
    /// around it — docs/22_IMPLEMENTATION_WORKFLOW.md Phase 14.
    failed,

    /// No fresh observation. Reached on communication loss; the robot may be
    /// doing anything, so nothing may be assumed about it.
    unknown,
};

/// Number of enumerators, for table sizing and iteration in tests.
inline constexpr std::size_t kRobotStateCount = 9;

/// All states, in declaration order.
[[nodiscard]] std::span<const RobotState> robot_states() noexcept;

/// True when \p from -> \p to is a legal move.
///
/// Self-transitions are allowed: state updates arrive continuously and most
/// report no change, so re-reporting the current state must not be an error.
[[nodiscard]] bool is_transition_allowed(RobotState from, RobotState to) noexcept;

/// True when the robot is not making progress along its route.
///
/// Groups the four stalled states so callers stop rewriting the same
/// disjunction, each time slightly differently.
[[nodiscard]] bool is_halted(RobotState state) noexcept;

/// True when traffic control can still act on this robot.
///
/// `failed` and `unknown` are excluded: neither will respond to a command, and
/// issuing one anyway is how a phantom reservation gets created.
[[nodiscard]] bool is_controllable(RobotState state) noexcept;

/// Wire and log name, in the upper-case form used by
/// docs/24_DOMAIN_MODEL.md §4.
[[nodiscard]] std::string_view to_string(RobotState state) noexcept;

/// Parses the name produced by `to_string`. Empty when unrecognised.
[[nodiscard]] std::optional<RobotState> robot_state_from_string(std::string_view name) noexcept;

}  // namespace traffic::domain
