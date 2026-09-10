/// Tests for the traffic map and its validation.
///
/// Naming follows docs/20_CODING_GUIDELINES.md §45. Covers the queries of
/// docs/03_MAP_GRAPH.md §18~20 and the checks of §17, which §25 names as
/// test_map_validation, test_duplicate_node, test_invalid_edge,
/// test_reachability, test_neighbor_query and test_resource_query.

#include "traffic/map/map.h"

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/domain/graph.h"
#include "traffic/domain/resources.h"
#include "traffic/map/map_validator.h"

namespace traffic::map {
namespace {

using core::EdgeId;
using core::NodeId;
using core::ResourceId;
using domain::Edge;
using domain::EdgeDirection;
using domain::Node;
using domain::NodeType;

[[nodiscard]] Node node(std::string id, NodeType type = NodeType::normal) {
    Node n;
    n.id = NodeId{std::move(id)};
    n.type = type;
    n.capacity = 1;
    return n;
}

[[nodiscard]] Edge edge(std::string id,
                        std::string from,
                        std::string to,
                        EdgeDirection direction = EdgeDirection::bidirectional) {
    Edge e;
    e.id = EdgeId{std::move(id)};
    e.from_node = NodeId{std::move(from)};
    e.to_node = NodeId{std::move(to)};
    e.length = 10.0;
    e.width = 1.5;
    e.speed_limit = 1.0;
    e.direction = direction;
    e.capacity = 1;
    return e;
}

/// A ---E1--- B ---E2--- C, all bidirectional.
[[nodiscard]] MapData line_map() {
    MapData data;
    data.map_id = "line";
    data.version = domain::MapVersion{1};
    data.nodes = {node("A"), node("B"), node("C")};
    data.edges = {edge("E1", "A", "B"), edge("E2", "B", "C")};
    return data;
}

[[nodiscard]] Map built(MapData data) {
    auto result = make_map(std::move(data));
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().to_string());
    return std::move(result).value();
}

// =================================================================== lookups

TEST(Map, map_reports_its_identity_and_version) {
    const Map map = built(line_map());
    EXPECT_EQ(map.id(), "line");
    EXPECT_EQ(map.version(), domain::MapVersion{1});
    EXPECT_EQ(map.node_count(), 3U);
    EXPECT_EQ(map.edge_count(), 2U);
}

TEST(Map, map_finds_a_node_by_id) {
    const Map map = built(line_map());

    const Node* found = map.find_node(NodeId{"B"});
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->id, NodeId{"B"});

    EXPECT_EQ(map.find_node(NodeId{"Z"}), nullptr);
    EXPECT_TRUE(map.has_node(NodeId{"A"}));
    EXPECT_FALSE(map.has_node(NodeId{"Z"}));
}

TEST(Map, map_finds_an_edge_by_id) {
    const Map map = built(line_map());

    const Edge* found = map.find_edge(EdgeId{"E1"});
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->from_node, NodeId{"A"});

    EXPECT_EQ(map.find_edge(EdgeId{"ZZ"}), nullptr);
    EXPECT_TRUE(map.has_edge(EdgeId{"E2"}));
}

// ================================================================ neighbours

TEST(Map, map_lists_incident_edges) {
    const Map map = built(line_map());

    const auto at_b = map.incident_edges(NodeId{"B"});
    ASSERT_EQ(at_b.size(), 2U);

    const auto at_a = map.incident_edges(NodeId{"A"});
    EXPECT_EQ(at_a.size(), 1U);
}

TEST(Map, map_incident_edges_of_an_unknown_node_is_empty) {
    const Map map = built(line_map());
    EXPECT_TRUE(map.incident_edges(NodeId{"Z"}).empty());
}

TEST(Map, map_lists_neighbours) {
    const Map map = built(line_map());

    const auto from_b = map.neighbours(NodeId{"B"});
    ASSERT_EQ(from_b.size(), 2U);
    EXPECT_NE(std::find(from_b.begin(), from_b.end(), NodeId{"A"}), from_b.end());
    EXPECT_NE(std::find(from_b.begin(), from_b.end(), NodeId{"C"}), from_b.end());
}

/// A planner that expands incident edges rather than traversable ones will
/// route the wrong way down a one-way corridor and only find out when the
/// reservation is refused.
TEST(Map, map_traversable_edges_respect_direction) {
    MapData data = line_map();
    data.edges[0].direction = EdgeDirection::forward;  // A -> B only
    const Map map = built(std::move(data));

    EXPECT_EQ(map.traversable_edges(NodeId{"A"}).size(), 1U);
    EXPECT_TRUE(map.traversable_edges(NodeId{"B"}).empty() ||
                map.traversable_edges(NodeId{"B"}).size() == 1U);

    const auto from_b = map.neighbours(NodeId{"B"});
    EXPECT_EQ(std::find(from_b.begin(), from_b.end(), NodeId{"A"}), from_b.end())
        << "B cannot go back to A along a forward-only edge";
}

