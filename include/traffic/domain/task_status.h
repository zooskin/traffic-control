#pragma once

/// \file
/// Task lifecycle.
///
/// States are those of docs/24_DOMAIN_MODEL.md §6.
///
/// docs/24_DOMAIN_MODEL.md §30 states the invariant this file exists to
/// enforce: a completed task never returns to RUNNING. Terminal is terminal —
/// re-running a finished task would double-count throughput and, worse, send a
/// robot to a destination whose work is already done.

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace traffic::domain {

enum class TaskStatus {
    /// Accepted from the task system, not yet assigned to a robot.
    created,

    /// Assigned to a robot, which has not begun planning.
    assigned,

    /// A route is being computed.
    planning,

    /// The robot is executing it.
    running,

    /// Execution is interrupted but the task is still live — the robot is held
    /// by traffic control or blocked.
    waiting,

    /// Finished successfully. Terminal.
    completed,

    /// Withdrawn before completion. Terminal.
    cancelled,

    /// Could not be completed. Terminal.
    failed,
};

inline constexpr std::size_t kTaskStatusCount = 8;

[[nodiscard]] std::span<const TaskStatus> task_statuses() noexcept;

/// True when \p from -> \p to is a legal move.
///
/// Self-transitions are allowed so that a repeated report is not an error.
[[nodiscard]] bool is_transition_allowed(TaskStatus from, TaskStatus to) noexcept;

/// True when the task will never change state again.
[[nodiscard]] bool is_terminal(TaskStatus status) noexcept;

/// True when the task still occupies a robot.
[[nodiscard]] bool is_active(TaskStatus status) noexcept;

[[nodiscard]] std::string_view to_string(TaskStatus status) noexcept;
[[nodiscard]] std::optional<TaskStatus> task_status_from_string(std::string_view name) noexcept;

}  // namespace traffic::domain
