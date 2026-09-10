/// A*. docs/05_GLOBAL_ROUTING.md §4, §13, §18, §19, §22, §25.
///
/// docs/05_GLOBAL_ROUTING.md §25 names test_astar_basic,
/// test_astar_shortest_path, test_dijkstra, test_unreachable_goal,
/// test_blocked_edge, test_blocked_node, test_congestion_cost,
/// test_waiting_cost, test_deterministic_route and test_replanning. They are
/// all here.
///
/// The two that carry the most weight are the direction test — a planner that
/// expands incident edges returns a route that looks right and cannot be
/// driven — and the determinism tests, because a route that varies between
/// runs makes every scenario in docs/26_TEST_SCENARIOS.md unreproducible.

#include "traffic/planning/astar_planner.h"

#include <chrono>
#include <cmath>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "planning_test_map.h"
#include "traffic/core/clock.h"
#include "traffic/core/ids.h"
#include "traffic/planning/cost_model.h"
#include "traffic/planning/heuristic.h"
#include "traffic/planning/route_request.h"

namespace traffic::planning {
namespace {

using core::EdgeId;
using core::NodeId;
using core::ResourceId;
using core::RobotId;
using test::build;

core::Duration seconds(int count) {
    return std::chrono::duration_cast<core::Duration>(core::Seconds{count});
}

/// Traffic conditions read from a table, so a test can say which resource is
/// busy and by how much.
class TabulatedConditions final : public ITrafficConditions {
public:
    void set_congestion(std::string resource, double value) {
        congestion_[ResourceId{std::move(resource)}] = value;
    }

    void set_wait(std::string resource, core::Duration value) {
        wait_[ResourceId{std::move(resource)}] = value;
    }

    [[nodiscard]] double congestion(const ResourceId& resource) const override {
        const auto found = congestion_.find(resource);
        return found == congestion_.end() ? 0.0 : found->second;
    }

    [[nodiscard]] core::Duration expected_wait(const ResourceId& resource,
                                               core::TimePoint /*at*/) const override {
        const auto found = wait_.find(resource);
        return found == wait_.end() ? core::Duration::zero() : found->second;
    }

private:
    std::map<ResourceId, double> congestion_;
    std::map<ResourceId, core::Duration> wait_;
};

/// A map, a clock, a heuristic and a planner, wired together.
struct Fixture {
    map::Map graph;
    core::SimulationClock clock;
    FreeFlowConditions free_flow;
    EuclideanHeuristic heuristic;
    AStarPlanner planner;

    Fixture(map::MapData data, AStarConfig config, ITrafficConditions* conditions)
        : graph(build(std::move(data))),
          heuristic(make_admissible_heuristic(graph, config.weights)),
          planner(graph,
                  clock,
                  heuristic,
                  conditions != nullptr ? *conditions : static_cast<ITrafficConditions&>(free_flow),
                  config) {}

    explicit Fixture(map::MapData data) : Fixture(std::move(data), AStarConfig{}, nullptr) {}
    Fixture(map::MapData data, AStarConfig config) : Fixture(std::move(data), config, nullptr) {}
};

RouteRequest request_for(std::string start, std::string goal) {
    RouteRequest request;
    request.robot_id = RobotId{"R01"};
    request.start_node = NodeId{std::move(start)};
    request.goal_node = NodeId{std::move(goal)};
    return request;
}

std::vector<std::string> node_ids(const domain::Route& route) {
    std::vector<std::string> ids;
    ids.reserve(route.nodes.size());
    for (const NodeId& node : route.nodes) {
        ids.push_back(node.value());
    }
    return ids;
}

std::vector<std::string> edge_ids(const domain::Route& route) {
    std::vector<std::string> ids;
    ids.reserve(route.segments.size());
    for (const domain::RouteSegment& segment : route.segments) {
        ids.push_back(segment.edge_id.value());
    }
    return ids;
}

// ===================================================================== basic

TEST(AStar, astar_plans_a_route_between_connected_nodes) {
    Fixture fixture{test::line_map()};

    const auto result = fixture.planner.plan_route(request_for("A", "C"));

    ASSERT_TRUE(result.has_value()) << to_string(result.error());
    EXPECT_EQ(node_ids(result.value().route), (std::vector<std::string>{"A", "B", "C"}));
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AB", "E-BC"}));
    EXPECT_DOUBLE_EQ(result.value().cost, 20.0);
}

TEST(AStar, astar_route_visits_one_more_node_than_it_has_segments) {
    Fixture fixture{test::line_map()};

    const auto result = fixture.planner.plan_route(request_for("A", "C"));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().route.nodes.size(), result.value().route.segments.size() + 1);
}

TEST(AStar, astar_route_reports_distance_and_estimated_time) {
    // 20 m at 1 m/s.
    Fixture fixture{test::line_map()};

    const auto result = fixture.planner.plan_route(request_for("A", "C"));

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result.value().total_distance, 20.0);
    EXPECT_EQ(result.value().estimated_time, seconds(20));
}

