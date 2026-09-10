/// The task register. docs/23_SYSTEM_ARCHITECTURE.md §22,
/// docs/22_IMPLEMENTATION_WORKFLOW.md Phase 4, docs/04_ROBOT_TASK_MODEL.md
/// §12~16, docs/24_DOMAIN_MODEL.md §6, §30, §34.
///
/// Phase 4's completion criteria are task creation, assignment, start,
/// completion, cancellation, failure and priority handling, and
/// docs/04_ROBOT_TASK_MODEL.md §25 names test_task_state_transition. All of
/// them are here.

#include "traffic/task/task_manager.h"

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/task.h"
#include "traffic/domain/task_status.h"
#include "traffic/domain/values.h"
#include "traffic/task/task_change.h"
#include "traffic/task/task_version.h"

namespace traffic::task {
namespace {

using core::NodeId;
using core::RobotId;
using core::TaskId;
using domain::TaskStatus;

core::Duration seconds(int count) {
    return std::chrono::duration_cast<core::Duration>(core::Seconds{count});
}

core::TimePoint at(int second) {
    return core::kTimeOrigin + seconds(second);
}

domain::Task new_task(std::string id, int priority = 50) {
    auto built = domain::make_task(
        TaskId{std::move(id)}, NodeId{"N1"}, NodeId{"N2"}, domain::Priority{priority}, at(0));
    EXPECT_TRUE(built.has_value());
    return std::move(built).value();
}

void given_task(TaskManager& manager, const char* id, int priority = 50) {
    EXPECT_TRUE(manager.create(new_task(id, priority), at(0)).has_value());
}

/// Drives a task all the way to `running`, which is where most of the
/// interesting refusals live.
void given_running_task(TaskManager& manager, const char* id, const char* robot = "R01") {
    given_task(manager, id);
    EXPECT_TRUE(manager.assign(TaskId{id}, RobotId{robot}, at(1)).has_value());
    EXPECT_TRUE(start_planning(manager, TaskId{id}, at(2)).has_value());
    EXPECT_TRUE(start(manager, TaskId{id}, at(3)).has_value());
}

// ====================================================================== create

TEST(TaskCreate, task_create_registers_a_task_in_created) {
    TaskManager manager;

    ASSERT_TRUE(manager.create(new_task("T1"), at(0)).has_value());

    const domain::Task* task = manager.task(TaskId{"T1"});
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->status, TaskStatus::created);
    EXPECT_FALSE(task->robot_id.has_value());
    EXPECT_TRUE(manager.contains(TaskId{"T1"}));
    EXPECT_EQ(manager.task_count(), 1U);
}

TEST(TaskCreate, task_create_refuses_an_unnamed_task) {
    TaskManager manager;

    const auto status = manager.create(domain::Task{}, at(0));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), TaskRejection::empty_task_id);
    EXPECT_EQ(manager.task_count(), 0U);
}

TEST(TaskCreate, task_create_refuses_a_duplicate) {
    // The second task would silently replace the first, and whichever robot
    // was working on the first would be running work nobody could look up.
    TaskManager manager;
    given_task(manager, "T1");

    const auto status = manager.create(new_task("T1"), at(5));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), TaskRejection::duplicate_task);
    EXPECT_EQ(manager.task_count(), 1U);
}

TEST(TaskCreate, task_create_refuses_a_task_that_goes_nowhere) {
    // docs/24_DOMAIN_MODEL.md §34. Re-checked here because Task is an
    // aggregate and a caller can build one without the factory.
    TaskManager manager;
    domain::Task task = new_task("T1");
    task.destination = task.source;

    const auto status = manager.create(std::move(task), at(0));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), TaskRejection::same_source_and_destination);
}

TEST(TaskCreate, task_create_refuses_a_task_that_arrives_half_way_through_its_life) {
    // A task enters at `created` and gets everywhere else through this
    // register. One that arrives already assigned has a history the register
    // never saw and cannot reconstruct.
    TaskManager manager;
    domain::Task task = new_task("T1");
    task.status = TaskStatus::running;

    const auto status = manager.create(std::move(task), at(0));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), TaskRejection::not_a_new_task);
}

