/// Batch planning. docs/24_DOMAIN_MODEL.md §20~21.

#include "traffic/planning/sequential_fleet_planner.h"

#include <chrono>
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
using core::RobotId;
using test::build;

core::Duration seconds(int count) {
    return std::chrono::duration_cast<core::Duration>(core::Seconds{count});
}

RouteRequest route_request(std::string robot, std::string start, std::string goal) {
    RouteRequest request;
    request.robot_id = RobotId{std::move(robot)};
    request.start_node = NodeId{std::move(start)};
    request.goal_node = NodeId{std::move(goal)};
    return request;
}

/// A clock that steps forward every time it is read.
///
/// The batch checks its budget between robots, so a clock that never moves
/// could never time one out. Stepping by a fixed amount keeps that testable
/// without making the result depend on how fast the machine is.
class TickingClock final : public core::IClock {
public:
    explicit TickingClock(core::Duration step) : step_(step) {}

    [[nodiscard]] core::TimePoint now() const override {
        const core::TimePoint current = now_;
        now_ += step_;
        return current;
    }

private:
    core::Duration step_;
    mutable core::TimePoint now_{core::kTimeOrigin};
};

/// A planner, a map and a batch planner over them.
struct Fleet {
    map::Map graph;
    core::SimulationClock clock;
    FreeFlowConditions conditions;
    EuclideanHeuristic heuristic;
    AStarPlanner planner;
    SequentialFleetPlanner fleet;

    explicit Fleet(map::MapData data)
        : graph(build(std::move(data))),
          heuristic(make_admissible_heuristic(graph, CostWeights{})),
          planner(graph, clock, heuristic, conditions, AStarConfig{}),
          fleet(planner, clock) {}
};

TEST(FleetPlanner, fleet_planner_plans_every_robot_in_the_batch) {
    Fleet fleet{test::two_route_map()};

    PlanningRequest request;
    request.request_id = core::PlanningRequestId{"PLAN-1"};
    request.routes = {route_request("R01", "A", "D"),
                      route_request("R02", "C", "B"),
                      route_request("R03", "E", "A")};

    const PlanningResult result = fleet.fleet.plan(request);

    EXPECT_EQ(result.status, PlanningStatus::success);
    EXPECT_EQ(result.request_id, core::PlanningRequestId{"PLAN-1"});
    EXPECT_EQ(succeeded_count(result), 3U);
}

TEST(FleetPlanner, fleet_planner_keeps_the_request_order) {
    // Deterministic output for a deterministic input. A batch that came back
    // in a different order each run would make every scenario comparison in
    // docs/26_TEST_SCENARIOS.md useless.
    Fleet fleet{test::two_route_map()};

    PlanningRequest request;
    request.routes = {route_request("R03", "A", "D"),
                      route_request("R01", "A", "D"),
                      route_request("R02", "A", "D")};

    const PlanningResult result = fleet.fleet.plan(request);

    ASSERT_EQ(result.outcomes.size(), 3U);
    EXPECT_EQ(result.outcomes[0].robot_id, RobotId{"R03"});
    EXPECT_EQ(result.outcomes[1].robot_id, RobotId{"R01"});
    EXPECT_EQ(result.outcomes[2].robot_id, RobotId{"R02"});
}

TEST(FleetPlanner, fleet_planner_keeps_going_after_a_robot_that_cannot_be_routed) {
    // One unreachable goal must not stall two hundred robots.
    Fleet fleet{test::island_map()};

    PlanningRequest request;
    request.routes = {route_request("R01", "A", "B"),
                      route_request("R02", "A", "Z"),
                      route_request("R03", "Z", "Y")};

    const PlanningResult result = fleet.fleet.plan(request);

    EXPECT_EQ(result.status, PlanningStatus::no_path);
    EXPECT_EQ(succeeded_count(result), 2U);
    ASSERT_EQ(result.outcomes.size(), 3U);
    EXPECT_EQ(result.outcomes[1].failure, RouteFailure::no_route);
    EXPECT_TRUE(result.outcomes[0].succeeded());
    EXPECT_TRUE(result.outcomes[2].succeeded());
}

TEST(FleetPlanner, fleet_planner_reports_a_malformed_request_as_failed) {
    Fleet fleet{test::line_map()};

    PlanningRequest request;
    request.routes = {route_request("R01", "A", "C"), route_request("R02", "A", "GHOST")};

    const PlanningResult result = fleet.fleet.plan(request);

    EXPECT_EQ(result.status, PlanningStatus::failed);
    EXPECT_EQ(succeeded_count(result), 1U);
}

