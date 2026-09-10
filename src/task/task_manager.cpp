#include "traffic/task/task_manager.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/task.h"
#include "traffic/domain/task_status.h"
#include "traffic/domain/values.h"
#include "traffic/task/task_change.h"
#include "traffic/task/task_version.h"

namespace traffic::task {
namespace {

using TaskStatusResult = core::Status<TaskRejection>;

/// Checks a task offered to `create` against the invariants of
/// docs/24_DOMAIN_MODEL.md §34 and docs/04_ROBOT_TASK_MODEL.md §14~15.
///
/// Repeats what `domain::make_task` already checks, on purpose: `Task` is an
/// aggregate, so a caller can build one without ever calling the factory, and
/// the owner of the data is the last place able to refuse a broken one.
[[nodiscard]] TaskStatusResult validate_new(const domain::Task& task) {
    if (task.id.empty() || task.source.empty() || task.destination.empty()) {
        return TaskStatusResult::failure(TaskRejection::empty_task_id);
    }
    if (task.source == task.destination) {
        // A task that asks a robot to travel nowhere would occupy it and
        // complete instantly, inflating throughput while moving nothing.
        return TaskStatusResult::failure(TaskRejection::same_source_and_destination);
    }
    if (task.status != domain::TaskStatus::created || task.robot_id.has_value()) {
        return TaskStatusResult::failure(TaskRejection::not_a_new_task);
    }
    if (task.priority < kMinTaskPriority || task.priority > kMaxTaskPriority) {
        return TaskStatusResult::failure(TaskRejection::priority_out_of_range);
    }
    if (task.deadline.has_value()) {
        if (task.deadline.value() < task.created_at) {
            return TaskStatusResult::failure(TaskRejection::deadline_before_creation);
        }
    }
    return TaskStatusResult::success();
}

/// Builds the record of a change that has already been applied.
[[nodiscard]] TaskChange make_change(const TaskRecord& record,
                                     domain::TaskStatus previous_status,
                                     const std::optional<core::RobotId>& previous_robot,
                                     core::TimePoint at) {
    TaskChange change;
    change.task_id = record.task.id;
    change.from = previous_status;
    change.to = record.task.status;
    change.from_robot = previous_robot;
    change.to_robot = record.task.robot_id;
    change.at = at;
    change.version = record.version;
    return change;
}

}  // namespace

std::string_view to_string(TaskRejection rejection) noexcept {
    switch (rejection) {
        case TaskRejection::empty_task_id:
            return "EMPTY_TASK_ID";
        case TaskRejection::duplicate_task:
            return "DUPLICATE_TASK";
        case TaskRejection::unknown_task:
            return "UNKNOWN_TASK";
        case TaskRejection::not_a_new_task:
            return "NOT_A_NEW_TASK";
        case TaskRejection::same_source_and_destination:
            return "SAME_SOURCE_AND_DESTINATION";
        case TaskRejection::invalid_transition:
            return "INVALID_TRANSITION";
        case TaskRejection::empty_robot_id:
            return "EMPTY_ROBOT_ID";
        case TaskRejection::task_is_terminal:
            return "TASK_IS_TERMINAL";
        case TaskRejection::task_still_live:
            return "TASK_STILL_LIVE";
        case TaskRejection::priority_out_of_range:
            return "PRIORITY_OUT_OF_RANGE";
        case TaskRejection::deadline_before_creation:
            return "DEADLINE_BEFORE_CREATION";
    }
    return "UNKNOWN";
}

// ------------------------------------------------------------------- register

core::Status<TaskRejection> TaskManager::create(domain::Task task, core::TimePoint at) {
    if (auto shape = validate_new(task); !shape.has_value()) {
        return shape;
    }
    if (tasks_.contains(task.id)) {
        // Accepting it as an update would silently replace work that a robot
        // may already be carrying out.
        return TaskStatusResult::failure(TaskRejection::duplicate_task);
    }

    TaskRecord record;
    record.task = std::move(task);
    record.controller_timestamp = at;

    const core::TaskId key = record.task.id;
    touch(record, at);
    tasks_.emplace(key, std::move(record));

    return TaskStatusResult::success();
}

core::Status<TaskRejection> TaskManager::remove(const core::TaskId& task_id) {
    const TaskRecord* found = find(task_id);
    if (found == nullptr) {
        return TaskStatusResult::failure(TaskRejection::unknown_task);
    }
    if (!domain::is_terminal(found->task.status)) {
        return TaskStatusResult::failure(TaskRejection::task_still_live);
    }

    tasks_.erase(task_id);

    // The set of tasks is different from what a decision made a moment ago
    // assumed, so anything computed against the old version is stale.
    version_ = version_.next();
    return TaskStatusResult::success();
}

bool TaskManager::contains(const core::TaskId& task_id) const {
    return tasks_.contains(task_id);
}

std::size_t TaskManager::task_count() const noexcept {
    return tasks_.size();
}

// ------------------------------------------------------------------ lifecycle

TaskOutcome TaskManager::assign(const core::TaskId& task_id,
                                core::RobotId robot_id,
                                core::TimePoint at) {
    if (robot_id.empty()) {
        return TaskOutcome::failure(TaskRejection::empty_robot_id);
    }

    TaskRecord* record = find(task_id);
    if (record == nullptr) {
        return TaskOutcome::failure(TaskRejection::unknown_task);
    }

    const domain::TaskStatus previous_status = record->task.status;
    const std::optional<core::RobotId> previous_robot = record->task.robot_id;

    auto assigned = domain::assign_to(record->task, std::move(robot_id), at);
    if (!assigned.has_value()) {
        // The empty-id case is refused above, so the state machine is the only
        // thing left that can have said no.
        return TaskOutcome::failure(TaskRejection::invalid_transition);
    }
    record->task = std::move(assigned).value();

    if (record->task.status == previous_status && record->task.robot_id == previous_robot) {
        // Assigning a task to the robot that already holds it is a repeat, not
        // a change. Bumping the version for it would make every re-send look
        // like the task set had moved.
        return TaskOutcome::success(std::nullopt);
    }

    touch(*record, at);
    return TaskOutcome::success(make_change(*record, previous_status, previous_robot, at));
}

TaskOutcome TaskManager::advance(const core::TaskId& task_id,
                                 domain::TaskStatus next,
                                 core::TimePoint at) {
    TaskRecord* record = find(task_id);
    if (record == nullptr) {
        return TaskOutcome::failure(TaskRejection::unknown_task);
    }

    const domain::TaskStatus previous_status = record->task.status;
    const std::optional<core::RobotId> previous_robot = record->task.robot_id;

    // The transition table of docs/24_DOMAIN_MODEL.md §6 decides, and
    // `with_status` stamps `started_at` / `completed_at` so a timestamp cannot
    // disagree with the status it describes.
    auto moved = domain::with_status(record->task, next, at);
    if (!moved.has_value()) {
        return TaskOutcome::failure(TaskRejection::invalid_transition);
    }
    record->task = std::move(moved).value();

    if (record->task.status == previous_status) {
        // A repeated report of the status a task is already in. Accepted, so a
        // re-sent message is not an error, but it moved nothing.
        return TaskOutcome::success(std::nullopt);
    }

    touch(*record, at);
    return TaskOutcome::success(make_change(*record, previous_status, previous_robot, at));
}

// ----------------------------------------------------------------- attributes

core::Status<TaskRejection> TaskManager::set_priority(const core::TaskId& task_id,
                                                      domain::Priority priority,
                                                      core::TimePoint at) {
    TaskRecord* record = find(task_id);
    if (record == nullptr) {
        return TaskStatusResult::failure(TaskRejection::unknown_task);
    }
    if (domain::is_terminal(record->task.status)) {
        return TaskStatusResult::failure(TaskRejection::task_is_terminal);
    }
    if (priority < kMinTaskPriority || priority > kMaxTaskPriority) {
        return TaskStatusResult::failure(TaskRejection::priority_out_of_range);
    }
    if (record->task.priority == priority) {
        return TaskStatusResult::success();
    }

    record->task.priority = priority;
    touch(*record, at);
    return TaskStatusResult::success();
}

core::Status<TaskRejection> TaskManager::set_deadline(const core::TaskId& task_id,
                                                      std::optional<core::TimePoint> deadline,
                                                      core::TimePoint at) {
    TaskRecord* record = find(task_id);
    if (record == nullptr) {
        return TaskStatusResult::failure(TaskRejection::unknown_task);
    }
    if (domain::is_terminal(record->task.status)) {
        return TaskStatusResult::failure(TaskRejection::task_is_terminal);
    }
    if (deadline.has_value()) {
        if (deadline.value() < record->task.created_at) {
            return TaskStatusResult::failure(TaskRejection::deadline_before_creation);
        }
    }
    if (record->task.deadline == deadline) {
        return TaskStatusResult::success();
    }

    record->task.deadline = deadline;
    touch(*record, at);
    return TaskStatusResult::success();
}

// ---------------------------------------------------------------------- query

const TaskRecord* TaskManager::record(const core::TaskId& task_id) const {
    return find(task_id);
}

const domain::Task* TaskManager::task(const core::TaskId& task_id) const {
    const TaskRecord* found = find(task_id);
    return found == nullptr ? nullptr : &found->task;
}

std::vector<domain::Task> TaskManager::tasks() const {
    std::vector<domain::Task> result;
    result.reserve(tasks_.size());
    for (const auto& [id, record] : tasks_) {
        result.push_back(record.task);
    }
    return result;
}

std::vector<core::TaskId> TaskManager::tasks_with_status(domain::TaskStatus status) const {
    std::vector<core::TaskId> result;
    for (const auto& [id, record] : tasks_) {
        if (record.task.status == status) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<core::TaskId> TaskManager::tasks_for_robot(const core::RobotId& robot_id) const {
    std::vector<core::TaskId> result;
    for (const auto& [id, record] : tasks_) {
        if (record.task.robot_id == robot_id) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<core::TaskId> TaskManager::active_tasks_for_robot(const core::RobotId& robot_id) const {
    std::vector<core::TaskId> result;
    for (const auto& [id, record] : tasks_) {
        if (record.task.robot_id == robot_id && domain::is_active(record.task.status)) {
            result.push_back(id);
        }
    }
    return result;
}

TaskVersion TaskManager::version() const noexcept {
    return version_;
}

// ------------------------------------------------------------------- deadline

std::vector<core::TaskId> TaskManager::overdue_tasks(core::TimePoint now) const {
    std::vector<core::TaskId> result;
    for (const auto& [id, record] : tasks_) {
        if (domain::is_terminal(record.task.status)) {
            continue;
        }
        if (domain::is_overdue(record.task, now)) {
            result.push_back(id);
        }
    }
    return result;
}

// ------------------------------------------------------------------ internals

TaskRecord* TaskManager::find(const core::TaskId& task_id) {
    const auto found = tasks_.find(task_id);
    return found == tasks_.end() ? nullptr : &found->second;
}

const TaskRecord* TaskManager::find(const core::TaskId& task_id) const {
    const auto found = tasks_.find(task_id);
    return found == tasks_.end() ? nullptr : &found->second;
}

void TaskManager::touch(TaskRecord& record, core::TimePoint at) {
    // One counter for the whole task set, stamped onto whichever record
    // changed — the reasoning of task_version.h, and the same shape as
    // `state::StateManager::touch`.
    version_ = version_.next();
    record.version = version_;
    record.controller_timestamp = at;
}

// --------------------------------------------------------------------- naming

TaskOutcome start_planning(ITaskManager& tasks, const core::TaskId& task_id, core::TimePoint at) {
    return tasks.advance(task_id, domain::TaskStatus::planning, at);
}

TaskOutcome start(ITaskManager& tasks, const core::TaskId& task_id, core::TimePoint at) {
    return tasks.advance(task_id, domain::TaskStatus::running, at);
}

TaskOutcome hold(ITaskManager& tasks, const core::TaskId& task_id, core::TimePoint at) {
    return tasks.advance(task_id, domain::TaskStatus::waiting, at);
}

TaskOutcome complete(ITaskManager& tasks, const core::TaskId& task_id, core::TimePoint at) {
    return tasks.advance(task_id, domain::TaskStatus::completed, at);
}

TaskOutcome cancel(ITaskManager& tasks, const core::TaskId& task_id, core::TimePoint at) {
    return tasks.advance(task_id, domain::TaskStatus::cancelled, at);
}

TaskOutcome fail(ITaskManager& tasks, const core::TaskId& task_id, core::TimePoint at) {
    return tasks.advance(task_id, domain::TaskStatus::failed, at);
}

std::vector<core::TaskId> by_priority(const ITaskManager& tasks, domain::TaskStatus status) {
    std::vector<core::TaskId> ids = tasks.tasks_with_status(status);

    // Keyed by the priority read once, rather than sorting with a comparator
    // that looks each task up: a comparator that can fail a lookup is a
    // comparator that can stop being a strict weak ordering.
    std::vector<std::pair<domain::Priority, core::TaskId>> keyed;
    keyed.reserve(ids.size());
    for (core::TaskId& id : ids) {
        const domain::Task* found = tasks.task(id);
        const domain::Priority priority =
            found == nullptr ? domain::kLowestPriority : found->priority;
        keyed.emplace_back(priority, std::move(id));
    }

    std::sort(keyed.begin(), keyed.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.first != rhs.first) {
            return lhs.first > rhs.first;
        }
        return lhs.second < rhs.second;
    });

    std::vector<core::TaskId> result;
    result.reserve(keyed.size());
    for (auto& entry : keyed) {
        result.push_back(std::move(entry.second));
    }
    return result;
}

}  // namespace traffic::task