TEST(Map, map_traversable_edges_skip_disabled_edges) {
    MapData data = line_map();
    data.edges[0].enabled = false;
    const Map map = built(std::move(data));

    EXPECT_TRUE(map.traversable_edges(NodeId{"A"}).empty());
    EXPECT_EQ(map.incident_edges(NodeId{"A"}).size(), 1U)
        << "the edge still exists; it is only closed to traffic";
}

/// docs/20_CODING_GUIDELINES.md §23. A planner that expands neighbours in a
/// hash-dependent order does not produce the same route twice.
TEST(Map, map_neighbour_order_is_deterministic) {
    const MapData data = line_map();

    const auto first = built(data).neighbours(NodeId{"B"});
    for (int i = 0; i < 20; ++i) {
        EXPECT_EQ(built(data).neighbours(NodeId{"B"}), first);
    }
}

// ============================================================== reachability

TEST(Map, map_reachability_finds_a_connected_goal) {
    const Map map = built(line_map());

    EXPECT_TRUE(map.is_reachable(NodeId{"A"}, NodeId{"C"}));
    EXPECT_TRUE(map.is_reachable(NodeId{"C"}, NodeId{"A"}));
    EXPECT_TRUE(map.is_reachable(NodeId{"A"}, NodeId{"A"})) << "a node reaches itself";
}

TEST(Map, map_reachability_rejects_unknown_nodes) {
    const Map map = built(line_map());
    EXPECT_FALSE(map.is_reachable(NodeId{"A"}, NodeId{"Z"}));
    EXPECT_FALSE(map.is_reachable(NodeId{"Z"}, NodeId{"A"}));
}

/// A one-way graph must not be reported as symmetric, or the planner will
/// promise a return trip that cannot be made.
TEST(Map, map_reachability_respects_one_way_edges) {
    MapData data = line_map();
    data.edges[0].direction = EdgeDirection::forward;
    data.edges[1].direction = EdgeDirection::forward;
    const Map map = built(std::move(data));

    EXPECT_TRUE(map.is_reachable(NodeId{"A"}, NodeId{"C"}));
    EXPECT_FALSE(map.is_reachable(NodeId{"C"}, NodeId{"A"}));
}

TEST(Map, map_reachable_from_lists_the_component) {
    const Map map = built(line_map());

    const auto reached = map.reachable_from(NodeId{"A"});
    EXPECT_EQ(reached.size(), 3U);
    EXPECT_EQ(reached.front(), NodeId{"A"}) << "the start is included first";
}

TEST(Map, map_reachable_from_an_unknown_node_is_empty) {
    const Map map = built(line_map());
    EXPECT_TRUE(map.reachable_from(NodeId{"Z"}).empty());
}

// ================================================================= resources

/// docs/00_MASTER_PLAN.md §4.2: several edges through one corridor share a
/// resource, and it is the resource that admits one robot at a time.
TEST(Map, map_maps_edges_to_their_corridor_resource) {
    MapData data = line_map();
    data.edges[0].resource_id = ResourceId{"CORRIDOR-01"};
    data.edges[1].resource_id = ResourceId{"CORRIDOR-01"};

    auto corridor = domain::make_corridor(ResourceId{"CORRIDOR-01"},
                                          NodeId{"A"},
                                          NodeId{"C"},
                                          {EdgeId{"E1"}, EdgeId{"E2"}},
                                          1,
                                          EdgeDirection::bidirectional);
    ASSERT_TRUE(corridor.has_value());
    data.corridors = {corridor.value()};

    const Map map = built(std::move(data));

    EXPECT_EQ(map.resource_for_edge(EdgeId{"E1"}), ResourceId{"CORRIDOR-01"});
    EXPECT_EQ(map.resource_for_edge(EdgeId{"E2"}), ResourceId{"CORRIDOR-01"});

    const auto member_edges = map.edges_for_resource(ResourceId{"CORRIDOR-01"});
    EXPECT_EQ(member_edges.size(), 2U);

    ASSERT_NE(map.find_corridor(ResourceId{"CORRIDOR-01"}), nullptr);
    EXPECT_TRUE(domain::can_deadlock_head_on(*map.find_corridor(ResourceId{"CORRIDOR-01"})));
}

TEST(Map, map_edge_without_a_corridor_is_its_own_resource) {
    const Map map = built(line_map());
    EXPECT_EQ(map.resource_for_edge(EdgeId{"E1"}), ResourceId{"E1"});
    EXPECT_EQ(map.edges_for_resource(ResourceId{"E1"}).size(), 1U);
}