TEST(FleetPlanner, fleet_planner_carries_the_versions_it_was_asked_against) {
    // docs/23_SYSTEM_ARCHITECTURE.md §2.4: a plan computed on state 100 must
    // not be applied to a world that has moved to 105, and the result has to
    // say which it was computed on.
    Fleet fleet{test::line_map()};

    PlanningRequest request;
    request.map_version = domain::MapVersion{3};
    request.traffic_state_version = domain::StateVersion{100};
    request.routes = {route_request("R01", "A", "C")};

    const PlanningResult result = fleet.fleet.plan(request);

    EXPECT_EQ(result.map_version, domain::MapVersion{3});
    EXPECT_EQ(result.traffic_state_version, domain::StateVersion{100});
}

TEST(FleetPlanner, fleet_planner_empty_batch_succeeds) {
    Fleet fleet{test::line_map()};

    const PlanningResult result = fleet.fleet.plan(PlanningRequest{});

    EXPECT_EQ(result.status, PlanningStatus::success);
    EXPECT_TRUE(result.outcomes.empty());
}

TEST(FleetPlanner, fleet_planner_reports_the_algorithm_it_wraps) {
    // What §23 wants recorded is which algorithm produced the routes.
    // "sequential" would not answer that.
    Fleet fleet{test::line_map()};

    EXPECT_EQ(fleet.fleet.name(), "astar");
}

TEST(FleetPlanner, fleet_planner_stops_at_the_deadline_and_keeps_what_it_planned) {
    const map::Map graph = build(test::line_map());
    TickingClock clock{seconds(10)};
    const FreeFlowConditions conditions;
    const EuclideanHeuristic heuristic = make_admissible_heuristic(graph, CostWeights{});

    AStarPlanner planner{graph, clock, heuristic, conditions, AStarConfig{}};
    SequentialFleetPlanner fleet{planner, clock};

    PlanningRequest request;
    request.deadline = core::kTimeOrigin + seconds(25);
    request.routes = {route_request("R01", "A", "C"),
                      route_request("R02", "A", "C"),
                      route_request("R03", "A", "C"),
                      route_request("R04", "A", "C")};

    const PlanningResult result = fleet.plan(request);

    EXPECT_EQ(result.status, PlanningStatus::timeout);

    // The robots already planned keep their routes — throwing away good work
    // because the batch ran long helps nobody.
    EXPECT_GT(succeeded_count(result), 0U);
    EXPECT_LT(succeeded_count(result), 4U);

    // And every robot asked about is accounted for, so the caller can tell
    // which ones still need planning.
    EXPECT_EQ(result.outcomes.size(), 4U);
    EXPECT_EQ(result.outcomes.back().robot_id, RobotId{"R04"});
    EXPECT_FALSE(result.outcomes.back().succeeded());
    EXPECT_FALSE(result.outcomes.back().failure.has_value());
}

TEST(FleetPlanner, fleet_planner_stops_at_the_timeout_as_well_as_the_deadline) {
    // §20 gives both. A deadline is a point on the timeline, a timeout a span
    // from when the batch started; a caller may set either.
    const map::Map graph = build(test::line_map());
    TickingClock clock{seconds(10)};
    const FreeFlowConditions conditions;
    const EuclideanHeuristic heuristic = make_admissible_heuristic(graph, CostWeights{});

    AStarPlanner planner{graph, clock, heuristic, conditions, AStarConfig{}};
    SequentialFleetPlanner fleet{planner, clock};

    PlanningRequest request;
    request.timeout = seconds(25);
    request.routes = {route_request("R01", "A", "C"),
                      route_request("R02", "A", "C"),
                      route_request("R03", "A", "C"),
                      route_request("R04", "A", "C")};

    EXPECT_EQ(fleet.plan(request).status, PlanningStatus::timeout);
}

TEST(FleetPlanner, fleet_planner_without_a_budget_plans_everything) {
    const map::Map graph = build(test::line_map());
    TickingClock clock{seconds(10)};
    const FreeFlowConditions conditions;
    const EuclideanHeuristic heuristic = make_admissible_heuristic(graph, CostWeights{});

    AStarPlanner planner{graph, clock, heuristic, conditions, AStarConfig{}};
    SequentialFleetPlanner fleet{planner, clock};

    PlanningRequest request;
    request.routes = {route_request("R01", "A", "C"),
                      route_request("R02", "A", "C"),
                      route_request("R03", "A", "C"),
                      route_request("R04", "A", "C")};

    const PlanningResult result = fleet.plan(request);

    EXPECT_EQ(result.status, PlanningStatus::success);
    EXPECT_EQ(succeeded_count(result), 4U);
}

}  // namespace
}  // namespace traffic::planning