TEST(AStar, astar_route_records_the_map_it_was_planned_against) {
    // docs/23_SYSTEM_ARCHITECTURE.md §16: without this a route computed on an
    // old map can be committed against a new one.
    map::MapData data = test::line_map();
    data.version = domain::MapVersion{9};
    Fixture fixture{std::move(data)};

    const auto result = fixture.planner.plan_route(request_for("A", "C"));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().route.map_version, domain::MapVersion{9});
}

TEST(AStar, astar_route_segments_carry_contiguous_expected_windows) {
    // The timing is what makes temporal conflict detection possible at all:
    // two routes over one resource at disjoint times do not conflict, and
    // without windows they would look as though they did.
    Fixture fixture{test::line_map()};

    RouteRequest request = request_for("A", "C");
    request.current_time = core::kTimeOrigin + seconds(100);

    const auto result = fixture.planner.plan_route(request);

    ASSERT_TRUE(result.has_value());
    const auto& segments = result.value().route.segments;
    ASSERT_EQ(segments.size(), 2U);
    ASSERT_TRUE(segments[0].expected_window.has_value());
    ASSERT_TRUE(segments[1].expected_window.has_value());

    EXPECT_EQ(segments[0].expected_window->start(), core::kTimeOrigin + seconds(100));
    EXPECT_EQ(segments[0].expected_window->end(), core::kTimeOrigin + seconds(110));
    EXPECT_EQ(segments[1].expected_window->start(), core::kTimeOrigin + seconds(110));
    EXPECT_EQ(segments[1].expected_window->end(), core::kTimeOrigin + seconds(120));

    // Half-open, so back to back does not read as an overlap.
    EXPECT_FALSE(segments[0].expected_window->overlaps(*segments[1].expected_window));
}

TEST(AStar, astar_start_equal_to_goal_gives_a_route_with_no_segments) {
    // The robot is already there. The controller still needs something to
    // hold, so this is a route rather than a failure.
    Fixture fixture{test::line_map()};

    const auto result = fixture.planner.plan_route(request_for("B", "B"));

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(domain::is_trivial(result.value().route));
    EXPECT_EQ(result.value().route.nodes.size(), 1U);
    EXPECT_DOUBLE_EQ(result.value().cost, 0.0);
}

// ============================================================ shortest path

TEST(AStar, astar_returns_the_cheaper_of_two_routes) {
    Fixture fixture{test::two_route_map()};

    const auto result = fixture.planner.plan_route(request_for("A", "D"));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AB", "E-BD"}));
    EXPECT_DOUBLE_EQ(result.value().cost, 20.0);
}

TEST(AStar, astar_prefers_the_corridor_over_the_long_way_round) {
    Fixture fixture{test::corridor_map()};

    const auto result = fixture.planner.plan_route(request_for("A", "B"));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-C1", "E-C2", "E-C3"}));
    EXPECT_DOUBLE_EQ(result.value().cost, 30.0);
}

// ================================================================== failures

TEST(AStar, astar_unreachable_goal_is_reported_as_no_route) {
    // docs/05_GLOBAL_ROUTING.md §13. The controller decides between WAIT,
    // RETRY, ALTERNATIVE_GOAL and TASK_FAILED; the planner only says there is
    // no path.
    Fixture fixture{test::island_map()};

    const auto result = fixture.planner.plan_route(request_for("A", "Z"));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RouteFailure::no_route);
}

