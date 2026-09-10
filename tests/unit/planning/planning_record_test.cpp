/// The planning log record. docs/05_GLOBAL_ROUTING.md §23.

#include "traffic/planning/planning_record.h"

#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "planning_test_map.h"
#include "traffic/core/clock.h"
#include "traffic/core/ids.h"
#include "traffic/planning/astar_planner.h"
#include "traffic/planning/cost_model.h"
#include "traffic/planning/heuristic.h"

namespace traffic::planning {
namespace {

using core::NodeId;
using core::ResourceId;
using core::RobotId;
using test::build;

struct Fixture {
    map::Map graph;
    core::SimulationClock clock;
    FreeFlowConditions conditions;
    EuclideanHeuristic heuristic;
    AStarPlanner planner;

    explicit Fixture(map::MapData data)
        : graph(build(std::move(data))),
          heuristic(make_admissible_heuristic(graph, CostWeights{})),
          planner(graph, clock, heuristic, conditions, AStarConfig{}) {}
};

RouteRequest request_for(std::string start, std::string goal) {
    RouteRequest request;
    request.robot_id = RobotId{"R01"};
    request.start_node = NodeId{std::move(start)};
    request.goal_node = NodeId{std::move(goal)};
    return request;
}

TEST(PlanningRecord, planning_record_carries_every_field_the_spec_names) {
    Fixture fixture{test::line_map()};

    RouteRequest request = request_for("A", "C");
    request.reason = "task assigned";
    request.constraints.blocked_nodes.insert(NodeId{"Z"});

    const auto result = fixture.planner.plan_route(request);
    ASSERT_TRUE(result.has_value());

    const PlanningRecord record = make_planning_record(request, result, nullptr);

    EXPECT_EQ(record.robot_id, RobotId{"R01"});
    EXPECT_EQ(record.start, NodeId{"A"});
    EXPECT_EQ(record.goal, NodeId{"C"});
    EXPECT_EQ(record.planner, "astar");
    EXPECT_FALSE(record.old_route.has_value());
    ASSERT_TRUE(record.new_route.has_value());
    EXPECT_EQ(*record.new_route, result.value().route.id);
    EXPECT_DOUBLE_EQ(record.cost, 20.0);
    EXPECT_EQ(record.constraints, 1U);
    EXPECT_EQ(record.reason, "task assigned");
    EXPECT_FALSE(record.failure.has_value());
    EXPECT_GT(record.expanded_nodes, 0U);
}

TEST(PlanningRecord, planning_record_of_a_failure_names_it) {
    Fixture fixture{test::island_map()};

    const RouteRequest request = request_for("A", "Z");
    const auto result = fixture.planner.plan_route(request);
    ASSERT_FALSE(result.has_value());

    const PlanningRecord record = make_planning_record(request, result, nullptr);

    ASSERT_TRUE(record.failure.has_value());
    EXPECT_EQ(*record.failure, RouteFailure::no_route);
    EXPECT_FALSE(record.new_route.has_value());
    EXPECT_FALSE(record.changed_route());
}

TEST(PlanningRecord, planning_record_of_a_kept_route_says_nothing_changed) {
    // The case a "did planning succeed" flag would get wrong. A replan can
    // succeed and deliberately leave the route alone (§16~17).
    AStarConfig config;
    config.stability.minimum_improvement = 0.99;

    map::Map graph = build(test::corridor_map());
    core::SimulationClock clock;
    const FreeFlowConditions conditions;
    const EuclideanHeuristic heuristic = make_admissible_heuristic(graph, config.weights);
    AStarPlanner planner{graph, clock, heuristic, conditions, config};

    RouteRequest detour = request_for("A", "B");
    detour.constraints.blocked_resources.insert(ResourceId{"CORRIDOR-01"});
    const auto current = planner.plan_route(detour);
    ASSERT_TRUE(current.has_value());

    const RouteRequest reopened = request_for("A", "B");
    const auto replanned = planner.replan_route(reopened, current.value().route);
    ASSERT_TRUE(replanned.has_value());

    const PlanningRecord record = make_planning_record(reopened, replanned, &current.value().route);

    ASSERT_TRUE(record.decision.has_value());
    EXPECT_EQ(*record.decision, ReplanDecision::kept_insufficient_improvement);
    EXPECT_FALSE(record.changed_route());
    EXPECT_EQ(record.old_route, record.new_route);
}

TEST(PlanningRecord, planning_record_of_an_adopted_route_says_it_changed) {
    Fixture fixture{test::corridor_map()};

    RouteRequest detour = request_for("A", "B");
    detour.constraints.blocked_resources.insert(ResourceId{"CORRIDOR-01"});
    const auto current = fixture.planner.plan_route(detour);
    ASSERT_TRUE(current.has_value());

    const RouteRequest reopened = request_for("A", "B");
    const auto replanned = fixture.planner.replan_route(reopened, current.value().route);
    ASSERT_TRUE(replanned.has_value());

    const PlanningRecord record = make_planning_record(reopened, replanned, &current.value().route);

    EXPECT_EQ(*record.decision, ReplanDecision::adopted_improved);
    EXPECT_TRUE(record.changed_route());
    EXPECT_NE(record.old_route, record.new_route);
}

TEST(PlanningRecord, planning_record_log_line_has_a_fixed_field_order) {
    // Two runs of a scenario are compared line by line to show it replayed
    // identically. A field order that varied would make identical runs look
    // different.
    Fixture fixture{test::line_map()};

    const RouteRequest request = request_for("A", "C");
    const auto result = fixture.planner.plan_route(request);
    ASSERT_TRUE(result.has_value());

    const std::string line = to_log_line(make_planning_record(request, result, nullptr));

    EXPECT_NE(line.find("robot=R01"), std::string::npos) << line;
    EXPECT_NE(line.find("start=A"), std::string::npos) << line;
    EXPECT_NE(line.find("goal=C"), std::string::npos) << line;
    EXPECT_NE(line.find("planner=astar"), std::string::npos) << line;
    EXPECT_NE(line.find("cost=20"), std::string::npos) << line;
    EXPECT_LT(line.find("robot="), line.find("goal=")) << line;
    EXPECT_LT(line.find("goal="), line.find("cost=")) << line;
}

TEST(PlanningRecord, planning_record_log_line_marks_absent_fields) {
    Fixture fixture{test::line_map()};

    const RouteRequest request = request_for("A", "C");
    const auto result = fixture.planner.plan_route(request);
    ASSERT_TRUE(result.has_value());

    const std::string line = to_log_line(make_planning_record(request, result, nullptr));

    // An empty field between two separators is harder to read than one that
    // says nothing is there.
    EXPECT_NE(line.find("old_route=-"), std::string::npos) << line;
    EXPECT_NE(line.find("reason=-"), std::string::npos) << line;
}

TEST(PlanningRecord, planning_record_log_line_names_the_failure) {
    Fixture fixture{test::island_map()};

    const RouteRequest request = request_for("A", "Z");
    const auto result = fixture.planner.plan_route(request);
    ASSERT_FALSE(result.has_value());

    const std::string line = to_log_line(make_planning_record(request, result, nullptr));

    EXPECT_NE(line.find("failure=NO_ROUTE"), std::string::npos) << line;
}

}  // namespace
}  // namespace traffic::planning