TEST(Map, map_resource_for_an_unknown_edge_is_empty) {
    const Map map = built(line_map());
    EXPECT_TRUE(map.resource_for_edge(EdgeId{"ZZ"}).empty());
    EXPECT_TRUE(map.edges_for_resource(ResourceId{"NOPE"}).empty());
}

TEST(Map, map_finds_a_node_resource) {
    MapData data = line_map();
    data.nodes[1].resource_id = ResourceId{"X1"};
    const Map map = built(std::move(data));

    EXPECT_EQ(map.resource_for_node(NodeId{"B"}), ResourceId{"X1"});
    EXPECT_FALSE(map.resource_for_node(NodeId{"A"}).has_value());
}

TEST(Map, map_finds_waiting_bays_and_intersections) {
    MapData data = line_map();

    auto bay = domain::make_waiting_bay(ResourceId{"BAY-01"}, NodeId{"C"}, 1);
    ASSERT_TRUE(bay.has_value());
    data.waiting_bays = {bay.value()};

    auto crossing = domain::make_intersection(ResourceId{"X1"}, {NodeId{"B"}}, {}, 1);
    ASSERT_TRUE(crossing.has_value());
    data.intersections = {crossing.value()};

    const Map map = built(std::move(data));

    EXPECT_NE(map.find_waiting_bay(ResourceId{"BAY-01"}), nullptr);
    EXPECT_NE(map.find_intersection(ResourceId{"X1"}), nullptr);
    EXPECT_EQ(map.find_waiting_bay(ResourceId{"NOPE"}), nullptr);
}

// ================================================================ validation

TEST(MapValidation, validation_accepts_a_well_formed_map) {
    const auto report = validate(line_map());
    EXPECT_TRUE(report.is_valid()) << report.to_string();
    EXPECT_EQ(report.error_count(), 0U);
}

TEST(MapValidation, validation_rejects_a_duplicate_node_id) {
    MapData data = line_map();
    data.nodes.push_back(node("B"));

    const auto report = validate(data);
    EXPECT_FALSE(report.is_valid());
    EXPECT_EQ(report.issues().front().kind, IssueKind::duplicate_node_id);
}

TEST(MapValidation, validation_rejects_a_duplicate_edge_id) {
    MapData data = line_map();
    data.edges.push_back(edge("E1", "A", "C"));

    const auto report = validate(data);
    EXPECT_FALSE(report.is_valid());
}

/// A reservation names a resource without saying what kind it is, so two kinds
/// sharing an id are indistinguishable at the moment it matters.
TEST(MapValidation, validation_rejects_a_resource_id_reused_across_kinds) {
    MapData data = line_map();

    auto corridor = domain::make_corridor(
        ResourceId{"R1"}, NodeId{"A"}, NodeId{"B"}, {EdgeId{"E1"}}, 1, EdgeDirection::forward);
    ASSERT_TRUE(corridor.has_value());
    data.corridors = {corridor.value()};

    auto bay = domain::make_waiting_bay(ResourceId{"R1"}, NodeId{"C"}, 1);
    ASSERT_TRUE(bay.has_value());
    data.waiting_bays = {bay.value()};

    const auto report = validate(data);
    EXPECT_FALSE(report.is_valid());
}

TEST(MapValidation, validation_rejects_an_edge_pointing_at_a_missing_node) {
    MapData data = line_map();
    data.edges.push_back(edge("E3", "C", "GHOST"));

    const auto report = validate(data);
    EXPECT_FALSE(report.is_valid());
    EXPECT_EQ(report.issues().front().kind, IssueKind::dangling_edge_endpoint);
}

TEST(MapValidation, validation_rejects_invalid_edge_measurements) {
    MapData bad_length = line_map();
    bad_length.edges[0].length = 0.0;
    EXPECT_FALSE(validate(bad_length).is_valid());

    MapData bad_speed = line_map();
    bad_speed.edges[0].speed_limit = -1.0;
    EXPECT_FALSE(validate(bad_speed).is_valid());

    MapData bad_capacity = line_map();
    bad_capacity.edges[0].capacity = 0;
    EXPECT_FALSE(validate(bad_capacity).is_valid());
}

TEST(MapValidation, validation_rejects_a_self_loop) {
    MapData data = line_map();
    data.edges.push_back(edge("E3", "C", "C"));

    const auto report = validate(data);
    EXPECT_FALSE(report.is_valid());
}

TEST(MapValidation, validation_rejects_zero_node_capacity) {
    MapData data = line_map();
    data.nodes[0].capacity = 0;

    EXPECT_FALSE(validate(data).is_valid());
}