TEST(AStar, astar_unknown_start_and_goal_are_reported_apart) {
    Fixture fixture{test::line_map()};

    EXPECT_EQ(fixture.planner.plan_route(request_for("GHOST", "C")).error(),
              RouteFailure::unknown_start);
    EXPECT_EQ(fixture.planner.plan_route(request_for("A", "GHOST")).error(),
              RouteFailure::unknown_goal);
}

TEST(AStar, astar_rejects_a_request_with_an_empty_identifier) {
    Fixture fixture{test::line_map()};

    RouteRequest request = request_for("A", "C");
    request.robot_id = RobotId{};

    EXPECT_EQ(fixture.planner.plan_route(request).error(), RouteFailure::invalid_request);
}

TEST(AStar, astar_reports_a_blocked_start_apart_from_no_route) {
    // A robot standing on a blocked node cannot be helped by waiting. Folding
    // this into NO_ROUTE would have the controller retry forever.
    Fixture fixture{test::line_map()};

    RouteRequest request = request_for("A", "C");
    request.constraints.blocked_nodes.insert(NodeId{"A"});

    EXPECT_EQ(fixture.planner.plan_route(request).error(), RouteFailure::blocked_start);
}

TEST(AStar, astar_reports_a_blocked_goal_apart_from_no_route) {
    Fixture fixture{test::line_map()};

    RouteRequest request = request_for("A", "C");
    request.constraints.blocked_nodes.insert(NodeId{"C"});

    EXPECT_EQ(fixture.planner.plan_route(request).error(), RouteFailure::blocked_goal);
}

TEST(AStar, astar_search_limit_is_reported_apart_from_no_route) {
    // A path may well exist; the planner was not allowed to look far enough.
    // Calling that NO_ROUTE would fail a task that was achievable.
    AStarConfig config;
    config.max_expansions = 1;
    Fixture fixture{test::two_route_map(), config};

    const auto result = fixture.planner.plan_route(request_for("A", "D"));

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), RouteFailure::search_limit_reached);
    EXPECT_FALSE(is_retryable(result.error()));
}

// ================================================================ direction

TEST(AStar, astar_never_drives_a_one_way_edge_backwards) {
    // The route from C to B is C -> A -> B, not the single reversed E-BC. A
    // planner expanding incident edges answers with the reversed edge, and
    // the mistake only surfaces when the reservation is refused.
    Fixture fixture{test::one_way_map()};

    const auto result = fixture.planner.plan_route(request_for("C", "B"));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-CA", "E-AB"}));
    EXPECT_TRUE(fixture.planner.is_route_valid(result.value().route, RouteConstraints{}));
}

TEST(AStar, astar_goes_the_long_way_round_a_one_way_loop) {
    Fixture fixture{test::one_way_map()};

    // Nothing leads out of B except E-BC, and C leads only back to A.
    const auto result = fixture.planner.plan_route(request_for("B", "A"));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-BC", "E-CA"}));
}

// ============================================================== constraints

TEST(AStar, astar_blocked_edge_forces_the_other_route) {
    Fixture fixture{test::two_route_map()};

    RouteRequest request = request_for("A", "D");
    request.constraints.blocked_edges.insert(EdgeId{"E-AB"});

    const auto result = fixture.planner.plan_route(request);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AC", "E-CE", "E-ED"}));
}

TEST(AStar, astar_blocked_node_forces_the_other_route) {
    Fixture fixture{test::two_route_map()};

    RouteRequest request = request_for("A", "D");
    request.constraints.blocked_nodes.insert(NodeId{"B"});

    const auto result = fixture.planner.plan_route(request);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AC", "E-CE", "E-ED"}));
}

TEST(AStar, astar_blocking_a_corridor_closes_every_edge_in_it) {
    // One resource named, three edges shut. docs/00_MASTER_PLAN.md §4.2 — if
    // the corridor were modelled as three independent edges this would take
    // three constraints and one forgotten edge would leave it half open.
    Fixture fixture{test::corridor_map()};

    RouteRequest request = request_for("A", "B");
    request.constraints.blocked_resources.insert(ResourceId{"CORRIDOR-01"});

    const auto result = fixture.planner.plan_route(request);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AW", "E-WB"}));
}

