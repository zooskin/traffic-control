/// Route validation. docs/05_GLOBAL_ROUTING.md §20, §24.
///
/// docs/05_GLOBAL_ROUTING.md §25 names test_route_validation. These are it,
/// one per way a route can stop being drivable between being planned and being
/// driven.

#include "traffic/planning/route_validator.h"

#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "planning_test_map.h"
#include "traffic/core/ids.h"
#include "traffic/domain/route.h"
#include "traffic/map/map.h"
#include "traffic/planning/route_request.h"

namespace traffic::planning {
namespace {

using core::EdgeId;
using core::NodeId;
using test::build;

/// Builds a route over the named nodes and edges, without asking the map
/// whether any of it makes sense — which is the whole point of the thing under
/// test.
domain::Route route_over(std::vector<std::string> node_ids,
                         std::vector<std::string> edge_ids,
                         domain::MapVersion version = domain::MapVersion{1}) {
    domain::Route route;
    route.id = core::RouteId{"ROUTE-1"};
    route.robot_id = core::RobotId{"R01"};
    route.map_version = version;

    for (std::string& id : node_ids) {
        route.nodes.push_back(NodeId{std::move(id)});
    }

    for (std::size_t i = 0; i < edge_ids.size(); ++i) {
        domain::RouteSegment segment;
        segment.edge_id = EdgeId{edge_ids[i]};
        segment.from_node = route.nodes[i];
        segment.to_node = route.nodes[i + 1];
        segment.sequence = i;
        route.segments.push_back(segment);
    }
    return route;
}

// ------------------------------------------------------------- the happy path

TEST(RouteValidation, route_validation_accepts_a_route_the_map_supports) {
    const map::Map map = build(test::line_map());
    const domain::Route route = route_over({"A", "B", "C"}, {"E-AB", "E-BC"});

    const RouteValidation validation = validate_route(map, route, RouteConstraints{});

    EXPECT_TRUE(validation.is_valid()) << validation.to_string();
    EXPECT_TRUE(is_route_valid(map, route, RouteConstraints{}));
}

TEST(RouteValidation, route_validation_accepts_a_bidirectional_edge_driven_backwards) {
    const map::Map map = build(test::line_map());
    const domain::Route route = route_over({"C", "B", "A"}, {"E-BC", "E-AB"});

    EXPECT_TRUE(is_route_valid(map, route, RouteConstraints{}));
}

// ------------------------------------------------------------------ defects

TEST(RouteValidation, route_validation_rejects_an_empty_route) {
    const map::Map map = build(test::line_map());
    const domain::Route route;

    const RouteValidation validation = validate_route(map, route, RouteConstraints{});

    EXPECT_FALSE(validation.is_valid());
    EXPECT_TRUE(validation.has(RouteDefect::empty_route));
}

TEST(RouteValidation, route_validation_rejects_a_route_whose_counts_do_not_describe_a_walk) {
    const map::Map map = build(test::line_map());
    domain::Route route = route_over({"A", "B", "C"}, {"E-AB", "E-BC"});
    route.segments.pop_back();

    const RouteValidation validation = validate_route(map, route, RouteConstraints{});

    EXPECT_TRUE(validation.has(RouteDefect::inconsistent_shape));
}

TEST(RouteValidation, route_validation_rejects_an_unknown_node) {
    const map::Map map = build(test::line_map());
    const domain::Route route = route_over({"A", "GHOST"}, {"E-AB"});

    const RouteValidation validation = validate_route(map, route, RouteConstraints{});

    EXPECT_TRUE(validation.has(RouteDefect::unknown_node));
}

TEST(RouteValidation, route_validation_rejects_an_unknown_edge) {
    const map::Map map = build(test::line_map());
    const domain::Route route = route_over({"A", "B"}, {"E-GHOST"});

    const RouteValidation validation = validate_route(map, route, RouteConstraints{});

    EXPECT_TRUE(validation.has(RouteDefect::unknown_edge));
}

TEST(RouteValidation, route_validation_rejects_an_edge_that_does_not_join_its_nodes) {
    // The route claims to get from A to C in one step using the edge between A
    // and B. Connected on paper, undrivable in fact.
    const map::Map map = build(test::line_map());
    const domain::Route route = route_over({"A", "C"}, {"E-AB"});

    const RouteValidation validation = validate_route(map, route, RouteConstraints{});

    EXPECT_TRUE(validation.has(RouteDefect::edge_does_not_connect));
}

TEST(RouteValidation, route_validation_rejects_a_disabled_edge) {
    // docs/03_MAP_GRAPH.md: a closed corridor is disabled rather than deleted,
    // so the map version stays stable. Routes planned before it closed are
    // still holding the edge.
    map::MapData data = test::line_map();
    data.edges[0].enabled = false;
    const map::Map map = build(std::move(data));

    const domain::Route route = route_over({"A", "B"}, {"E-AB"});

    const RouteValidation validation = validate_route(map, route, RouteConstraints{});

    EXPECT_TRUE(validation.has(RouteDefect::edge_disabled));

    // And not also a direction violation. `is_traversable_from` reports false
    // for a disabled edge too, so reporting both would label every closed
    // corridor a one-way mistake.
    EXPECT_FALSE(validation.has(RouteDefect::wrong_direction));
}

TEST(RouteValidation, route_validation_rejects_driving_a_one_way_edge_backwards) {
    // The check that catches a planner expanding incident edges instead of
    // traversable ones. The walk is connected; one of its steps runs the wrong
    // way down a one-way corridor.
    const map::Map map = build(test::one_way_map());
    const domain::Route route = route_over({"C", "B"}, {"E-BC"});

    const RouteValidation validation = validate_route(map, route, RouteConstraints{});

    EXPECT_TRUE(validation.has(RouteDefect::wrong_direction));
    EXPECT_FALSE(validation.has(RouteDefect::edge_does_not_connect));
}

TEST(RouteValidation, route_validation_accepts_the_legal_way_round_a_one_way_loop) {
    const map::Map map = build(test::one_way_map());
    const domain::Route route = route_over({"C", "A", "B"}, {"E-CA", "E-AB"});

    EXPECT_TRUE(is_route_valid(map, route, RouteConstraints{}));
}

// -------------------------------------------------------------- constraints

TEST(RouteValidation, route_validation_rejects_a_blocked_node) {
    const map::Map map = build(test::line_map());
    const domain::Route route = route_over({"A", "B", "C"}, {"E-AB", "E-BC"});

    RouteConstraints constraints;
    constraints.blocked_nodes.insert(NodeId{"B"});

    EXPECT_TRUE(validate_route(map, route, constraints).has(RouteDefect::blocked_node));
}

TEST(RouteValidation, route_validation_rejects_a_blocked_edge) {
    const map::Map map = build(test::line_map());
    const domain::Route route = route_over({"A", "B", "C"}, {"E-AB", "E-BC"});

    RouteConstraints constraints;
    constraints.blocked_edges.insert(EdgeId{"E-BC"});

    EXPECT_TRUE(validate_route(map, route, constraints).has(RouteDefect::blocked_edge));
}

TEST(RouteValidation, route_validation_rejects_a_route_through_a_blocked_corridor) {
    // One blocked resource, three edges invalidated. Naming the corridor once
    // is the only way to close it without missing an edge.
    const map::Map map = build(test::corridor_map());
    const domain::Route route = route_over({"A", "M1", "M2", "B"}, {"E-C1", "E-C2", "E-C3"});

    RouteConstraints constraints;
    constraints.blocked_resources.insert(core::ResourceId{"CORRIDOR-01"});

    const RouteValidation validation = validate_route(map, route, constraints);

    EXPECT_TRUE(validation.has(RouteDefect::blocked_resource));
    EXPECT_EQ(validation.issues().size(), 3U) << validation.to_string();
}

TEST(RouteValidation, route_validation_rejects_a_route_through_a_forbidden_resource) {
    const map::Map map = build(test::corridor_map());
    const domain::Route route = route_over({"A", "M1", "M2", "B"}, {"E-C1", "E-C2", "E-C3"});

    RouteConstraints constraints;
    constraints.forbidden_resources.insert(core::ResourceId{"CORRIDOR-01"});

    EXPECT_TRUE(validate_route(map, route, constraints).has(RouteDefect::blocked_resource));
}

TEST(RouteValidation, route_validation_accepts_a_route_that_avoids_the_blocked_corridor) {
    const map::Map map = build(test::corridor_map());
    const domain::Route route = route_over({"A", "W", "B"}, {"E-AW", "E-WB"});

    RouteConstraints constraints;
    constraints.blocked_resources.insert(core::ResourceId{"CORRIDOR-01"});

    EXPECT_TRUE(is_route_valid(map, route, constraints));
}

// ------------------------------------------------------------------- staleness

TEST(RouteValidation, route_validation_rejects_a_route_planned_against_an_older_map) {
    // docs/23_SYSTEM_ARCHITECTURE.md §16: caught before the route is
    // committed, not after the robot has driven it.
    map::MapData data = test::line_map();
    data.version = domain::MapVersion{7};
    const map::Map map = build(std::move(data));

    const domain::Route route = route_over({"A", "B"}, {"E-AB"}, domain::MapVersion{6});

    EXPECT_TRUE(validate_route(map, route, RouteConstraints{}).has(RouteDefect::stale_map_version));
}

TEST(RouteValidation, route_validation_accepts_a_route_planned_against_the_current_map) {
    map::MapData data = test::line_map();
    data.version = domain::MapVersion{7};
    const map::Map map = build(std::move(data));

    const domain::Route route = route_over({"A", "B"}, {"E-AB"}, domain::MapVersion{7});

    EXPECT_TRUE(is_route_valid(map, route, RouteConstraints{}));
}

// -------------------------------------------------------------- reporting

TEST(RouteValidation, route_validation_reports_every_defect_not_only_the_first) {
    // A controller choosing between WAIT and REPLAN (§13) needs to know
    // everything that is wrong, not the first thing found.
    map::MapData data = test::line_map();
    data.edges[0].enabled = false;
    const map::Map map = build(std::move(data));

    const domain::Route route = route_over({"A", "B", "C"}, {"E-AB", "E-BC"});

    RouteConstraints constraints;
    constraints.blocked_nodes.insert(NodeId{"C"});
    constraints.blocked_edges.insert(EdgeId{"E-BC"});

    const RouteValidation validation = validate_route(map, route, constraints);

    EXPECT_TRUE(validation.has(RouteDefect::edge_disabled));
    EXPECT_TRUE(validation.has(RouteDefect::blocked_node));
    EXPECT_TRUE(validation.has(RouteDefect::blocked_edge));
    EXPECT_EQ(validation.issues().size(), 3U) << validation.to_string();
}

TEST(RouteValidation, route_validation_names_the_element_at_fault) {
    const map::Map map = build(test::line_map());
    const domain::Route route = route_over({"A", "B"}, {"E-GHOST"});

    const RouteValidation validation = validate_route(map, route, RouteConstraints{});

    ASSERT_EQ(validation.issues().size(), 1U);
    EXPECT_EQ(validation.issues().front().subject, "E-GHOST");
    EXPECT_EQ(validation.issues().front().segment, 0U);
}

TEST(RouteDefectName, route_defect_every_value_has_a_name) {
    for (const RouteDefect defect : {RouteDefect::empty_route,
                                     RouteDefect::inconsistent_shape,
                                     RouteDefect::unknown_node,
                                     RouteDefect::unknown_edge,
                                     RouteDefect::edge_does_not_connect,
                                     RouteDefect::edge_disabled,
                                     RouteDefect::wrong_direction,
                                     RouteDefect::blocked_node,
                                     RouteDefect::blocked_edge,
                                     RouteDefect::blocked_resource,
                                     RouteDefect::stale_map_version}) {
        EXPECT_NE(to_string(defect), "UNKNOWN");
    }
}

}  // namespace
}  // namespace traffic::planning
