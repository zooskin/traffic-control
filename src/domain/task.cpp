#include "traffic/domain/task.h"

#include <utility>

namespace traffic::domain {
namespace {

using TaskResult = core::Result<Task, DomainError>;

}  // namespace

bool is_overdue(const Task& task, core::TimePoint now) noexcept {
    return task.deadline.has_value() && now > *task.deadline;
}

core::Result<Task, DomainError> with_status(const Task& task, TaskStatus next, core::TimePoint at) {
    if (!is_transition_allowed(task.status, next)) {
        return TaskResult::failure(DomainError::invalid_transition);
    }

    Task updated = task;
    updated.status = next;

    // Stamp the timestamps here rather than at the call sites, so a status and
    // its timestamp cannot disagree about what happened.
    if (next == TaskStatus::running && !updated.started_at.has_value()) {
        updated.started_at = at;
    }
    if (is_terminal(next) && !updated.completed_at.has_value()) {
        updated.completed_at = at;
    }

    return TaskResult::success(std::move(updated));
}

core::Result<Task, DomainError> assign_to(const Task& task,
                                          core::RobotId robot_id,
                                          core::TimePoint at) {
    if (robot_id.empty()) {
        return TaskResult::failure(DomainError::empty_id);
    }

    auto assigned = with_status(task, TaskStatus::assigned, at);
    if (!assigned.has_value()) {
        return assigned;
    }

    Task updated = std::move(assigned).value();
    updated.robot_id = std::move(robot_id);
    return TaskResult::success(std::move(updated));
}

core::Result<Task, DomainError> make_task(core::TaskId id,
                                          core::NodeId source,
                                          core::NodeId destination,
                                          Priority priority,
                                          core::TimePoint created_at) {
    if (id.empty() || source.empty() || destination.empty()) {
        return TaskResult::failure(DomainError::empty_id);
    }
    if (source == destination) {
        return TaskResult::failure(DomainError::same_source_and_destination);
    }

    Task task;
    task.id = std::move(id);
    task.source = std::move(source);
    task.destination = std::move(destination);
    task.priority = priority;
    task.created_at = created_at;
    return TaskResult::success(std::move(task));
}

}  // namespace traffic::domain