TEST(TaskCreate, task_create_refuses_a_task_that_arrives_already_holding_a_robot) {
    TaskManager manager;
    domain::Task task = new_task("T1");
    task.robot_id = RobotId{"R01"};

    const auto status = manager.create(std::move(task), at(0));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), TaskRejection::not_a_new_task);
}

TEST(TaskCreate, task_create_refuses_a_priority_outside_the_band) {
    // docs/04_ROBOT_TASK_MODEL.md §15: 0 to 100.
    TaskManager manager;

    ASSERT_EQ(manager.create(new_task("T1", 101), at(0)).error(),
              TaskRejection::priority_out_of_range);
    ASSERT_EQ(manager.create(new_task("T2", -1), at(0)).error(),
              TaskRejection::priority_out_of_range);
    EXPECT_EQ(manager.task_count(), 0U);
}

TEST(TaskCreate, task_create_accepts_the_edges_of_the_priority_band) {
    TaskManager manager;

    EXPECT_TRUE(manager.create(new_task("T1", 0), at(0)).has_value());
    EXPECT_TRUE(manager.create(new_task("T2", 100), at(0)).has_value());
}

TEST(TaskCreate, task_create_refuses_a_deadline_that_has_already_passed) {
    // Such a task is overdue from the instant it exists, so it would sit at
    // the front of every urgency queue forever without being satisfiable.
    TaskManager manager;
    domain::Task task = new_task("T1");
    task.created_at = at(10);
    task.deadline = at(5);

    const auto status = manager.create(std::move(task), at(10));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), TaskRejection::deadline_before_creation);
}

TEST(TaskCreate, task_create_keeps_the_deadline_it_was_given) {
    // The reason `create` takes a built Task: docs/04_ROBOT_TASK_MODEL.md §12
    // gives a task a deadline and `domain::make_task` does not take one.
    TaskManager manager;
    domain::Task task = new_task("T1");
    task.deadline = at(60);

    ASSERT_TRUE(manager.create(std::move(task), at(0)).has_value());

    ASSERT_NE(manager.task(TaskId{"T1"}), nullptr);
    EXPECT_EQ(manager.task(TaskId{"T1"})->deadline, at(60));
}

// =================================================================== lifecycle

TEST(TaskLifecycle, task_state_transition_follows_the_documented_path) {
    // docs/04_ROBOT_TASK_MODEL.md §14, docs/24_DOMAIN_MODEL.md §6:
    // CREATED -> ASSIGNED -> PLANNING -> RUNNING -> COMPLETED.
    TaskManager manager;
    given_task(manager, "T1");

    ASSERT_TRUE(manager.assign(TaskId{"T1"}, RobotId{"R01"}, at(1)).has_value());
    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::assigned);

    ASSERT_TRUE(start_planning(manager, TaskId{"T1"}, at(2)).has_value());
    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::planning);

    ASSERT_TRUE(start(manager, TaskId{"T1"}, at(3)).has_value());
    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::running);

    ASSERT_TRUE(complete(manager, TaskId{"T1"}, at(9)).has_value());
    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::completed);
}

TEST(TaskLifecycle, task_state_transition_stamps_the_timestamps_it_describes) {
    // Stamped by the domain, not by the caller, so a status and its timestamp
    // cannot disagree about what happened.
    TaskManager manager;
    given_running_task(manager, "T1");

    ASSERT_TRUE(complete(manager, TaskId{"T1"}, at(9)).has_value());

    const domain::Task* task = manager.task(TaskId{"T1"});
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->started_at, at(3));
    EXPECT_EQ(task->completed_at, at(9));
}

TEST(TaskLifecycle, task_state_transition_refuses_a_move_the_machine_rejects) {
    // Nothing has been planned, so nothing can be running.
    TaskManager manager;
    given_task(manager, "T1");

    const auto outcome = start(manager, TaskId{"T1"}, at(1));

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error(), TaskRejection::invalid_transition);
    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::created);
}

TEST(TaskLifecycle, task_state_transition_never_leaves_a_terminal_state) {
    // docs/24_DOMAIN_MODEL.md §30. Re-running a finished task would
    // double-count throughput and send a robot to a destination whose work is
    // already done.
    TaskManager manager;
    given_running_task(manager, "T1");
    ASSERT_TRUE(complete(manager, TaskId{"T1"}, at(9)).has_value());

    EXPECT_EQ(start(manager, TaskId{"T1"}, at(10)).error(), TaskRejection::invalid_transition);
    EXPECT_EQ(cancel(manager, TaskId{"T1"}, at(10)).error(), TaskRejection::invalid_transition);
    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::completed);
}

