/// Heuristics. docs/05_GLOBAL_ROUTING.md §5.
///
/// The tests that matter are the admissibility ones. A heuristic that
/// overestimates does not make A* fail — it makes A* return a route that is
/// not the cheapest, with no indication that anything went wrong. So the bound
/// is asserted directly rather than inferred from the routes that come out.

#include "traffic/planning/heuristic.h"

#include <utility>

#include <gtest/gtest.h>

#include "planning_test_map.h"
#include "traffic/domain/values.h"
#include "traffic/map/map.h"
#include "traffic/planning/cost_model.h"

namespace traffic::planning {
namespace {

using domain::Position;
using test::build;

// ------------------------------------------------------------- distances

TEST(Distance, euclidean_distance_is_the_straight_line) {
    EXPECT_DOUBLE_EQ(euclidean_distance(Position{0.0, 0.0, 0.0}, Position{3.0, 4.0, 0.0}), 5.0);
}

TEST(Distance, euclidean_distance_of_a_point_to_itself_is_zero) {
    const Position point{7.0, -2.0, 1.0};
    EXPECT_DOUBLE_EQ(euclidean_distance(point, point), 0.0);
}

TEST(Distance, euclidean_distance_includes_the_vertical_axis) {
    // docs/03_MAP_GRAPH.md §4 keeps z for multi-floor sites. Ignoring it would
    // make two nodes on different floors look like the same place.
    EXPECT_DOUBLE_EQ(euclidean_distance(Position{0.0, 0.0, 0.0}, Position{0.0, 0.0, 6.0}), 6.0);
}

TEST(Distance, euclidean_distance_is_symmetric) {
    const Position a{1.0, 2.0, 3.0};
    const Position b{-4.0, 5.0, -6.0};
    EXPECT_DOUBLE_EQ(euclidean_distance(a, b), euclidean_distance(b, a));
}

TEST(Distance, manhattan_distance_sums_the_axes) {
    EXPECT_DOUBLE_EQ(manhattan_distance(Position{0.0, 0.0, 0.0}, Position{3.0, 4.0, 0.0}), 7.0);
}

TEST(Distance, manhattan_distance_is_never_below_euclidean) {
    // Which is exactly why it is not the default: on a diagonal edge it
    // exceeds the straight line, and an estimate above the truth is not
    // admissible.
    for (const double x : {0.0, 1.0, 3.0, -7.5}) {
        for (const double y : {0.0, 2.0, -4.0, 11.25}) {
            const Position from{0.0, 0.0, 0.0};
            const Position to{x, y, 0.0};
            EXPECT_GE(manhattan_distance(from, to), euclidean_distance(from, to));
        }
    }
}

// ------------------------------------------------------------ heuristics

TEST(ZeroHeuristic, zero_heuristic_estimates_nothing) {
    const ZeroHeuristic heuristic;
    EXPECT_DOUBLE_EQ(heuristic.estimate(Position{0.0, 0.0, 0.0}, Position{100.0, 100.0, 0.0}), 0.0);
}

TEST(ZeroHeuristic, zero_heuristic_is_named_for_the_algorithm_it_produces) {
    // docs/05_GLOBAL_ROUTING.md §3 names Dijkstra as the fallback. A* with a
    // zero heuristic is Dijkstra, so the log record should say so.
    const ZeroHeuristic heuristic;
    EXPECT_EQ(heuristic.name(), "dijkstra");
}

TEST(EuclideanHeuristic, euclidean_heuristic_scales_distance_into_cost) {
    const EuclideanHeuristic heuristic{2.0};
    EXPECT_DOUBLE_EQ(heuristic.estimate(Position{0.0, 0.0, 0.0}, Position{3.0, 4.0, 0.0}), 10.0);
}

TEST(EuclideanHeuristic, euclidean_heuristic_is_zero_at_the_goal) {
    // A heuristic that is non-zero at the goal makes A* stop expanding before
    // it has proved the route optimal.
    const EuclideanHeuristic heuristic{2.0};
    const Position goal{5.0, 5.0, 0.0};
    EXPECT_DOUBLE_EQ(heuristic.estimate(goal, goal), 0.0);
}

TEST(ManhattanHeuristic, manhattan_heuristic_scales_axis_distance_into_cost) {
    const ManhattanHeuristic heuristic{2.0};
    EXPECT_DOUBLE_EQ(heuristic.estimate(Position{0.0, 0.0, 0.0}, Position{3.0, 4.0, 0.0}), 14.0);
}

TEST(ManhattanHeuristic, manhattan_heuristic_overestimates_off_the_axes) {
    // The reason for decision D-008. On a diagonal the Manhattan estimate is
    // above the true straight-line cost, so it is not admissible on a map that
    // is not a grid.
    const ManhattanHeuristic manhattan{1.0};
    const EuclideanHeuristic euclidean{1.0};

    const Position from{0.0, 0.0, 0.0};
    const Position to{10.0, 10.0, 0.0};

    EXPECT_GT(manhattan.estimate(from, to), euclidean.estimate(from, to));
}

// ------------------------------------------------------------ map scaling

TEST(MaxSpeedLimit, max_speed_limit_is_the_fastest_edge) {
    map::MapData data = test::line_map();
    data.edges[0].speed_limit = 1.5;
    data.edges[1].speed_limit = 0.8;

    EXPECT_DOUBLE_EQ(max_speed_limit(build(std::move(data))), 1.5);
}

TEST(MaxSpeedLimit, max_speed_limit_counts_disabled_edges_too) {
    // A disabled edge can be switched back on without a new Map. A heuristic
    // that stopped being admissible the moment a corridor reopened would be a
    // very quiet way to lose optimality.
    map::MapData data = test::line_map();
    data.edges[0].speed_limit = 3.0;
    data.edges[0].enabled = false;

    EXPECT_DOUBLE_EQ(max_speed_limit(build(std::move(data))), 3.0);
}

TEST(AdmissibleHeuristic, admissible_heuristic_never_exceeds_the_real_route_cost) {
    // The property, end to end: on the two-route map the cheapest way from A
    // to D costs 20, and the estimate from A must not exceed it.
    const map::Map map = build(test::two_route_map());
    const CostWeights weights;
    const EuclideanHeuristic heuristic = make_admissible_heuristic(map, weights);

    const domain::Node* start = map.find_node(core::NodeId{"A"});
    const domain::Node* goal = map.find_node(core::NodeId{"D"});
    ASSERT_NE(start, nullptr);
    ASSERT_NE(goal, nullptr);

    constexpr double kCheapestRouteCost = 20.0;
    EXPECT_LE(heuristic.estimate(start->position, goal->position), kCheapestRouteCost);
}

TEST(AdmissibleHeuristic, admissible_heuristic_stays_bounded_with_a_time_weight) {
    map::MapData data = test::two_route_map();
    for (domain::Edge& e : data.edges) {
        e.speed_limit = 2.0;
    }
    const map::Map map = build(std::move(data));

    CostWeights weights;
    weights.travel_time = 4.0;  // 4 per second, and 2 m/s, so 2 per metre

    const EuclideanHeuristic heuristic = make_admissible_heuristic(map, weights);

    const domain::Node* start = map.find_node(core::NodeId{"A"});
    const domain::Node* goal = map.find_node(core::NodeId{"D"});
    ASSERT_NE(start, nullptr);
    ASSERT_NE(goal, nullptr);

    // 20 m of edges: 20 * (1 distance + 2 time) = 60.
    constexpr double kCheapestRouteCost = 60.0;
    EXPECT_LE(heuristic.estimate(start->position, goal->position), kCheapestRouteCost);
}

// -------------------------------------------------------------- geometry

TEST(Geometry, geometry_check_passes_on_the_shared_test_maps) {
    EXPECT_TRUE(geometry_supports_distance_heuristic(build(test::line_map())));
    EXPECT_TRUE(geometry_supports_distance_heuristic(build(test::two_route_map())));
    EXPECT_TRUE(geometry_supports_distance_heuristic(build(test::symmetric_map())));
    EXPECT_TRUE(geometry_supports_distance_heuristic(build(test::one_way_map())));
    EXPECT_TRUE(geometry_supports_distance_heuristic(build(test::corridor_map())));
}

TEST(Geometry, geometry_check_fails_when_an_edge_is_shorter_than_the_straight_line) {
    // Geometrically impossible, and trivially typed into a JSON file. When it
    // happens the straight-line estimate exceeds the real cost and A* quietly
    // stops being optimal — so the map should be planned with ZeroHeuristic.
    map::MapData data = test::line_map();
    data.edges[0].length = 1.0;  // A and B are 10 m apart

    EXPECT_FALSE(geometry_supports_distance_heuristic(build(std::move(data))));
}

TEST(Geometry, geometry_check_allows_an_edge_longer_than_the_straight_line) {
    // The normal case: a corridor bends, so driving it is further than the
    // straight line. That keeps the estimate below the truth, which is what
    // admissibility asks for.
    map::MapData data = test::line_map();
    data.edges[0].length = 25.0;

    EXPECT_TRUE(geometry_supports_distance_heuristic(build(std::move(data))));
}

}  // namespace
}  // namespace traffic::planning
