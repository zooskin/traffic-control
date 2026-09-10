/// The assignment seam. docs/04_ROBOT_TASK_MODEL.md §16, §24 ("Task
/// assignment interface"), docs/22_IMPLEMENTATION_WORKFLOW.md Phase 4.
///
/// These tests exercise the shape of the conversation between the task
/// register and an assignment engine. They do **not** test an assignment
/// policy, because there is none in this module and §16 says there should not
/// be — the stub below hands out robots in the order it was given them, which
/// is enough to prove the seam carries what an engine needs and nothing more.

#include "traffic/task/task_assignment.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/task.h"
#include "traffic/domain/task_status.h"
#include "traffic/domain/values.h"
#include "traffic/task/task_manager.h"

namespace traffic::task {
namespace {

using core::NodeId;
using core::RobotId;
using core::TaskId;
using domain::TaskStatus;

core::TimePoint at(int second) {
    return core::kTimeOrigin + std::chrono::duration_cast<core::Duration>(core::Seconds{second});
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

/// Pairs each request with the next free robot, in the order both arrived.
///
/// Not a policy: it looks at nothing about the robot, which is precisely why
/// it is safe to keep in this module. A real engine needs robot positions and
/// route costs, and both live above `task` in the dependency order.
class InOrderStub final : public ITaskAssignment {
public:
    [[nodiscard]] std::vector<AssignmentProposal> propose(
        std::span<const AssignmentRequest> requests,
        std::span<const core::RobotId> available_robots) override {
        std::vector<AssignmentProposal> proposals;
        const std::size_t pairs = std::min(requests.size(), available_robots.size());
        proposals.reserve(pairs);
        for (std::size_t index = 0; index < pairs; ++index) {
            proposals.push_back(
                AssignmentProposal{requests[index].task_id, available_robots[index]});
        }
        return proposals;
    }
};

// ==================================================================== requests

TEST(TaskAssignmentSeam, assignment_requests_offer_only_tasks_without_a_robot) {
    // A task in `assigned` or beyond already has one. Offering it again would
    // let two engines hand the same work to two robots.
    TaskManager manager;
    given_task(manager, "T1");
    given_task(manager, "T2");
    ASSERT_TRUE(manager.assign(TaskId{"T2"}, RobotId{"R01"}, at(1)).has_value());

    const std::vector<AssignmentRequest> requests = assignment_requests(manager, at(2));

    ASSERT_EQ(requests.size(), 1U);
    EXPECT_EQ(requests[0].task_id, TaskId{"T1"});
}

TEST(TaskAssignmentSeam, assignment_requests_carry_what_the_task_itself_knows) {
    // Endpoints, priority and deadline. No route and no cost: what a journey
    // costs is the planner's answer, and an engine that wants one asks the
    // planner rather than being handed a stale copy through here.
    TaskManager manager;
    domain::Task task = new_task("T1", 70);
    task.deadline = at(90);
    ASSERT_TRUE(manager.create(std::move(task), at(0)).has_value());

    const std::vector<AssignmentRequest> requests = assignment_requests(manager, at(2));

    ASSERT_EQ(requests.size(), 1U);
    EXPECT_EQ(requests[0].source, NodeId{"N1"});
    EXPECT_EQ(requests[0].destination, NodeId{"N2"});
    EXPECT_EQ(requests[0].priority, domain::Priority{70});
    EXPECT_EQ(requests[0].deadline, at(90));
    EXPECT_EQ(requests[0].requested_at, at(2));
}

TEST(TaskAssignmentSeam, assignment_requests_are_offered_highest_priority_first) {
    TaskManager manager;
    given_task(manager, "T1", 10);
    given_task(manager, "T2", 90);
    given_task(manager, "T3", 90);

    const std::vector<AssignmentRequest> requests = assignment_requests(manager, at(2));

    ASSERT_EQ(requests.size(), 3U);
    EXPECT_EQ(requests[0].task_id, TaskId{"T2"});
    EXPECT_EQ(requests[1].task_id, TaskId{"T3"});
    EXPECT_EQ(requests[2].task_id, TaskId{"T1"});
}

TEST(TaskAssignmentSeam, assignment_requests_are_empty_when_nothing_is_waiting) {
    TaskManager manager;

    EXPECT_TRUE(assignment_requests(manager, at(2)).empty());
}

// ======================================================================= apply

TEST(TaskAssignmentSeam, assignment_applies_what_an_engine_proposed) {
    TaskManager manager;
    given_task(manager, "T1", 90);
    given_task(manager, "T2", 10);
    InOrderStub engine;

    const std::vector<AssignmentRequest> requests = assignment_requests(manager, at(2));
    const std::vector<RobotId> robots{RobotId{"R01"}, RobotId{"R02"}};
    const std::vector<AssignmentProposal> proposals = engine.propose(requests, robots);

    EXPECT_TRUE(apply_assignments(manager, proposals, at(3)).empty());

    ASSERT_NE(manager.task(TaskId{"T1"}), nullptr);
    ASSERT_NE(manager.task(TaskId{"T2"}), nullptr);
    EXPECT_EQ(manager.task(TaskId{"T1"})->robot_id, RobotId{"R01"});
    EXPECT_EQ(manager.task(TaskId{"T2"})->robot_id, RobotId{"R02"});
    EXPECT_EQ(manager.task(TaskId{"T1"})->status, TaskStatus::assigned);
}

TEST(TaskAssignmentSeam, assignment_leaves_a_task_alone_when_there_is_no_robot_for_it) {
    // "No candidate" is the normal state of a busy fleet, not a failure.
    TaskManager manager;
    given_task(manager, "T1", 90);
    given_task(manager, "T2", 10);
    InOrderStub engine;

    const std::vector<RobotId> robots{RobotId{"R01"}};
    const std::vector<AssignmentProposal> proposals =
        engine.propose(assignment_requests(manager, at(2)), robots);

    EXPECT_TRUE(apply_assignments(manager, proposals, at(3)).empty());

    EXPECT_EQ(manager.task(TaskId{"T1"})->robot_id, RobotId{"R01"});
    EXPECT_FALSE(manager.task(TaskId{"T2"})->robot_id.has_value());
}

TEST(TaskAssignmentSeam, assignment_returns_a_proposal_the_register_refused) {
    // The engine answered a question that has since gone stale. It is told so
    // it can offer that robot to something else, rather than having the answer
    // dropped on the floor.
    TaskManager manager;
    given_task(manager, "T1");
    const std::vector<AssignmentRequest> requests = assignment_requests(manager, at(2));
    ASSERT_EQ(requests.size(), 1U);
    ASSERT_TRUE(cancel(manager, TaskId{"T1"}, at(3)).has_value());

    const std::vector<AssignmentProposal> proposals{
        AssignmentProposal{TaskId{"T1"}, RobotId{"R01"}}};
    const std::vector<AssignmentProposal> refused = apply_assignments(manager, proposals, at(4));

    ASSERT_EQ(refused.size(), 1U);
    EXPECT_EQ(refused[0].task_id, TaskId{"T1"});
    EXPECT_EQ(refused[0].robot_id, RobotId{"R01"});
}

TEST(TaskAssignmentSeam, assignment_returns_a_proposal_for_a_task_that_no_longer_exists) {
    TaskManager manager;

    const std::vector<AssignmentProposal> proposals{
        AssignmentProposal{TaskId{"T1"}, RobotId{"R01"}}};

    EXPECT_EQ(apply_assignments(manager, proposals, at(4)).size(), 1U);
}

}  // namespace
}  // namespace traffic::task