TEST(TaskLifecycle, task_state_transition_alternates_between_running_and_waiting) {
    // Traffic control holds a robot and releases it repeatedly over one task.
    TaskManager manager;
    given_running_task(manager, "T1");

    ASSERT_TRUE(hold(manager, TaskId{"T1"}, at(4)).has_value());
    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::waiting);

    ASSERT_TRUE(start(manager, TaskId{"T1"}, at(5)).has_value());
    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::running);

    ASSERT_TRUE(hold(manager, TaskId{"T1"}, at(6)).has_value());
    ASSERT_TRUE(start(manager, TaskId{"T1"}, at(7)).has_value());
    EXPECT_EQ(manager.task(TaskId{"T1"})->started_at, at(3));
}

TEST(TaskLifecycle, task_cancel_withdraws_a_task_nobody_has_started) {
    // docs/04_ROBOT_TASK_MODEL.md §14: CREATED / ASSIGNED / PLANNED may be
    // cancelled.
    TaskManager manager;
    given_task(manager, "T1");

    ASSERT_TRUE(cancel(manager, TaskId{"T1"}, at(2)).has_value());

    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::cancelled);
    EXPECT_EQ(manager.task(TaskId{"T1"})->completed_at, at(2));
}

TEST(TaskLifecycle, task_fail_ends_a_running_task) {
    TaskManager manager;
    given_running_task(manager, "T1");

    ASSERT_TRUE(fail(manager, TaskId{"T1"}, at(8)).has_value());

    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::failed);
    EXPECT_EQ(manager.task(TaskId{"T1"})->completed_at, at(8));
}

TEST(TaskLifecycle, task_repeated_status_is_accepted_and_moves_nothing) {
    // A re-sent message is not an error, but it is not a change either.
    TaskManager manager;
    given_running_task(manager, "T1");
    const TaskVersion before = manager.version();

    const auto outcome = start(manager, TaskId{"T1"}, at(5));

    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome.value().has_value());
    EXPECT_EQ(manager.version(), before);
}

TEST(TaskLifecycle, task_lifecycle_refuses_an_unknown_task) {
    TaskManager manager;
    given_task(manager, "T1");

    EXPECT_EQ(complete(manager, TaskId{"T99"}, at(1)).error(), TaskRejection::unknown_task);
}

// ====================================================================== assign

TEST(TaskAssign, task_assign_gives_the_task_to_a_robot) {
    TaskManager manager;
    given_task(manager, "T1");

    ASSERT_TRUE(manager.assign(TaskId{"T1"}, RobotId{"R01"}, at(1)).has_value());

    const domain::Task* task = manager.task(TaskId{"T1"});
    ASSERT_NE(task, nullptr);
    EXPECT_EQ(task->robot_id, RobotId{"R01"});
    EXPECT_EQ(task->status, TaskStatus::assigned);
}

TEST(TaskAssign, task_assign_refuses_a_robot_with_no_id) {
    TaskManager manager;
    given_task(manager, "T1");

    const auto outcome = manager.assign(TaskId{"T1"}, RobotId{}, at(1));

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error(), TaskRejection::empty_robot_id);
    EXPECT_FALSE(manager.task(TaskId{"T1"})->robot_id.has_value());
}

TEST(TaskAssign, task_assign_refuses_an_unknown_task) {
    TaskManager manager;

    EXPECT_EQ(manager.assign(TaskId{"T1"}, RobotId{"R01"}, at(1)).error(),
              TaskRejection::unknown_task);
}