TEST(AStar, astar_forbidden_resource_closes_it_the_same_way) {
    Fixture fixture{test::corridor_map()};

    RouteRequest request = request_for("A", "B");
    request.constraints.forbidden_resources.insert(ResourceId{"CORRIDOR-01"});

    const auto result = fixture.planner.plan_route(request);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AW", "E-WB"}));
}

TEST(AStar, astar_constraints_do_not_leak_between_requests) {
    // One planner serves the whole fleet. A resource forbidden to one robot
    // must not disappear for the next.
    Fixture fixture{test::corridor_map()};

    RouteRequest restricted = request_for("A", "B");
    restricted.constraints.blocked_resources.insert(ResourceId{"CORRIDOR-01"});
    const auto first = fixture.planner.plan_route(restricted);
    ASSERT_TRUE(first.has_value());

    const auto second = fixture.planner.plan_route(request_for("A", "B"));

    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(edge_ids(second.value().route), (std::vector<std::string>{"E-C1", "E-C2", "E-C3"}));
}

TEST(AStar, astar_takes_a_preferred_resource_when_the_discount_is_enough) {
    AStarConfig config;
    config.weights.preferred_factor = 0.4;
    Fixture fixture{test::two_route_map(), config};

    RouteRequest request = request_for("A", "D");
    // The long way round: 40 at full price, 16 discounted, against 20.
    request.constraints.preferred_resources.insert(ResourceId{"E-AC"});
    request.constraints.preferred_resources.insert(ResourceId{"E-CE"});
    request.constraints.preferred_resources.insert(ResourceId{"E-ED"});

    const auto result = fixture.planner.plan_route(request);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AC", "E-CE", "E-ED"}));
    EXPECT_DOUBLE_EQ(result.value().cost, 16.0);
}

// ============================================================ traffic costs

TEST(AStar, astar_congestion_cost_moves_the_route_off_the_busy_edges) {
    // docs/05_GLOBAL_ROUTING.md §9. The short route costs 20 and the long one
    // 40; congestion on the short route's edges makes it the expensive one.
    TabulatedConditions conditions;
    conditions.set_congestion("E-AB", 1.0);
    conditions.set_congestion("E-BD", 1.0);

    AStarConfig config;
    config.weights.congestion = 20.0;

    Fixture fixture{test::two_route_map(), config, &conditions};

    const auto result = fixture.planner.plan_route(request_for("A", "D"));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AC", "E-CE", "E-ED"}));
    EXPECT_DOUBLE_EQ(result.value().cost, 40.0);
}

TEST(AStar, astar_expected_waiting_cost_moves_the_route_too) {
    // docs/05_GLOBAL_ROUTING.md §10.
    TabulatedConditions conditions;
    conditions.set_wait("E-AB", seconds(15));
    conditions.set_wait("E-BD", seconds(15));

    AStarConfig config;
    config.weights.waiting = 2.0;

    Fixture fixture{test::two_route_map(), config, &conditions};

    const auto result = fixture.planner.plan_route(request_for("A", "D"));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AC", "E-CE", "E-ED"}));
}

TEST(AStar, astar_ignores_traffic_when_its_weight_is_zero) {
    // The default weights are pure shortest-distance, so congestion reported
    // by the conditions changes nothing until it is configured to.
    TabulatedConditions conditions;
    conditions.set_congestion("E-AB", 1.0);
    conditions.set_congestion("E-BD", 1.0);

    Fixture fixture{test::two_route_map(), AStarConfig{}, &conditions};

    const auto result = fixture.planner.plan_route(request_for("A", "D"));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AB", "E-BD"}));
}

// ================================================================= dijkstra

