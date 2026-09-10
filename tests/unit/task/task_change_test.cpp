/// Task change records. docs/04_ROBOT_TASK_MODEL.md §19, §24 ("Event
/// generation"), docs/24_DOMAIN_MODEL.md §18, §30.

#include "traffic/task/task_change.h"

#include <chrono>
#include <optional>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/task_status.h"
#include "traffic/domain/traffic_event.h"
#include "traffic/task/task_version.h"

namespace traffic::task {
namespace {

using core::RobotId;
using core::TaskId;
using domain::TaskStatus;
using domain::TrafficEventType;

core::TimePoint at(int second) {
    return core::kTimeOrigin + std::chrono::duration_cast<core::Duration>(core::Seconds{second});
}

TaskChange moved(TaskStatus from, TaskStatus to) {
    TaskChange change;
    change.task_id = TaskId{"T1"};
    change.from = from;
    change.to = to;
    change.from_robot = RobotId{"R01"};
    change.to_robot = RobotId{"R01"};
    change.at = at(5);
    change.version = TaskVersion{7};
    return change;
}

TEST(TaskChangeRecord, task_change_reports_a_status_move) {
    EXPECT_TRUE(is_status_change(moved(TaskStatus::planning, TaskStatus::running)));
    EXPECT_FALSE(is_status_change(moved(TaskStatus::running, TaskStatus::running)));
}

TEST(TaskChangeRecord, task_change_reports_a_handover_that_moved_no_status) {
    // docs/04_ROBOT_TASK_MODEL.md §16 leaves an engine free to move a task
    // between robots before it runs. A record that only carried the status
    // would show that as no change at all, and the robot that was stood down
    // would have no trace of why.
    TaskChange change = moved(TaskStatus::assigned, TaskStatus::assigned);
    change.to_robot = RobotId{"R02"};

    EXPECT_FALSE(is_status_change(change));
    EXPECT_TRUE(is_reassignment(change));
}

TEST(TaskChangeRecord, task_change_reports_the_first_assignment_as_a_handover) {
    TaskChange change = moved(TaskStatus::created, TaskStatus::assigned);
    change.from_robot = std::nullopt;

    EXPECT_TRUE(is_reassignment(change));
}

TEST(TaskChangeRecord, task_change_finished_needs_a_real_move) {
    // docs/24_DOMAIN_MODEL.md §30. A task reported completed twice finished
    // once; counting the second would inflate throughput.
    EXPECT_TRUE(is_task_finished(moved(TaskStatus::running, TaskStatus::completed)));
    EXPECT_TRUE(is_task_finished(moved(TaskStatus::running, TaskStatus::failed)));
    EXPECT_TRUE(is_task_finished(moved(TaskStatus::created, TaskStatus::cancelled)));
    EXPECT_FALSE(is_task_finished(moved(TaskStatus::completed, TaskStatus::completed)));
    EXPECT_FALSE(is_task_finished(moved(TaskStatus::planning, TaskStatus::running)));
}

TEST(TaskChangeRecord, task_change_maps_an_ending_to_the_documented_event) {
    // docs/24_DOMAIN_MODEL.md §18 defines TASK_COMPLETED and TASK_CANCELLED.
    EXPECT_EQ(to_event_type(moved(TaskStatus::running, TaskStatus::completed)),
              TrafficEventType::task_completed);
    EXPECT_EQ(to_event_type(moved(TaskStatus::assigned, TaskStatus::cancelled)),
              TrafficEventType::task_cancelled);
}

TEST(TaskChangeRecord, task_change_has_no_event_for_a_failed_task) {
    // Deliberate. docs/24_DOMAIN_MODEL.md §18 has no TASK_FAILED, and
    // borrowing ROBOT_FAILED would say something else — the robot is out of
    // service, not the work. Adding the event is a domain change and needs a
    // documented decision first, so the gap is reported rather than papered
    // over.
    EXPECT_FALSE(to_event_type(moved(TaskStatus::running, TaskStatus::failed)).has_value());
}

TEST(TaskChangeRecord, task_change_has_no_event_for_an_intermediate_step) {
    // Assignment, planning, starting and holding are steps the controller took
    // itself and does not need to be told about.
    EXPECT_FALSE(to_event_type(moved(TaskStatus::created, TaskStatus::assigned)).has_value());
    EXPECT_FALSE(to_event_type(moved(TaskStatus::planning, TaskStatus::running)).has_value());
    EXPECT_FALSE(to_event_type(moved(TaskStatus::running, TaskStatus::waiting)).has_value());
    EXPECT_FALSE(to_event_type(moved(TaskStatus::completed, TaskStatus::completed)).has_value());
}

TEST(TaskChangeRecord, task_change_log_line_keeps_a_fixed_field_order) {
    // Fixed order because two runs of a scenario are compared line by line to
    // show the run replayed identically.
    EXPECT_EQ(to_log_line(moved(TaskStatus::planning, TaskStatus::running)),
              "task=T1 from=PLANNING to=RUNNING robot=R01 version=7");
}

TEST(TaskChangeRecord, task_change_log_line_marks_a_task_with_no_robot) {
    TaskChange change = moved(TaskStatus::created, TaskStatus::cancelled);
    change.from_robot = std::nullopt;
    change.to_robot = std::nullopt;

    EXPECT_EQ(to_log_line(change), "task=T1 from=CREATED to=CANCELLED robot=- version=7");
}

TEST(TaskChangeRecord, task_change_compares_by_value) {
    // A change is a record of what happened, not an entity, so two identical
    // records are the same record. docs/24_DOMAIN_MODEL.md §33.
    EXPECT_EQ(moved(TaskStatus::planning, TaskStatus::running),
              moved(TaskStatus::planning, TaskStatus::running));
    EXPECT_NE(moved(TaskStatus::planning, TaskStatus::running),
              moved(TaskStatus::running, TaskStatus::waiting));
}

}  // namespace
}  // namespace traffic::task