TEST(TaskAssign, task_assign_moves_a_task_to_another_robot_before_it_runs) {
    // The transition table lets `planning` go back to `assigned`, so a task
    // can change hands right up until it starts moving.
    TaskManager manager;
    given_task(manager, "T1");
    ASSERT_TRUE(manager.assign(TaskId{"T1"}, RobotId{"R01"}, at(1)).has_value());
    ASSERT_TRUE(start_planning(manager, TaskId{"T1"}, at(2)).has_value());

    const auto outcome = manager.assign(TaskId{"T1"}, RobotId{"R02"}, at(3));

    ASSERT_TRUE(outcome.has_value());
    ASSERT_TRUE(outcome.value().has_value());
    EXPECT_TRUE(is_reassignment(*outcome.value()));
    EXPECT_EQ(outcome.value()->from_robot, RobotId{"R01"});
    EXPECT_EQ(outcome.value()->to_robot, RobotId{"R02"});
    EXPECT_EQ(manager.task(TaskId{"T1"})->robot_id, RobotId{"R02"});
}

TEST(TaskAssign, task_assign_refuses_to_move_a_task_that_is_already_running) {
    // Taking it away now would leave a robot executing work the register says
    // belongs to somebody else.
    TaskManager manager;
    given_running_task(manager, "T1");

    const auto outcome = manager.assign(TaskId{"T1"}, RobotId{"R02"}, at(4));

    ASSERT_FALSE(outcome.has_value());
    EXPECT_EQ(outcome.error(), TaskRejection::invalid_transition);
    EXPECT_EQ(manager.task(TaskId{"T1"})->robot_id, RobotId{"R01"});
}

TEST(TaskAssign, task_assign_to_the_same_robot_again_moves_nothing) {
    TaskManager manager;
    given_task(manager, "T1");
    ASSERT_TRUE(manager.assign(TaskId{"T1"}, RobotId{"R01"}, at(1)).has_value());
    const TaskVersion before = manager.version();

    const auto outcome = manager.assign(TaskId{"T1"}, RobotId{"R01"}, at(2));

    ASSERT_TRUE(outcome.has_value());
    EXPECT_FALSE(outcome.value().has_value());
    EXPECT_EQ(manager.version(), before);
}

TEST(TaskAssign, task_assign_reports_the_change_it_made) {
    TaskManager manager;
    given_task(manager, "T1");

    const auto outcome = manager.assign(TaskId{"T1"}, RobotId{"R01"}, at(1));

    ASSERT_TRUE(outcome.has_value());
    ASSERT_TRUE(outcome.value().has_value());
    EXPECT_EQ(outcome.value()->task_id, TaskId{"T1"});
    EXPECT_EQ(outcome.value()->from, TaskStatus::created);
    EXPECT_EQ(outcome.value()->to, TaskStatus::assigned);
    EXPECT_FALSE(outcome.value()->from_robot.has_value());
    EXPECT_EQ(outcome.value()->to_robot, RobotId{"R01"});
    EXPECT_EQ(outcome.value()->at, at(1));
    EXPECT_EQ(outcome.value()->version, manager.version());
}

// ======================================================================= query

TEST(TaskQuery, task_query_lists_tasks_in_id_order) {
    // Ordered, not hashed: a listing that came back differently on every run
    // would make the scenarios of docs/26_TEST_SCENARIOS.md impossible to
    // compare.
    TaskManager manager;
    given_task(manager, "T3");
    given_task(manager, "T1");
    given_task(manager, "T2");

    const std::vector<domain::Task> listed = manager.tasks();

    ASSERT_EQ(listed.size(), 3U);
    EXPECT_EQ(listed[0].id, TaskId{"T1"});
    EXPECT_EQ(listed[1].id, TaskId{"T2"});
    EXPECT_EQ(listed[2].id, TaskId{"T3"});
}

TEST(TaskQuery, task_query_finds_the_tasks_in_a_status) {
    TaskManager manager;
    given_task(manager, "T1");
    given_task(manager, "T2");
    ASSERT_TRUE(manager.assign(TaskId{"T2"}, RobotId{"R01"}, at(1)).has_value());

    EXPECT_EQ(manager.tasks_with_status(TaskStatus::created), std::vector<TaskId>{TaskId{"T1"}});
    EXPECT_EQ(manager.tasks_with_status(TaskStatus::assigned), std::vector<TaskId>{TaskId{"T2"}});
    EXPECT_TRUE(manager.tasks_with_status(TaskStatus::failed).empty());
}