TEST(Dijkstra, dijkstra_finds_the_same_cost_as_astar) {
    // docs/05_GLOBAL_ROUTING.md §3 names Dijkstra as the fallback, and A* with
    // a zero heuristic is Dijkstra. Agreeing on cost is what says the
    // heuristic did not cost us optimality.
    const map::Map graph = build(test::two_route_map());
    core::SimulationClock clock;
    const FreeFlowConditions conditions;

    const EuclideanHeuristic euclidean = make_admissible_heuristic(graph, CostWeights{});
    const ZeroHeuristic zero;

    AStarPlanner astar{graph, clock, euclidean, conditions, AStarConfig{}};
    AStarPlanner dijkstra{graph, clock, zero, conditions, AStarConfig{}};

    const auto by_astar = astar.plan_route(request_for("A", "D"));
    const auto by_dijkstra = dijkstra.plan_route(request_for("A", "D"));

    ASSERT_TRUE(by_astar.has_value());
    ASSERT_TRUE(by_dijkstra.has_value());
    EXPECT_DOUBLE_EQ(by_astar.value().cost, by_dijkstra.value().cost);
    EXPECT_EQ(edge_ids(by_astar.value().route), edge_ids(by_dijkstra.value().route));
}

TEST(Dijkstra, dijkstra_expands_at_least_as_many_nodes_as_astar) {
    // What the heuristic is for. If A* ever expanded more, the estimate would
    // be doing nothing and the extra work would be pure loss.
    const map::Map graph = build(test::two_route_map());
    core::SimulationClock clock;
    const FreeFlowConditions conditions;

    const EuclideanHeuristic euclidean = make_admissible_heuristic(graph, CostWeights{});
    const ZeroHeuristic zero;

    AStarPlanner astar{graph, clock, euclidean, conditions, AStarConfig{}};
    AStarPlanner dijkstra{graph, clock, zero, conditions, AStarConfig{}};

    const auto by_astar = astar.plan_route(request_for("A", "D"));
    const auto by_dijkstra = dijkstra.plan_route(request_for("A", "D"));

    ASSERT_TRUE(by_astar.has_value());
    ASSERT_TRUE(by_dijkstra.has_value());
    EXPECT_GE(by_dijkstra.value().expanded_nodes, by_astar.value().expanded_nodes);
}

TEST(Dijkstra, dijkstra_reports_itself_as_the_planner_that_ran) {
    // docs/05_GLOBAL_ROUTING.md §23 records which algorithm produced a route.
    // The class is the same either way; what ran is not.
    const map::Map graph = build(test::line_map());
    core::SimulationClock clock;
    const FreeFlowConditions conditions;
    const ZeroHeuristic zero;

    AStarPlanner dijkstra{graph, clock, zero, conditions, AStarConfig{}};

    EXPECT_EQ(dijkstra.name(), "dijkstra");
    EXPECT_EQ(dijkstra.plan_route(request_for("A", "C")).value().planner, "dijkstra");
}

TEST(AStar, astar_reports_itself_as_the_planner_that_ran) {
    Fixture fixture{test::line_map()};

    EXPECT_EQ(fixture.planner.name(), "astar");
    EXPECT_EQ(fixture.planner.plan_route(request_for("A", "C")).value().planner, "astar");
}

// ============================================================== determinism

TEST(AStar, astar_returns_the_same_route_on_every_run) {
    // docs/01_REQUIREMENTS.md NFR-003. Two fresh planners, same map, same
    // request — down to the route id.
    Fixture first{test::two_route_map()};
    Fixture second{test::two_route_map()};

    const auto a = first.planner.plan_route(request_for("A", "D"));
    const auto b = second.planner.plan_route(request_for("A", "D"));

    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(node_ids(a.value().route), node_ids(b.value().route));
    EXPECT_EQ(edge_ids(a.value().route), edge_ids(b.value().route));
    EXPECT_DOUBLE_EQ(a.value().cost, b.value().cost);
    EXPECT_EQ(a.value().route.id, b.value().route.id);
}

TEST(AStar, astar_settles_equal_cost_routes_on_the_lower_edge_id) {
    // docs/05_GLOBAL_ROUTING.md §22: compare edge_id, then node_id. Both ways
    // round the symmetric map cost exactly 40, so nothing but the tie-breaker
    // decides — and it has to decide the same way every time.
    Fixture fixture{test::symmetric_map()};

    const auto result = fixture.planner.plan_route(request_for("A", "G"));

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(result.value().cost, 40.0);
    EXPECT_EQ(edge_ids(result.value().route), (std::vector<std::string>{"E-AL", "E-LG"}));
}

