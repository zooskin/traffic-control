#pragma once

/// \file
/// Task entity. docs/24_DOMAIN_MODEL.md §6.
///
/// A task is what the warehouse asked for: move from here to there, by this
/// time. It is deliberately free of route and traffic detail — the external
/// API says "assign task T to robot R", never "move R to node C02", and
/// docs/13_API_SPECIFICATION.md §19 makes that a design rule.

#include <optional>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/task_status.h"
#include "traffic/domain/values.h"

namespace traffic::domain {

struct Task {
    core::TaskId id;

    /// Where the work starts.
    core::NodeId source;

    /// Where it ends.
    core::NodeId destination;

    /// Set once the task is assigned.
    std::optional<core::RobotId> robot_id;

    TaskStatus status{TaskStatus::created};

    /// Base priority as given by the task system. The effective priority used
    /// to resolve a conflict is computed from this plus waiting time and
    /// urgency — docs/24_DOMAIN_MODEL.md §17, Phase 8.
    Priority priority;

    core::TimePoint created_at{core::kTimeOrigin};
    std::optional<core::TimePoint> started_at;
    std::optional<core::TimePoint> completed_at;

    /// When the task stops being useful. Empty means no deadline.
    std::optional<core::TimePoint> deadline;

    [[nodiscard]] friend bool operator==(const Task& lhs, const Task& rhs) noexcept {
        // docs/24_DOMAIN_MODEL.md §33: entities compare by identity.
        return lhs.id == rhs.id;
    }
};

/// True when the task has a deadline that has passed at \p now.
[[nodiscard]] bool is_overdue(const Task& task, core::TimePoint now) noexcept;

/// Moves a task to \p next, or reports `invalid_transition`.
///
/// Stamps `started_at` on the first move to `running` and `completed_at` on
/// reaching a terminal state, so the timestamps cannot drift from the status
/// they describe. The original task is left untouched.
[[nodiscard]] core::Result<Task, DomainError> with_status(const Task& task,
                                                          TaskStatus next,
                                                          core::TimePoint at);

/// Assigns a task to a robot and moves it to `assigned`.
[[nodiscard]] core::Result<Task, DomainError> assign_to(const Task& task,
                                                        core::RobotId robot_id,
                                                        core::TimePoint at);

/// Builds a Task, checking the invariants of docs/24_DOMAIN_MODEL.md §34.
///
/// Rejects an empty id, an empty endpoint, and source == destination — a task
/// that asks a robot to travel nowhere would occupy it and complete instantly,
/// inflating throughput while moving nothing.
[[nodiscard]] core::Result<Task, DomainError> make_task(core::TaskId id,
                                                        core::NodeId source,
                                                        core::NodeId destination,
                                                        Priority priority,
                                                        core::TimePoint created_at);

}  // namespace traffic::domain