TEST(TaskQuery, task_query_finds_everything_a_robot_was_ever_given) {
    TaskManager manager;
    given_running_task(manager, "T2", "R01");
    given_running_task(manager, "T1", "R01");
    given_running_task(manager, "T3", "R02");
    ASSERT_TRUE(complete(manager, TaskId{"T1"}, at(9)).has_value());

    const std::vector<TaskId> owned = manager.tasks_for_robot(RobotId{"R01"});

    ASSERT_EQ(owned.size(), 2U);
    EXPECT_EQ(owned[0], TaskId{"T1"});
    EXPECT_EQ(owned[1], TaskId{"T2"});
}

TEST(TaskQuery, task_query_active_tasks_leave_out_the_finished_ones) {
    // What still occupies the robot, which is the question an assignment
    // engine asks.
    TaskManager manager;
    given_running_task(manager, "T1", "R01");
    given_running_task(manager, "T2", "R01");
    ASSERT_TRUE(complete(manager, TaskId{"T1"}, at(9)).has_value());

    EXPECT_EQ(manager.active_tasks_for_robot(RobotId{"R01"}), std::vector<TaskId>{TaskId{"T2"}});
    EXPECT_TRUE(manager.active_tasks_for_robot(RobotId{"R99"}).empty());
}

TEST(TaskQuery, task_query_returns_nothing_for_an_unknown_task) {
    TaskManager manager;
    given_task(manager, "T1");

    EXPECT_EQ(manager.task(TaskId{"T99"}), nullptr);
    EXPECT_EQ(manager.record(TaskId{"T99"}), nullptr);
    EXPECT_FALSE(manager.contains(TaskId{"T99"}));
}

// ===================================================================== version

TEST(TaskVersioning, task_version_starts_at_zero_and_advances_with_each_change) {
    TaskManager manager;
    EXPECT_EQ(manager.version(), TaskVersion{0});

    given_task(manager, "T1");
    EXPECT_EQ(manager.version(), TaskVersion{1});

    ASSERT_TRUE(manager.assign(TaskId{"T1"}, RobotId{"R01"}, at(1)).has_value());
    EXPECT_EQ(manager.version(), TaskVersion{2});
}

TEST(TaskVersioning, task_version_is_set_wide_not_per_task) {
    // "Has anything I planned around changed" is a question about the set. A
    // per-task counter cannot answer it without walking every task.
    TaskManager manager;
    given_task(manager, "T1");
    given_task(manager, "T2");

    ASSERT_TRUE(manager.assign(TaskId{"T2"}, RobotId{"R01"}, at(1)).has_value());

    ASSERT_NE(manager.record(TaskId{"T1"}), nullptr);
    ASSERT_NE(manager.record(TaskId{"T2"}), nullptr);
    EXPECT_EQ(manager.record(TaskId{"T1"})->version, TaskVersion{1});
    EXPECT_EQ(manager.record(TaskId{"T2"})->version, TaskVersion{3});
    EXPECT_EQ(manager.version(), TaskVersion{3});
}

TEST(TaskVersioning, task_version_does_not_move_for_a_refused_call) {
    TaskManager manager;
    given_task(manager, "T1");
    const TaskVersion before = manager.version();

    EXPECT_FALSE(start(manager, TaskId{"T1"}, at(1)).has_value());
    EXPECT_FALSE(manager.create(new_task("T1"), at(1)).has_value());
    EXPECT_FALSE(manager.assign(TaskId{"T1"}, RobotId{}, at(1)).has_value());

    EXPECT_EQ(manager.version(), before);
}

TEST(TaskVersioning, task_version_does_not_move_for_a_query) {
    TaskManager manager;
    given_task(manager, "T1");
    const TaskVersion before = manager.version();

    EXPECT_EQ(manager.tasks().size(), 1U);
    EXPECT_TRUE(manager.contains(TaskId{"T1"}));
    EXPECT_TRUE(manager.overdue_tasks(at(100)).empty());

    EXPECT_EQ(manager.version(), before);
}

TEST(TaskVersioning, task_version_moves_when_a_task_is_forgotten) {
    // The set of tasks is different from what a decision made a moment ago
    // assumed.
    TaskManager manager;
    given_running_task(manager, "T1");
    ASSERT_TRUE(complete(manager, TaskId{"T1"}, at(9)).has_value());
    const TaskVersion before = manager.version();

    ASSERT_TRUE(manager.remove(TaskId{"T1"}).has_value());

    EXPECT_EQ(manager.version(), before.next());
}