TEST(AStar, astar_settles_equal_cost_routes_the_same_way_repeatedly) {
    Fixture fixture{test::symmetric_map()};

    const auto first = fixture.planner.plan_route(request_for("A", "G"));
    const auto second = fixture.planner.plan_route(request_for("A", "G"));
    const auto third = fixture.planner.plan_route(request_for("A", "G"));

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(edge_ids(first.value().route), edge_ids(second.value().route));
    EXPECT_EQ(edge_ids(second.value().route), edge_ids(third.value().route));
}

TEST(AStar, astar_planning_time_does_not_vary_with_the_machine) {
    // Measured on the injected clock. Under a SimulationClock that is zero,
    // which is the point: a scenario must replay identically on a fast
    // machine and a slow one. Real latency is measured by the Phase 15
    // benchmark, outside the search.
    Fixture fixture{test::two_route_map()};

    const auto result = fixture.planner.plan_route(request_for("A", "D"));

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().planning_time, core::Duration::zero());
}

TEST(AStar, astar_route_ids_are_unique_within_a_planner) {
    Fixture fixture{test::line_map()};

    const auto first = fixture.planner.plan_route(request_for("A", "C"));
    const auto second = fixture.planner.plan_route(request_for("A", "C"));

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_NE(first.value().route.id, second.value().route.id);
}

// ============================================================ cost estimate

TEST(AStar, astar_estimate_matches_the_cost_of_the_route_it_planned) {
    Fixture fixture{test::two_route_map()};

    const auto result = fixture.planner.plan_route(request_for("A", "D"));

    ASSERT_TRUE(result.has_value());
    EXPECT_DOUBLE_EQ(fixture.planner.estimate_route_cost(
                         result.value().route, RouteConstraints{}, core::kTimeOrigin),
                     result.value().cost);
}

TEST(AStar, astar_estimate_of_a_route_with_an_unknown_edge_is_infinite) {
    // An undrivable route must never win a replan comparison by being cheap.
    Fixture fixture{test::line_map()};

    domain::Route route;
    route.id = core::RouteId{"ROUTE-X"};
    route.nodes = {NodeId{"A"}, NodeId{"B"}};
    domain::RouteSegment segment;
    segment.edge_id = EdgeId{"E-GHOST"};
    segment.from_node = NodeId{"A"};
    segment.to_node = NodeId{"B"};
    route.segments = {segment};

    const double cost =
        fixture.planner.estimate_route_cost(route, RouteConstraints{}, core::kTimeOrigin);

    EXPECT_TRUE(std::isinf(cost));
}

// ================================================================ replanning

TEST(AStar, astar_replan_adopts_a_clearly_better_route) {
    Fixture fixture{test::corridor_map()};

    // The robot is on the long way round, costing 90.
    RouteRequest detour = request_for("A", "B");
    detour.constraints.blocked_resources.insert(ResourceId{"CORRIDOR-01"});
    const auto current = fixture.planner.plan_route(detour);
    ASSERT_TRUE(current.has_value());
    ASSERT_DOUBLE_EQ(current.value().cost, 90.0);

    // The corridor reopens.
    const auto replanned =
        fixture.planner.replan_route(request_for("A", "B"), current.value().route);

    ASSERT_TRUE(replanned.has_value());
    EXPECT_EQ(replanned.value().replan_decision, ReplanDecision::adopted_improved);
    EXPECT_EQ(edge_ids(replanned.value().route),
              (std::vector<std::string>{"E-C1", "E-C2", "E-C3"}));
}

TEST(AStar, astar_replan_keeps_a_route_that_is_only_slightly_better) {
    // docs/05_GLOBAL_ROUTING.md §16. Re-reserving every resource on a route
    // costs more than a few percent of path length is worth.
    AStarConfig config;
    config.stability.minimum_improvement = 0.90;
    Fixture fixture{test::corridor_map(), config};

    RouteRequest detour = request_for("A", "B");
    detour.constraints.blocked_resources.insert(ResourceId{"CORRIDOR-01"});
    const auto current = fixture.planner.plan_route(detour);
    ASSERT_TRUE(current.has_value());

    const auto replanned =
        fixture.planner.replan_route(request_for("A", "B"), current.value().route);

    ASSERT_TRUE(replanned.has_value());
    EXPECT_EQ(replanned.value().replan_decision, ReplanDecision::kept_insufficient_improvement);
    EXPECT_EQ(replanned.value().route.id, current.value().route.id);
}

