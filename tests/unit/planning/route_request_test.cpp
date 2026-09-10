/// Planning contracts. docs/05_GLOBAL_ROUTING.md §11~13, §18,
/// docs/24_DOMAIN_MODEL.md §20~21.

#include "traffic/planning/route_request.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/planning/planning_request.h"

namespace traffic::planning {
namespace {

using core::EdgeId;
using core::NodeId;
using core::ResourceId;
using core::RobotId;

RouteOutcome success_outcome(std::string robot) {
    RouteOutcome outcome;
    outcome.robot_id = RobotId{std::move(robot)};
    outcome.response = RouteResponse{};
    return outcome;
}

RouteOutcome failure_outcome(std::string robot, RouteFailure failure) {
    RouteOutcome outcome;
    outcome.robot_id = RobotId{std::move(robot)};
    outcome.failure = failure;
    return outcome;
}

// ------------------------------------------------------------- constraints

TEST(RouteConstraintsTest, DefaultAllowsEverything) {
    const RouteConstraints constraints;

    EXPECT_TRUE(is_unconstrained(constraints));
    EXPECT_TRUE(allows_node(constraints, NodeId{"N1"}));
    EXPECT_TRUE(allows_edge(constraints, EdgeId{"E1"}, ResourceId{"R1"}));
    EXPECT_EQ(constraint_count(constraints), 0U);
}

TEST(RouteConstraintsTest, BlockedNodeIsRefused) {
    RouteConstraints constraints;
    constraints.blocked_nodes.insert(NodeId{"N1"});

    EXPECT_FALSE(allows_node(constraints, NodeId{"N1"}));
    EXPECT_TRUE(allows_node(constraints, NodeId{"N2"}));
    EXPECT_FALSE(is_unconstrained(constraints));
}

TEST(RouteConstraintsTest, BlockedEdgeIsRefused) {
    RouteConstraints constraints;
    constraints.blocked_edges.insert(EdgeId{"E1"});

    EXPECT_FALSE(allows_edge(constraints, EdgeId{"E1"}, ResourceId{"R1"}));
    EXPECT_TRUE(allows_edge(constraints, EdgeId{"E2"}, ResourceId{"R1"}));
}

TEST(RouteConstraintsTest, BlockingAResourceBlocksEveryEdgeInIt) {
    // The point of docs/00_MASTER_PLAN.md §4.2: a corridor is one resource
    // over several edges. Taking it out of service must not have to name each
    // edge, or a corridor stays half-open when one is forgotten.
    RouteConstraints constraints;
    constraints.blocked_resources.insert(ResourceId{"CORRIDOR-01"});

    EXPECT_FALSE(allows_edge(constraints, EdgeId{"E-C1"}, ResourceId{"CORRIDOR-01"}));
    EXPECT_FALSE(allows_edge(constraints, EdgeId{"E-C2"}, ResourceId{"CORRIDOR-01"}));
    EXPECT_FALSE(allows_edge(constraints, EdgeId{"E-C3"}, ResourceId{"CORRIDOR-01"}));
    EXPECT_TRUE(allows_edge(constraints, EdgeId{"E-OTHER"}, ResourceId{"CORRIDOR-02"}));
}

TEST(RouteConstraintsTest, ForbiddenResourceIsRefusedLikeABlockedOne) {
    RouteConstraints constraints;
    constraints.forbidden_resources.insert(ResourceId{"LIFT-01"});

    EXPECT_FALSE(allows_edge(constraints, EdgeId{"E-LIFT"}, ResourceId{"LIFT-01"}));
}

TEST(RouteConstraintsTest, BlockedAndForbiddenStayDistinct) {
    // Both exclude, and they mean different things: one expires when the aisle
    // clears, the other is a property of the robot.
    RouteConstraints blocked;
    blocked.blocked_resources.insert(ResourceId{"R1"});

    RouteConstraints forbidden;
    forbidden.forbidden_resources.insert(ResourceId{"R1"});

    EXPECT_FALSE(allows_edge(blocked, EdgeId{"E1"}, ResourceId{"R1"}));
    EXPECT_FALSE(allows_edge(forbidden, EdgeId{"E1"}, ResourceId{"R1"}));
    EXPECT_NE(blocked, forbidden);
}

TEST(RouteConstraintsTest, PreferenceIsNotARestriction) {
    RouteConstraints constraints;
    constraints.preferred_resources.insert(ResourceId{"MAIN-AISLE"});

    EXPECT_TRUE(allows_edge(constraints, EdgeId{"E1"}, ResourceId{"MAIN-AISLE"}));
    EXPECT_TRUE(allows_edge(constraints, EdgeId{"E2"}, ResourceId{"SIDE-AISLE"}));
    EXPECT_TRUE(prefers_resource(constraints, ResourceId{"MAIN-AISLE"}));
    EXPECT_FALSE(prefers_resource(constraints, ResourceId{"SIDE-AISLE"}));
}

TEST(RouteConstraintsTest, CountsEveryRestriction) {
    RouteConstraints constraints;
    constraints.blocked_edges.insert(EdgeId{"E1"});
    constraints.blocked_nodes.insert(NodeId{"N1"});
    constraints.blocked_resources.insert(ResourceId{"R1"});
    constraints.forbidden_resources.insert(ResourceId{"R2"});
    constraints.preferred_resources.insert(ResourceId{"R3"});

    EXPECT_EQ(constraint_count(constraints), 5U);
}

TEST(RouteConstraintsTest, IterationOrderIsSorted) {
    // docs/20_CODING_GUIDELINES.md §23. These sets are iterated when a
    // planning decision is logged; a hashed container would produce a
    // different log for the same decision on a different run.
    RouteConstraints constraints;
    constraints.blocked_edges.insert(EdgeId{"E-C"});
    constraints.blocked_edges.insert(EdgeId{"E-A"});
    constraints.blocked_edges.insert(EdgeId{"E-B"});

    std::vector<std::string> seen;
    for (const EdgeId& edge : constraints.blocked_edges) {
        seen.push_back(edge.value());
    }

    EXPECT_EQ(seen, (std::vector<std::string>{"E-A", "E-B", "E-C"}));
}

// ---------------------------------------------------------------- failures

TEST(RouteFailureTest, EveryValueHasAName) {
    for (const RouteFailure failure : {RouteFailure::invalid_request,
                                       RouteFailure::unknown_start,
                                       RouteFailure::unknown_goal,
                                       RouteFailure::blocked_start,
                                       RouteFailure::blocked_goal,
                                       RouteFailure::no_route}) {
        EXPECT_NE(to_string(failure), "UNKNOWN");
    }
}

TEST(RouteFailureTest, NoRouteKeepsTheSpecName) {
    // docs/05_GLOBAL_ROUTING.md §13 names this exactly. It appears in logs and
    // in the traffic controller's policy table.
    EXPECT_EQ(to_string(RouteFailure::no_route), "NO_ROUTE");
}

TEST(RouteFailureTest, OnlyConstraintDrivenFailuresAreWorthRetrying) {
    EXPECT_TRUE(is_retryable(RouteFailure::no_route));
    EXPECT_TRUE(is_retryable(RouteFailure::blocked_start));
    EXPECT_TRUE(is_retryable(RouteFailure::blocked_goal));

    EXPECT_FALSE(is_retryable(RouteFailure::invalid_request));
    EXPECT_FALSE(is_retryable(RouteFailure::unknown_start));
    EXPECT_FALSE(is_retryable(RouteFailure::unknown_goal));
}

// ------------------------------------------------------------------- batch

TEST(PlanningStatusTest, EveryValueHasAName) {
    for (const PlanningStatus status : {PlanningStatus::success,
                                        PlanningStatus::no_path,
                                        PlanningStatus::timeout,
                                        PlanningStatus::cancelled,
                                        PlanningStatus::failed}) {
        EXPECT_NE(to_string(status), "UNKNOWN");
    }
}

TEST(PlanningResultTest, AllSucceededIsSuccess) {
    const std::vector<RouteOutcome> outcomes{success_outcome("R01"), success_outcome("R02")};

    EXPECT_EQ(status_for(outcomes), PlanningStatus::success);
}

TEST(PlanningResultTest, EmptyBatchIsSuccess) {
    // An idle fleet asked for nothing is not a failure.
    EXPECT_EQ(status_for({}), PlanningStatus::success);
}

TEST(PlanningResultTest, OneUnreachableGoalIsNoPathNotFailed) {
    const std::vector<RouteOutcome> outcomes{success_outcome("R01"),
                                             failure_outcome("R02", RouteFailure::no_route)};

    EXPECT_EQ(status_for(outcomes), PlanningStatus::no_path);
}

TEST(PlanningResultTest, AMalformedRequestIsFailed) {
    // A request that can never succeed is a different answer from a goal that
    // is unreachable right now: NO_PATH invites a retry, FAILED does not.
    const std::vector<RouteOutcome> outcomes{success_outcome("R01"),
                                             failure_outcome("R02", RouteFailure::unknown_goal)};

    EXPECT_EQ(status_for(outcomes), PlanningStatus::failed);
}

TEST(PlanningResultTest, PartialFailureStillReturnsTheRoutesThatWorked) {
    // One unreachable goal must not stall two hundred robots.
    PlanningResult result;
    result.outcomes = {success_outcome("R01"),
                       failure_outcome("R02", RouteFailure::no_route),
                       success_outcome("R03")};
    result.status = status_for(result.outcomes);

    EXPECT_EQ(result.status, PlanningStatus::no_path);
    EXPECT_EQ(succeeded_count(result), 2U);
    EXPECT_EQ(successful_routes(result).size(), 2U);
}

TEST(PlanningResultTest, OutcomesKeepRequestOrder) {
    PlanningResult result;
    result.outcomes = {success_outcome("R03"), success_outcome("R01"), success_outcome("R02")};

    ASSERT_EQ(result.outcomes.size(), 3U);
    EXPECT_EQ(result.outcomes[0].robot_id, RobotId{"R03"});
    EXPECT_EQ(result.outcomes[1].robot_id, RobotId{"R01"});
    EXPECT_EQ(result.outcomes[2].robot_id, RobotId{"R02"});
}

}  // namespace
}  // namespace traffic::planning