// ==================================================================== priority

TEST(TaskPriority, task_priority_orders_the_backlog_highest_first) {
    // docs/04_ROBOT_TASK_MODEL.md §15: higher number, higher priority.
    TaskManager manager;
    given_task(manager, "T1", 10);
    given_task(manager, "T2", 90);
    given_task(manager, "T3", 50);

    const std::vector<TaskId> ordered = by_priority(manager, TaskStatus::created);

    ASSERT_EQ(ordered.size(), 3U);
    EXPECT_EQ(ordered[0], TaskId{"T2"});
    EXPECT_EQ(ordered[1], TaskId{"T3"});
    EXPECT_EQ(ordered[2], TaskId{"T1"});
}

TEST(TaskPriority, task_priority_breaks_ties_by_id_so_two_runs_agree) {
    // Determinism, CLAUDE.md and docs/01_REQUIREMENTS.md NFR-003: an
    // equal-priority backlog must not be ordered by whatever the container
    // felt like.
    TaskManager manager;
    given_task(manager, "T3", 90);
    given_task(manager, "T1", 90);
    given_task(manager, "T2", 90);

    const std::vector<TaskId> ordered = by_priority(manager, TaskStatus::created);

    ASSERT_EQ(ordered.size(), 3U);
    EXPECT_EQ(ordered[0], TaskId{"T1"});
    EXPECT_EQ(ordered[1], TaskId{"T2"});
    EXPECT_EQ(ordered[2], TaskId{"T3"});
}

TEST(TaskPriority, task_priority_can_be_raised_while_the_task_is_live) {
    TaskManager manager;
    given_task(manager, "T1", 10);

    ASSERT_TRUE(manager.set_priority(TaskId{"T1"}, domain::Priority{80}, at(2)).has_value());

    EXPECT_EQ(manager.task(TaskId{"T1"})->priority, domain::Priority{80});
    EXPECT_EQ(manager.version(), TaskVersion{2});
}

TEST(TaskPriority, task_priority_refuses_a_value_outside_the_band) {
    TaskManager manager;
    given_task(manager, "T1");

    EXPECT_EQ(manager.set_priority(TaskId{"T1"}, domain::Priority{101}, at(2)).error(),
              TaskRejection::priority_out_of_range);
    EXPECT_EQ(manager.set_priority(TaskId{"T1"}, domain::Priority{-1}, at(2)).error(),
              TaskRejection::priority_out_of_range);
}

TEST(TaskPriority, task_priority_refuses_to_re_band_a_finished_task) {
    TaskManager manager;
    given_running_task(manager, "T1");
    ASSERT_TRUE(complete(manager, TaskId{"T1"}, at(9)).has_value());

    EXPECT_EQ(manager.set_priority(TaskId{"T1"}, domain::Priority{80}, at(10)).error(),
              TaskRejection::task_is_terminal);
}

TEST(TaskPriority, task_priority_refuses_an_unknown_task) {
    TaskManager manager;

    EXPECT_EQ(manager.set_priority(TaskId{"T1"}, domain::Priority{80}, at(2)).error(),
              TaskRejection::unknown_task);
}

// ==================================================================== deadline

TEST(TaskDeadline, task_deadline_is_reported_once_the_instant_has_passed) {
    // docs/04_ROBOT_TASK_MODEL.md §12. Reported at a supplied instant — the
    // register never reads a clock.
    TaskManager manager;
    given_task(manager, "T1");
    ASSERT_TRUE(manager.set_deadline(TaskId{"T1"}, at(30), at(0)).has_value());

    EXPECT_TRUE(manager.overdue_tasks(at(29)).empty());
    EXPECT_TRUE(manager.overdue_tasks(at(30)).empty());
    EXPECT_EQ(manager.overdue_tasks(at(31)), std::vector<TaskId>{TaskId{"T1"}});
}

TEST(TaskDeadline, task_deadline_absent_means_never_overdue) {
    TaskManager manager;
    given_task(manager, "T1");

    EXPECT_TRUE(manager.overdue_tasks(at(100000)).empty());
}