TEST(AStar, astar_replan_keeps_a_route_that_is_still_within_its_hold_time) {
    // docs/05_GLOBAL_ROUTING.md §17. The brake that stops A -> B -> A, which
    // the improvement threshold cannot see.
    AStarConfig config;
    config.stability.minimum_hold_time = seconds(60);
    Fixture fixture{test::corridor_map(), config};

    RouteRequest detour = request_for("A", "B");
    detour.constraints.blocked_resources.insert(ResourceId{"CORRIDOR-01"});
    const auto current = fixture.planner.plan_route(detour);
    ASSERT_TRUE(current.has_value());

    RouteRequest soon = request_for("A", "B");
    soon.current_time = current.value().route.created_at + seconds(5);

    const auto replanned = fixture.planner.replan_route(soon, current.value().route);

    ASSERT_TRUE(replanned.has_value());
    EXPECT_EQ(replanned.value().replan_decision, ReplanDecision::kept_within_hold_time);
    EXPECT_EQ(replanned.value().route.id, current.value().route.id);
}

TEST(AStar, astar_replan_adopts_once_the_hold_time_has_passed) {
    AStarConfig config;
    config.stability.minimum_hold_time = seconds(60);
    Fixture fixture{test::corridor_map(), config};

    RouteRequest detour = request_for("A", "B");
    detour.constraints.blocked_resources.insert(ResourceId{"CORRIDOR-01"});
    const auto current = fixture.planner.plan_route(detour);
    ASSERT_TRUE(current.has_value());

    RouteRequest later = request_for("A", "B");
    later.current_time = current.value().route.created_at + seconds(120);

    const auto replanned = fixture.planner.replan_route(later, current.value().route);

    ASSERT_TRUE(replanned.has_value());
    EXPECT_EQ(replanned.value().replan_decision, ReplanDecision::adopted_improved);
}

TEST(AStar, astar_replan_replaces_a_route_that_can_no_longer_be_driven) {
    // The ordering that matters: a route through a closed corridor is
    // replaced whatever the thresholds say.
    AStarConfig config;
    config.stability.minimum_improvement = 0.99;
    config.stability.minimum_hold_time = seconds(3600);
    Fixture fixture{test::corridor_map(), config};

    const auto current = fixture.planner.plan_route(request_for("A", "B"));
    ASSERT_TRUE(current.has_value());
    ASSERT_EQ(edge_ids(current.value().route), (std::vector<std::string>{"E-C1", "E-C2", "E-C3"}));

    RouteRequest closed = request_for("A", "B");
    closed.constraints.blocked_resources.insert(ResourceId{"CORRIDOR-01"});

    const auto replanned = fixture.planner.replan_route(closed, current.value().route);

    ASSERT_TRUE(replanned.has_value());
    EXPECT_EQ(replanned.value().replan_decision, ReplanDecision::adopted_current_invalid);
    EXPECT_EQ(edge_ids(replanned.value().route), (std::vector<std::string>{"E-AW", "E-WB"}));
}

TEST(AStar, astar_replan_without_an_alternative_reports_the_failure) {
    // No route at all is the controller's problem to solve (§13), not
    // something the planner papers over by keeping a broken route.
    Fixture fixture{test::island_map()};

    domain::Route current;
    current.id = core::RouteId{"ROUTE-OLD"};
    current.nodes = {NodeId{"A"}};

    const auto replanned = fixture.planner.replan_route(request_for("A", "Z"), current);

    ASSERT_FALSE(replanned.has_value());
    EXPECT_EQ(replanned.error(), RouteFailure::no_route);
}

TEST(AStar, astar_plan_route_records_no_replan_decision) {
    // There was nothing to keep, so there was no decision to make.
    Fixture fixture{test::line_map()};

    const auto result = fixture.planner.plan_route(request_for("A", "C"));

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().replan_decision.has_value());
}

}  // namespace
}  // namespace traffic::planning
