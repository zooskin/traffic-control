#include "traffic/task/task_change.h"

#include <optional>
#include <sstream>
#include <string>

#include "traffic/domain/task_status.h"
#include "traffic/domain/traffic_event.h"

namespace traffic::task {

bool is_status_change(const TaskChange& change) noexcept {
    return change.from != change.to;
}

bool is_reassignment(const TaskChange& change) noexcept {
    return change.from_robot != change.to_robot;
}

bool is_task_finished(const TaskChange& change) noexcept {
    // A self-transition does not count. A task reported completed twice has
    // finished once, and counting the second would inflate throughput — the
    // exact failure docs/24_DOMAIN_MODEL.md §30 exists to prevent.
    return change.from != change.to && domain::is_terminal(change.to);
}

std::optional<domain::TrafficEventType> to_event_type(const TaskChange& change) noexcept {
    if (change.from == change.to) {
        return std::nullopt;
    }

    switch (change.to) {
        case domain::TaskStatus::completed:
            return domain::TrafficEventType::task_completed;
        case domain::TaskStatus::cancelled:
            return domain::TrafficEventType::task_cancelled;
        case domain::TaskStatus::created:
        case domain::TaskStatus::assigned:
        case domain::TaskStatus::planning:
        case domain::TaskStatus::running:
        case domain::TaskStatus::waiting:
        case domain::TaskStatus::failed:
            // docs/24_DOMAIN_MODEL.md §18 defines no event for these. `failed`
            // is the one worth noticing: the gap is reported by returning
            // nothing rather than papered over with ROBOT_FAILED, which says
            // something else entirely — the robot is out of service, not the
            // work.
            return std::nullopt;
    }
    return std::nullopt;
}

std::string to_log_line(const TaskChange& change) {
    std::ostringstream line;
    line << "task=" << change.task_id.value() << " from=" << domain::to_string(change.from)
         << " to=" << domain::to_string(change.to)
         << " robot=" << (change.to_robot.has_value() ? change.to_robot->value() : "-")
         << " version=" << change.version;
    return line.str();
}

}  // namespace traffic::task
