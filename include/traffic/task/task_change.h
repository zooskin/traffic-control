#pragma once

/// \file
/// A task moved. docs/04_ROBOT_TASK_MODEL.md §19, §24 ("Event generation"),
/// docs/24_DOMAIN_MODEL.md §18.
///
/// §19 defines the pattern for the robot side — a state change produces a
/// record naming what moved, from where, to where — and §24 asks for event
/// generation as an acceptance criterion without restricting it to robots.
/// This is that record for tasks.
///
/// It is **returned, not published**, for the reason `state::RobotStateChange`
/// gives: a manager that published its own events would need a listener
/// registry, and the order those listeners ran in would quietly become part of
/// the system's behaviour. The caller decides whether this becomes a
/// `domain::TrafficEvent`, a log line, or nothing.
///
/// It carries the robot as well as the status because a task can change hands
/// without changing status — docs/04_ROBOT_TASK_MODEL.md §16 leaves an
/// assignment engine free to move a task from one robot to another before it
/// starts running. A record that only reported status would show that
/// reassignment as no change at all, and the robot that was stood down would
/// have no trace of why.

#include <optional>
#include <string>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/task_status.h"
#include "traffic/domain/traffic_event.h"
#include "traffic/task/task_version.h"

namespace traffic::task {

/// One accepted change to a task.
struct TaskChange {
    core::TaskId task_id;

    domain::TaskStatus from{domain::TaskStatus::created};
    domain::TaskStatus to{domain::TaskStatus::created};

    /// Who held the task before and after. Empty before assignment.
    std::optional<core::RobotId> from_robot;
    std::optional<core::RobotId> to_robot;

    /// When the change was decided.
    core::TimePoint at{core::kTimeOrigin};

    /// The task-set version this change produced, so a later result can be
    /// checked against it. See task_version.h.
    TaskVersion version;

    [[nodiscard]] friend bool operator==(const TaskChange&, const TaskChange&) = default;
};

/// True when the status moved.
[[nodiscard]] bool is_status_change(const TaskChange& change) noexcept;

/// True when the task changed hands, including the first assignment.
[[nodiscard]] bool is_reassignment(const TaskChange& change) noexcept;

/// True when the task reached a state it will never leave.
///
/// The signal a throughput metric counts on: `completed`, `cancelled` and
/// `failed` all release the robot, and only one of them is success.
[[nodiscard]] bool is_task_finished(const TaskChange& change) noexcept;

/// The traffic event this change should be published as, when there is one.
///
/// docs/24_DOMAIN_MODEL.md §18 defines TASK_COMPLETED and TASK_CANCELLED and
/// no others for a moving task, so everything else — assignment, planning,
/// starting, holding — maps to nothing. That is deliberate: those are steps
/// the controller took itself and does not need to be told about.
///
/// `failed` also returns nothing, because §18 has no TASK_FAILED. Adding one
/// is a domain change and needs a documented decision first, so this reports
/// the gap by returning empty rather than borrowing ROBOT_FAILED, which means
/// something different — the robot is out of service, not the work.
[[nodiscard]] std::optional<domain::TrafficEventType> to_event_type(
    const TaskChange& change) noexcept;

/// The change as one line, fields in a fixed order.
///
/// Fixed order because two runs of a scenario are compared line by line to
/// show the run replayed identically.
[[nodiscard]] std::string to_log_line(const TaskChange& change);

}  // namespace traffic::task