TEST(TaskDeadline, task_deadline_leaves_out_tasks_that_are_already_over) {
    // This answers "what still needs attention", and nothing can be done for
    // work that has finished.
    TaskManager manager;
    given_running_task(manager, "T1");
    ASSERT_TRUE(manager.set_deadline(TaskId{"T1"}, at(30), at(4)).has_value());
    ASSERT_TRUE(complete(manager, TaskId{"T1"}, at(40)).has_value());

    EXPECT_TRUE(manager.overdue_tasks(at(50)).empty());
}

TEST(TaskDeadline, task_deadline_lists_in_id_order) {
    TaskManager manager;
    given_task(manager, "T3");
    given_task(manager, "T1");
    given_task(manager, "T2");
    for (const char* id : {"T1", "T2", "T3"}) {
        ASSERT_TRUE(manager.set_deadline(TaskId{id}, at(30), at(0)).has_value());
    }

    const std::vector<TaskId> overdue = manager.overdue_tasks(at(31));

    ASSERT_EQ(overdue.size(), 3U);
    EXPECT_EQ(overdue[0], TaskId{"T1"});
    EXPECT_EQ(overdue[1], TaskId{"T2"});
    EXPECT_EQ(overdue[2], TaskId{"T3"});
}

TEST(TaskDeadline, task_deadline_can_be_cleared) {
    TaskManager manager;
    given_task(manager, "T1");
    ASSERT_TRUE(manager.set_deadline(TaskId{"T1"}, at(30), at(0)).has_value());

    ASSERT_TRUE(manager.set_deadline(TaskId{"T1"}, std::nullopt, at(1)).has_value());

    EXPECT_FALSE(manager.task(TaskId{"T1"})->deadline.has_value());
    EXPECT_TRUE(manager.overdue_tasks(at(31)).empty());
}

TEST(TaskDeadline, task_deadline_refuses_an_instant_before_the_task_existed) {
    TaskManager manager;
    given_task(manager, "T1");

    EXPECT_EQ(manager.set_deadline(TaskId{"T1"}, core::kTimeOrigin - seconds(1), at(1)).error(),
              TaskRejection::deadline_before_creation);
}

TEST(TaskDeadline, task_deadline_refuses_a_finished_task) {
    TaskManager manager;
    given_running_task(manager, "T1");
    ASSERT_TRUE(complete(manager, TaskId{"T1"}, at(9)).has_value());

    EXPECT_EQ(manager.set_deadline(TaskId{"T1"}, at(30), at(10)).error(),
              TaskRejection::task_is_terminal);
}

// ====================================================================== remove

TEST(TaskRemove, task_remove_forgets_a_finished_task) {
    TaskManager manager;
    given_running_task(manager, "T1");
    ASSERT_TRUE(complete(manager, TaskId{"T1"}, at(9)).has_value());

    ASSERT_TRUE(manager.remove(TaskId{"T1"}).has_value());

    EXPECT_FALSE(manager.contains(TaskId{"T1"}));
    EXPECT_EQ(manager.task_count(), 0U);
}

TEST(TaskRemove, task_remove_refuses_a_task_that_is_still_live) {
    // Dropping it would strand whichever robot is carrying it out, with no
    // record left of what that robot was doing.
    TaskManager manager;
    given_running_task(manager, "T1");

    const auto status = manager.remove(TaskId{"T1"});

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), TaskRejection::task_still_live);
    EXPECT_TRUE(manager.contains(TaskId{"T1"}));
}

TEST(TaskRemove, task_remove_refuses_an_unknown_task) {
    TaskManager manager;

    EXPECT_EQ(manager.remove(TaskId{"T1"}).error(), TaskRejection::unknown_task);
}

// ======================================================================= names

TEST(TaskRejectionName, task_rejection_every_value_has_a_name) {
    for (const TaskRejection rejection : {TaskRejection::empty_task_id,
                                          TaskRejection::duplicate_task,
                                          TaskRejection::unknown_task,
                                          TaskRejection::not_a_new_task,
                                          TaskRejection::same_source_and_destination,
                                          TaskRejection::invalid_transition,
                                          TaskRejection::empty_robot_id,
                                          TaskRejection::task_is_terminal,
                                          TaskRejection::task_still_live,
                                          TaskRejection::priority_out_of_range,
                                          TaskRejection::deadline_before_creation}) {
        EXPECT_NE(to_string(rejection), "UNKNOWN");
    }
}

}  // namespace
}  // namespace traffic::task