/// A corridor claiming to be one passage while covering two disconnected
/// stretches would hand a robot a resource that does not lead where it thinks.
TEST(MapValidation, validation_rejects_a_corridor_whose_edges_do_not_join_up) {
    MapData data = line_map();
    data.nodes.push_back(node("D"));
    data.edges.push_back(edge("E3", "C", "D"));

    domain::Corridor corridor;
    corridor.id = ResourceId{"CORRIDOR-01"};
    corridor.entry_node = NodeId{"A"};
    corridor.exit_node = NodeId{"D"};
    corridor.edges = {EdgeId{"E1"}, EdgeId{"E3"}};  // skips E2, so B does not reach C
    corridor.capacity = 1;
    data.corridors = {corridor};

    const auto report = validate(data);
    EXPECT_FALSE(report.is_valid()) << report.to_string();
    EXPECT_TRUE(std::any_of(
        report.issues().begin(), report.issues().end(), [](const ValidationIssue& issue) {
            return issue.kind == IssueKind::corridor_not_contiguous;
        }));
}

TEST(MapValidation, validation_accepts_a_contiguous_corridor) {
    MapData data = line_map();

    domain::Corridor corridor;
    corridor.id = ResourceId{"CORRIDOR-01"};
    corridor.entry_node = NodeId{"A"};
    corridor.exit_node = NodeId{"C"};
    corridor.edges = {EdgeId{"E1"}, EdgeId{"E2"}};
    corridor.capacity = 1;
    data.corridors = {corridor};

    EXPECT_TRUE(validate(data).is_valid());
}

/// A conflict group naming a movement the intersection does not define
/// protects nothing: the lookup misses and the two movements read as
/// compatible.
TEST(MapValidation, validation_rejects_a_conflict_group_naming_an_unknown_movement) {
    MapData data = line_map();

    domain::Intersection crossing;
    crossing.id = ResourceId{"X1"};
    crossing.nodes = {NodeId{"B"}};
    crossing.capacity = 1;
    crossing.movements = {
        domain::Movement{core::MovementId{"M1"}, ResourceId{"X1"}, EdgeId{"E1"}, EdgeId{"E2"}}};
    crossing.conflict_groups = {domain::ConflictGroup{
        core::ConflictGroupId{"G1"}, {core::MovementId{"M1"}, core::MovementId{"M_GHOST"}}}};
    data.intersections = {crossing};

    const auto report = validate(data);
    EXPECT_FALSE(report.is_valid()) << report.to_string();
}

/// An unreachable maintenance spur is odd but usable. Refusing to load the map
/// would block a site over something harmless.
TEST(MapValidation, validation_reports_a_disconnected_graph_as_a_warning) {
    MapData data = line_map();
    data.nodes.push_back(node("ISLAND-1"));
    data.nodes.push_back(node("ISLAND-2"));
    data.edges.push_back(edge("E9", "ISLAND-1", "ISLAND-2"));

    const auto report = validate(data);
    EXPECT_TRUE(report.is_valid()) << "disconnection is a warning, not an error";
    EXPECT_GT(report.warning_count(), 0U);
}

TEST(MapValidation, validation_reports_an_orphan_node_as_a_warning) {
    MapData data = line_map();
    data.nodes.push_back(node("LONELY"));

    const auto report = validate(data);
    EXPECT_TRUE(report.is_valid());
    EXPECT_TRUE(std::any_of(
        report.issues().begin(), report.issues().end(), [](const ValidationIssue& issue) {
            return issue.kind == IssueKind::orphan_node;
        }));
}

/// Fixing a site map one error per run is not a workable loop.
TEST(MapValidation, validation_reports_every_problem_not_just_the_first) {
    MapData data = line_map();
    data.nodes.push_back(node("B"));                 // duplicate
    data.edges[0].length = 0.0;                      // invalid length
    data.edges.push_back(edge("E3", "C", "GHOST"));  // dangling

    const auto report = validate(data);
    EXPECT_FALSE(report.is_valid());
    EXPECT_GE(report.error_count(), 3U) << report.to_string();
}

TEST(MapValidation, make_map_returns_the_report_on_failure) {
    MapData data = line_map();
    data.edges[0].length = -5.0;

    auto result = make_map(std::move(data));
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(result.error().is_valid());
    EXPECT_NE(result.error().to_string().find("INVALID_EDGE_LENGTH"), std::string::npos);
}

TEST(MapValidation, validation_report_describes_itself) {
    const auto clean = validate(line_map());
    EXPECT_EQ(clean.to_string(), "map is valid");
}

}  // namespace
}  // namespace traffic::map
