#include "traffic/domain/task_status.h"

#include <array>
#include <cstddef>

namespace traffic::domain {
namespace {

constexpr std::array<TaskStatus, kTaskStatusCount> kAllStatuses{
    TaskStatus::created,
    TaskStatus::assigned,
    TaskStatus::planning,
    TaskStatus::running,
    TaskStatus::waiting,
    TaskStatus::completed,
    TaskStatus::cancelled,
    TaskStatus::failed,
};

[[nodiscard]] constexpr std::size_t index_of(TaskStatus status) noexcept {
    return static_cast<std::size_t>(status);
}

/// Rows are the current status, columns the requested one, in declaration
/// order.
///
///   * Any live status may be cancelled or fail. Both are outcomes the task
///     system or the fleet can impose at any point.
///   * `running` and `waiting` alternate freely: traffic control holds a robot
///     and releases it repeatedly over one task.
///   * Only `running` reaches `completed`. A task cannot finish from
///     `planning` — nothing has moved yet.
///   * The three terminal rows are false everywhere except their diagonal.
///     This is the invariant of docs/24_DOMAIN_MODEL.md §30.
constexpr std::array<std::array<bool, kTaskStatusCount>, kTaskStatusCount> kTransitions{{
    // to:      created assigned planning running waiting completed cancelled failed
    /* created   */ {{true, true, false, false, false, false, true, true}},
    /* assigned  */ {{false, true, true, false, false, false, true, true}},
    /* planning  */ {{false, true, true, true, true, false, true, true}},
    /* running   */ {{false, false, true, true, true, true, true, true}},
    /* waiting   */ {{false, false, true, true, true, false, true, true}},
    /* completed */ {{false, false, false, false, false, true, false, false}},
    /* cancelled */ {{false, false, false, false, false, false, true, false}},
    /* failed    */ {{false, false, false, false, false, false, false, true}},
}};

static_assert(kAllStatuses.size() == kTaskStatusCount);
static_assert(index_of(TaskStatus::created) == 0);
static_assert(index_of(TaskStatus::assigned) == 1);
static_assert(index_of(TaskStatus::planning) == 2);
static_assert(index_of(TaskStatus::running) == 3);
static_assert(index_of(TaskStatus::waiting) == 4);
static_assert(index_of(TaskStatus::completed) == 5);
static_assert(index_of(TaskStatus::cancelled) == 6);
static_assert(index_of(TaskStatus::failed) == 7);

}  // namespace

std::span<const TaskStatus> task_statuses() noexcept {
    return kAllStatuses;
}

bool is_transition_allowed(TaskStatus from, TaskStatus to) noexcept {
    return kTransitions[index_of(from)][index_of(to)];
}

bool is_terminal(TaskStatus status) noexcept {
    switch (status) {
        case TaskStatus::completed:
        case TaskStatus::cancelled:
        case TaskStatus::failed:
            return true;
        case TaskStatus::created:
        case TaskStatus::assigned:
        case TaskStatus::planning:
        case TaskStatus::running:
        case TaskStatus::waiting:
            return false;
    }
    return false;
}

bool is_active(TaskStatus status) noexcept {
    switch (status) {
        case TaskStatus::assigned:
        case TaskStatus::planning:
        case TaskStatus::running:
        case TaskStatus::waiting:
            return true;
        case TaskStatus::created:
        case TaskStatus::completed:
        case TaskStatus::cancelled:
        case TaskStatus::failed:
            return false;
    }
    return false;
}

std::string_view to_string(TaskStatus status) noexcept {
    switch (status) {
        case TaskStatus::created:
            return "CREATED";
        case TaskStatus::assigned:
            return "ASSIGNED";
        case TaskStatus::planning:
            return "PLANNING";
        case TaskStatus::running:
            return "RUNNING";
        case TaskStatus::waiting:
            return "WAITING";
        case TaskStatus::completed:
            return "COMPLETED";
        case TaskStatus::cancelled:
            return "CANCELLED";
        case TaskStatus::failed:
            return "FAILED";
    }
    return "CREATED";
}

std::optional<TaskStatus> task_status_from_string(std::string_view name) noexcept {
    for (const TaskStatus status : kAllStatuses) {
        if (to_string(status) == name) {
            return status;
        }
    }
    return std::nullopt;
}

}  // namespace traffic::domain
