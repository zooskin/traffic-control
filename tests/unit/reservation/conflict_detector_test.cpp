/// The conflict detector. docs/22_IMPLEMENTATION_WORKFLOW.md Phase 6 (Phase 7
/// of the confirmed order in docs/00_INDEX.md D-003),
/// docs/24_DOMAIN_MODEL.md §15~16, docs/06_TRAFFIC_RESERVATION.md §7~11 and
/// §38~39, docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8 and §12,
/// docs/03_MAP_GRAPH.md §9~12.
///
/// Phase 6's completion criteria are Two Robot Conflict, Multi Robot Conflict,
/// Temporal Conflict, Corridor Conflict and Regression Test; the acceptance
/// cases of docs/06 §38 add two robots entering one corridor together, the same
/// two from opposite ends, and an intersection conflict. They are all here.
///
/// Two tests matter more than the rest. The head-on case is the failure the
/// whole project is written around — two robots one edge apart inside a
/// single-lane corridor, neither able to pass. The compatible-movements case is
/// its mirror image: an intersection where two robots must be allowed through
/// together, because a detector that reports every shared crossing stops the
/// floor just as effectively as one that reports none.

#include "traffic/reservation/conflict_detector.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/clock.h"
#include "traffic/core/ids.h"
#include "traffic/domain/conflict.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/graph.h"
#include "traffic/domain/reservation.h"
#include "traffic/domain/reservation_state.h"
#include "traffic/domain/resources.h"
#include "traffic/domain/route.h"
#include "traffic/domain/values.h"
#include "traffic/map/map.h"
#include "traffic/map/map_validator.h"

namespace traffic::reservation {
namespace {

using core::ConflictGroupId;
using core::EdgeId;
using core::MovementId;
using core::NodeId;
using core::ReservationId;
using core::ResourceId;
using core::RobotId;
using core::RouteId;
using domain::ConflictType;

core::Duration seconds(int count) {
    return std::chrono::duration_cast<core::Duration>(core::Seconds{count});
}

core::TimePoint at(int second) {
    return core::kTimeOrigin + seconds(second);
}

domain::TimeWindow window(int start, int end) {
    return domain::TimeWindow{at(start), at(end)};
}

// ------------------------------------------------------------------ fixtures

domain::Node node(std::string id, double x, double y) {
    domain::Node value;
    value.id = NodeId{std::move(id)};
    value.position = domain::Position{x, y, 0.0};
    value.type = domain::NodeType::normal;
    value.capacity = 1;
    return value;
}

domain::Edge edge(std::string id, std::string from, std::string to) {
    domain::Edge value;
    value.id = EdgeId{std::move(id)};
    value.from_node = NodeId{std::move(from)};
    value.to_node = NodeId{std::move(to)};
    value.length = 10.0;
    value.width = 1.5;
    value.speed_limit = 1.0;
    value.direction = domain::EdgeDirection::bidirectional;
    value.capacity = 1;
    return value;
}

map::Map build(map::MapData data) {
    auto result = map::make_map(std::move(data));
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().to_string());
    return std::move(result).value();
}

/// A --E-AB-- B --E-BC-- C. Every edge is its own resource.
map::MapData plain_map() {
    map::MapData data;
    data.map_id = "plain";
    data.version = domain::MapVersion{1};
    data.nodes = {node("A", 0.0, 0.0), node("B", 10.0, 0.0), node("C", 20.0, 0.0)};
    data.edges = {edge("E-AB", "A", "B"), edge("E-BC", "B", "C")};
    return data;
}

/// The shape this project exists for: three edges, one resource.
///
///     A --E-C1-- M1 --E-C2-- M2 --E-C3-- B      all of it CORRIDOR-01
///
/// Two robots one edge apart inside it are in trouble even though they share no
/// edge at all — docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8.
map::MapData corridor_map(std::uint32_t capacity) {
    map::MapData data;
    data.map_id = "corridor";
    data.version = domain::MapVersion{1};
    data.nodes = {
        node("A", 0.0, 0.0), node("M1", 10.0, 0.0), node("M2", 20.0, 0.0), node("B", 30.0, 0.0)};
    data.edges = {edge("E-C1", "A", "M1"), edge("E-C2", "M1", "M2"), edge("E-C3", "M2", "B")};
    for (domain::Edge& value : data.edges) {
        value.resource_id = ResourceId{"CORRIDOR-01"};
        value.capacity = capacity;
    }

    domain::Corridor lane;
    lane.id = ResourceId{"CORRIDOR-01"};
    lane.entry_node = NodeId{"A"};
    lane.exit_node = NodeId{"B"};
    lane.edges = {EdgeId{"E-C1"}, EdgeId{"E-C2"}, EdgeId{"E-C3"}};
    lane.capacity = capacity;
    lane.direction = domain::EdgeDirection::bidirectional;
    data.corridors = {lane};

    return data;
}

/// A four-way crossing at C, with nothing declared about it.
///
///          N
///          |
///     W -- C -- E
///          |
///          S
map::MapData cross_map() {
    map::MapData data;
    data.map_id = "cross";
    data.version = domain::MapVersion{1};
    data.nodes = {node("C", 0.0, 0.0),
                  node("W", -10.0, 0.0),
                  node("E", 10.0, 0.0),
                  node("N", 0.0, 10.0),
                  node("S", 0.0, -10.0)};
    data.edges = {edge("E-WC", "W", "C"),
                  edge("E-CE", "C", "E"),
                  edge("E-NC", "N", "C"),
                  edge("E-CS", "C", "S")};
    return data;
}

domain::Movement movement(std::string id, std::string from_edge, std::string to_edge) {
    domain::Movement value;
    value.id = MovementId{std::move(id)};
    value.intersection_id = ResourceId{"INTERSECTION-01"};
    value.from_edge = EdgeId{std::move(from_edge)};
    value.to_edge = EdgeId{std::move(to_edge)};
    return value;
}

/// The same crossing, declared as an intersection.
///
/// West-to-east and north-to-south exclude each other. South-to-north excludes
/// neither, so a robot taking it may cross alongside one going west to east —
/// the case docs/03_MAP_GRAPH.md §11 introduces conflict groups for.
map::MapData intersection_map() {
    map::MapData data = cross_map();
    data.map_id = "intersection";
    for (domain::Node& value : data.nodes) {
        if (value.id == NodeId{"C"}) {
            value.type = domain::NodeType::intersection;
            value.resource_id = ResourceId{"INTERSECTION-01"};
        }
    }

    domain::Intersection crossroads;
    crossroads.id = ResourceId{"INTERSECTION-01"};
    crossroads.nodes = {NodeId{"C"}};
    crossroads.edges = {EdgeId{"E-WC"}, EdgeId{"E-CE"}, EdgeId{"E-NC"}, EdgeId{"E-CS"}};
    crossroads.movements = {movement("M-WE", "E-WC", "E-CE"),
                            movement("M-NS", "E-NC", "E-CS"),
                            movement("M-SN", "E-CS", "E-NC")};

    domain::ConflictGroup group;
    group.id = ConflictGroupId{"G-CROSS"};
    group.movements = {MovementId{"M-WE"}, MovementId{"M-NS"}};
    crossroads.conflict_groups = {group};

    crossroads.capacity = 1;
    data.intersections = {crossroads};

    return data;
}

/// A route whose segments each take \p segment_seconds, starting at
/// \p start_second.
domain::Route timed_route(const std::string& robot,
                          const std::vector<std::string>& nodes,
                          const std::vector<std::string>& edges,
                          int start_second,
                          int segment_seconds) {
    std::vector<core::NodeId> node_ids;
    node_ids.reserve(nodes.size());
    for (const std::string& name : nodes) {
        node_ids.push_back(NodeId{name});
    }

    std::vector<domain::RouteSegment> segments;
    segments.reserve(edges.size());
    for (std::size_t i = 0; i < edges.size(); ++i) {
        const int entry = start_second + (static_cast<int>(i) * segment_seconds);
        domain::RouteSegment segment;
        segment.edge_id = EdgeId{edges[i]};
        segment.from_node = node_ids[i];
        segment.to_node = node_ids[i + 1];
        segment.sequence = i;
        segment.expected_window = window(entry, entry + segment_seconds);
        segments.push_back(segment);
    }

    auto result = domain::make_route(
        RouteId{"RT-" + robot}, RobotId{robot}, node_ids, segments, domain::MapVersion{1}, at(0));
    EXPECT_TRUE(result.has_value());
    return std::move(result).value();
}

/// The same route with the timing stripped out, as a planner that has not
/// estimated yet would leave it.
domain::Route untimed_route(const std::string& robot,
                            const std::vector<std::string>& nodes,
                            const std::vector<std::string>& edges) {
    domain::Route route = timed_route(robot, nodes, edges, 10, 10);
    for (domain::RouteSegment& segment : route.segments) {
        segment.expected_window.reset();
    }
    return route;
}

/// A -> B along the whole corridor.
domain::Route through_corridor(const std::string& robot, int start) {
    return timed_route(robot, {"A", "M1", "M2", "B"}, {"E-C1", "E-C2", "E-C3"}, start, 10);
}

/// B -> A, the other way through the same corridor.
domain::Route back_through_corridor(const std::string& robot, int start) {
    return timed_route(robot, {"B", "M2", "M1", "A"}, {"E-C3", "E-C2", "E-C1"}, start, 10);
}

/// W -> C -> E across the crossing.
domain::Route west_to_east(const std::string& robot, int start) {
    return timed_route(robot, {"W", "C", "E"}, {"E-WC", "E-CE"}, start, 10);
}

/// N -> C -> S, which cuts across the path above.
domain::Route north_to_south(const std::string& robot, int start) {
    return timed_route(robot, {"N", "C", "S"}, {"E-NC", "E-CS"}, start, 10);
}

/// S -> C -> N, which does not.
domain::Route south_to_north(const std::string& robot, int start) {
    return timed_route(robot, {"S", "C", "N"}, {"E-CS", "E-NC"}, start, 10);
}

/// Aggregate initialisation, because TimeWindow has no default constructor:
/// there is no sensible empty window, so a Reservation cannot be built blank
/// and filled in afterwards.
domain::Reservation reserve(const std::string& robot,
                            std::string resource,
                            int start,
                            int end,
                            domain::ReservationState state) {
    return domain::Reservation{
        .id = ReservationId{"RES-" + robot},
        .robot_id = RobotId{robot},
        .resource_id = ResourceId{std::move(resource)},
        .window = window(start, end),
        .state = state,
        .priority = domain::Priority{50},
        .requested_at = core::kTimeOrigin,
    };
}

std::vector<core::ConflictId> ids_of(const std::vector<domain::Conflict>& conflicts) {
    std::vector<core::ConflictId> ids;
    ids.reserve(conflicts.size());
    for (const domain::Conflict& conflict : conflicts) {
        ids.push_back(conflict.id);
    }
    return ids;
}

// ------------------------------------------------------------- route reading

TEST(ConflictDetector, three_edges_in_one_corridor_read_as_a_single_passage) {
    const map::Map map = build(corridor_map(1));
    const domain::Route route = through_corridor("R01", 10);

    const std::vector<CorridorPassage> passages = corridor_passages(map, route);
    ASSERT_EQ(passages.size(), 1U);
    EXPECT_EQ(passages.front().corridor_id, ResourceId{"CORRIDOR-01"});
    EXPECT_EQ(passages.front().entry_node, NodeId{"A"});
    EXPECT_EQ(passages.front().exit_node, NodeId{"B"});
    EXPECT_EQ(passages.front().direction, PassageDirection::entry_to_exit);
    EXPECT_EQ(passages.front().window, window(10, 40));
}

TEST(ConflictDetector, a_corridor_run_the_other_way_reads_as_the_reverse_direction) {
    const map::Map map = build(corridor_map(1));
    const domain::Route route = back_through_corridor("R01", 10);

    const std::vector<CorridorPassage> passages = corridor_passages(map, route);
    ASSERT_EQ(passages.size(), 1U);
    EXPECT_EQ(passages.front().direction, PassageDirection::exit_to_entry);
}

TEST(ConflictDetector, segments_without_timing_produce_no_occupancies) {
    const map::Map map = build(plain_map());
    const domain::Route route = untimed_route("R01", {"A", "B", "C"}, {"E-AB", "E-BC"});

    EXPECT_TRUE(edge_traversals(map, route).empty());
    EXPECT_TRUE(node_visits(map, route, SafetyBuffer{}).empty());
}

TEST(ConflictDetector, intersection_nodes_are_left_out_of_the_node_visits) {
    // Inside an intersection, movements decide and occupancy does not —
    // docs/06_TRAFFIC_RESERVATION.md §8. Two robots crossing on compatible
    // movements share the crossing node by design.
    const map::Map map = build(intersection_map());
    const domain::Route route = west_to_east("R01", 10);

    const std::vector<NodeVisit> visits = node_visits(map, route, SafetyBuffer{});
    ASSERT_EQ(visits.size(), 2U);
    EXPECT_EQ(visits[0].node_id, NodeId{"W"});
    EXPECT_EQ(visits[1].node_id, NodeId{"E"});

    const std::vector<IntersectionCrossing> crossings =
        intersection_crossings(map, route, SafetyBuffer{});
    ASSERT_EQ(crossings.size(), 1U);
    ASSERT_TRUE(crossings.front().movement_id.has_value());
    EXPECT_EQ(crossings.front().movement_id.value(), MovementId{"M-WE"});
}

// -------------------------------------------------------- two robot conflict

TEST(ConflictDetector, two_robots_on_one_edge_report_a_single_edge_conflict) {
    const map::Map map = build(plain_map());
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route leader = timed_route("R01", {"A", "B"}, {"E-AB"}, 10, 10);
    const domain::Route follower = timed_route("R02", {"A", "B"}, {"E-AB"}, 15, 10);

    const std::vector<domain::Conflict> conflicts = detector.detect_between(leader, follower);
    ASSERT_EQ(conflicts.size(), 1U);
    EXPECT_EQ(conflicts.front().type, ConflictType::edge);
    EXPECT_EQ(conflicts.front().resource_id, ResourceId{"E-AB"});
    EXPECT_EQ(conflicts.front().robot_a, RobotId{"R01"});
    EXPECT_EQ(conflicts.front().robot_b, RobotId{"R02"});
}

TEST(ConflictDetector, two_robots_meeting_on_one_edge_report_head_on) {
    const map::Map map = build(plain_map());
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route eastbound = timed_route("R01", {"A", "B"}, {"E-AB"}, 10, 10);
    const domain::Route westbound = timed_route("R02", {"B", "A"}, {"E-AB"}, 10, 10);

    const std::vector<domain::Conflict> conflicts = detector.detect_between(eastbound, westbound);
    ASSERT_EQ(conflicts.size(), 1U);
    EXPECT_EQ(conflicts.front().type, ConflictType::head_on);
}

TEST(ConflictDetector, a_conflict_between_two_robots_is_reported_once) {
    // Whichever way round the pair is presented, one conflict comes back with
    // the same identity. Two entries would have the priority manager resolve
    // one contention twice, and the second answer could disagree with the
    // first.
    const map::Map map = build(plain_map());
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route leader = timed_route("R01", {"A", "B"}, {"E-AB"}, 10, 10);
    const domain::Route follower = timed_route("R02", {"A", "B"}, {"E-AB"}, 15, 10);

    const std::vector<domain::Conflict> forward = detector.detect_between(leader, follower);
    const std::vector<domain::Conflict> reversed = detector.detect_between(follower, leader);

    ASSERT_EQ(forward.size(), 1U);
    ASSERT_EQ(reversed.size(), 1U);
    EXPECT_EQ(forward.front().id, reversed.front().id);
    EXPECT_EQ(forward.front().robot_a, reversed.front().robot_a);
}

TEST(ConflictDetector, routes_that_share_nothing_report_nothing) {
    const map::Map map = build(plain_map());
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route first = timed_route("R01", {"A", "B"}, {"E-AB"}, 10, 10);
    const domain::Route second = timed_route("R02", {"B", "C"}, {"E-BC"}, 100, 10);

    EXPECT_TRUE(detector.detect_between(first, second).empty());
}

// --------------------------------------------------------------------- nodes

TEST(ConflictDetector, two_routes_crossing_at_an_undeclared_node_report_a_node_conflict) {
    const map::Map map = build(cross_map());
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route eastbound = west_to_east("R01", 10);
    const domain::Route southbound = north_to_south("R02", 10);

    const std::vector<domain::Conflict> conflicts = detector.detect_between(eastbound, southbound);
    ASSERT_EQ(conflicts.size(), 1U);
    EXPECT_EQ(conflicts.front().type, ConflictType::node);
    EXPECT_EQ(conflicts.front().resource_id, ResourceId{"C"});
}

TEST(ConflictDetector, two_routes_passing_the_same_node_in_turn_report_nothing) {
    const map::Map map = build(cross_map());
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route eastbound = west_to_east("R01", 10);
    const domain::Route southbound = north_to_south("R02", 60);

    EXPECT_TRUE(detector.detect_between(eastbound, southbound).empty());
}

// ------------------------------------------------------------------ corridor

TEST(ConflictDetector, two_robots_entering_a_corridor_from_opposite_ends_report_head_on) {
    // docs/06_TRAFFIC_RESERVATION.md §38 Test 2, and the case docs/25 §8 draws.
    // The robots are on the same edge for only one of the three legs; it is the
    // corridor, not the edge, that has room for one of them.
    const map::Map map = build(corridor_map(1));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route eastbound = through_corridor("R01", 10);
    const domain::Route westbound = back_through_corridor("R02", 10);

    const std::vector<domain::Conflict> conflicts = detector.detect_between(eastbound, westbound);
    ASSERT_EQ(conflicts.size(), 1U);
    EXPECT_EQ(conflicts.front().type, ConflictType::head_on);
    EXPECT_EQ(conflicts.front().resource_id, ResourceId{"CORRIDOR-01"});
}

TEST(ConflictDetector, three_edges_of_one_corridor_report_a_single_corridor_conflict) {
    // docs/06_TRAFFIC_RESERVATION.md §38 Test 1. Both robots share all three
    // edges, and the answer is one conflict about the corridor rather than
    // three about the edges inside it.
    const map::Map map = build(corridor_map(1));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route leader = through_corridor("R01", 10);
    const domain::Route follower = through_corridor("R02", 15);

    const std::vector<domain::Conflict> conflicts = detector.detect_between(leader, follower);
    ASSERT_EQ(conflicts.size(), 1U);
    EXPECT_EQ(conflicts.front().type, ConflictType::corridor);
    EXPECT_EQ(conflicts.front().resource_id, ResourceId{"CORRIDOR-01"});
}

TEST(ConflictDetector, a_corridor_with_room_for_two_admits_two_robots) {
    const map::Map map = build(corridor_map(2));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    EXPECT_TRUE(
        detector.detect_between(through_corridor("R01", 10), through_corridor("R02", 15)).empty());
}

TEST(ConflictDetector, a_corridor_with_room_for_two_refuses_a_third) {
    const map::Map map = build(corridor_map(2));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const std::vector<domain::Route> routes{
        through_corridor("R01", 10), through_corridor("R02", 15), through_corridor("R03", 20)};
    ConflictQuery query;
    query.routes = routes;

    // Every pair is over capacity while all three are inside, and each pair
    // needs its own decision.
    EXPECT_EQ(detector.detect(query).size(), 3U);
}

// ------------------------------------------------------ multi robot conflict

TEST(ConflictDetector, three_robots_in_one_corridor_report_a_conflict_for_every_pair) {
    const map::Map map = build(corridor_map(1));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const std::vector<domain::Route> routes{
        through_corridor("R01", 10), through_corridor("R02", 15), through_corridor("R03", 20)};
    ConflictQuery query;
    query.routes = routes;

    const std::vector<domain::Conflict> conflicts = detector.detect(query);
    ASSERT_EQ(conflicts.size(), 3U);
    for (const domain::Conflict& conflict : conflicts) {
        EXPECT_EQ(conflict.type, ConflictType::corridor);
        EXPECT_LT(conflict.robot_a, conflict.robot_b);
    }
}

// ------------------------------------------------------------- intersections

TEST(ConflictDetector, movements_in_one_conflict_group_report_a_crossing_conflict) {
    // docs/06_TRAFFIC_RESERVATION.md §38 Test 3.
    const map::Map map = build(intersection_map());
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route eastbound = west_to_east("R01", 10);
    const domain::Route southbound = north_to_south("R02", 10);

    const std::vector<domain::Conflict> conflicts = detector.detect_between(eastbound, southbound);
    ASSERT_EQ(conflicts.size(), 1U);
    EXPECT_EQ(conflicts.front().type, ConflictType::crossing);
    EXPECT_EQ(conflicts.front().resource_id, ResourceId{"INTERSECTION-01"});
}

TEST(ConflictDetector, compatible_movements_at_an_intersection_report_nothing) {
    // The test that keeps an intersection usable. Both robots are inside the
    // crossing at the same instant and their movements share no conflict group,
    // so they may go together — docs/03_MAP_GRAPH.md §11.
    const map::Map map = build(intersection_map());
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route eastbound = west_to_east("R01", 10);
    const domain::Route northbound = south_to_north("R02", 10);

    EXPECT_TRUE(detector.detect_between(eastbound, northbound).empty());
}

// -------------------------------------------------------------- reservations

TEST(ConflictDetector, two_reservations_on_one_resource_conflict) {
    const map::Map map = build(corridor_map(1));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const std::vector<domain::Reservation> reservations{
        reserve("R01", "CORRIDOR-01", 10, 40, domain::ReservationState::active),
        reserve("R02", "CORRIDOR-01", 30, 60, domain::ReservationState::active)};

    ConflictQuery query;
    query.reservations = reservations;

    const std::vector<domain::Conflict> conflicts = detector.detect(query);
    ASSERT_EQ(conflicts.size(), 1U);
    EXPECT_EQ(conflicts.front().type, ConflictType::resource);
}

TEST(ConflictDetector, a_released_reservation_conflicts_with_nothing) {
    const map::Map map = build(corridor_map(1));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const std::vector<domain::Reservation> reservations{
        reserve("R01", "CORRIDOR-01", 10, 40, domain::ReservationState::released),
        reserve("R02", "CORRIDOR-01", 30, 60, domain::ReservationState::active)};

    ConflictQuery query;
    query.reservations = reservations;

    EXPECT_TRUE(detector.detect(query).empty());
}

TEST(ConflictDetector, a_route_into_a_reserved_corridor_reports_a_temporal_conflict) {
    // The clash exists only because the route carries expected entry and exit
    // times — docs/24_DOMAIN_MODEL.md §12. Without them one robot's intent and
    // another's grant could not be compared at all.
    const map::Map map = build(corridor_map(1));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const std::vector<domain::Route> routes{through_corridor("R01", 10)};
    const std::vector<domain::Reservation> reservations{
        reserve("R02", "CORRIDOR-01", 15, 45, domain::ReservationState::active)};

    ConflictQuery query;
    query.routes = routes;
    query.reservations = reservations;

    const std::vector<domain::Conflict> conflicts = detector.detect(query);
    ASSERT_EQ(conflicts.size(), 1U);
    EXPECT_EQ(conflicts.front().type, ConflictType::temporal);
    EXPECT_EQ(conflicts.front().resource_id, ResourceId{"CORRIDOR-01"});
}

TEST(ConflictDetector, a_robot_does_not_conflict_with_its_own_reservation) {
    const map::Map map = build(corridor_map(1));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const std::vector<domain::Route> routes{through_corridor("R01", 10)};
    const std::vector<domain::Reservation> reservations{
        reserve("R01", "CORRIDOR-01", 10, 40, domain::ReservationState::active)};

    ConflictQuery query;
    query.routes = routes;
    query.reservations = reservations;

    EXPECT_TRUE(detector.detect(query).empty());
}

// ---------------------------------------------------------- temporal conflict

TEST(ConflictDetector, routes_sharing_a_corridor_at_disjoint_times_report_nothing) {
    const map::Map map = build(corridor_map(1));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    EXPECT_TRUE(
        detector.detect_between(through_corridor("R01", 10), through_corridor("R02", 100)).empty());
}

TEST(ConflictDetector, a_route_that_leaves_before_the_next_arrives_reports_nothing) {
    // Half-open windows, so a handover at the exact instant is not a conflict.
    // Anything else forces a fabricated gap between every pair of robots.
    const map::Map map = build(plain_map());
    core::SimulationClock clock;

    ConflictDetectorConfig config;
    config.node_buffer.entry = core::Duration::zero();
    config.node_buffer.exit = core::Duration::zero();
    const ConflictDetector detector{map, clock, config};

    const domain::Route first = timed_route("R01", {"A", "B"}, {"E-AB"}, 10, 10);
    const domain::Route second = timed_route("R02", {"A", "B"}, {"E-AB"}, 20, 10);

    EXPECT_TRUE(detector.detect_between(first, second).empty());
}

TEST(ConflictDetector, routes_without_timing_report_nothing) {
    const map::Map map = build(corridor_map(1));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route first = untimed_route("R01", {"A", "M1"}, {"E-C1"});
    const domain::Route second = untimed_route("R02", {"A", "M1"}, {"E-C1"});

    EXPECT_TRUE(detector.detect_between(first, second).empty());
}

// ----------------------------------------------------------- regression tests

TEST(ConflictDetector, detection_is_deterministic_across_route_orderings) {
    // docs/01_REQUIREMENTS.md NFR-003. A conflict list that depends on the
    // order the routes arrived in makes every scenario in
    // docs/26_TEST_SCENARIOS.md unreproducible, and the failure stays invisible
    // until a rerun disagrees.
    const map::Map map = build(corridor_map(1));
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    const domain::Route first = through_corridor("R01", 10);
    const domain::Route second = through_corridor("R02", 15);
    const domain::Route third = through_corridor("R03", 20);

    const std::vector<domain::Route> forward{first, second, third};
    const std::vector<domain::Route> reversed{third, second, first};

    ConflictQuery forward_query;
    forward_query.routes = forward;
    ConflictQuery reversed_query;
    reversed_query.routes = reversed;

    EXPECT_EQ(ids_of(detector.detect(forward_query)), ids_of(detector.detect(reversed_query)));
}

TEST(ConflictDetector, conflict_ids_are_the_same_on_a_later_run) {
    // The id names the conflict, not the moment it was noticed, so a detector
    // run later against the same intent produces the same id — which is what
    // lets a controller tell an unresolved conflict from a new one.
    const map::Map map = build(corridor_map(1));
    core::SimulationClock early;
    core::SimulationClock late{at(90)};
    const ConflictDetector first_detector{map, early};
    const ConflictDetector second_detector{map, late};

    const domain::Route leader = through_corridor("R01", 10);
    const domain::Route follower = through_corridor("R02", 15);

    EXPECT_EQ(ids_of(first_detector.detect_between(leader, follower)),
              ids_of(second_detector.detect_between(leader, follower)));
}

TEST(ConflictDetector, detected_at_comes_from_the_injected_clock) {
    const map::Map map = build(plain_map());
    core::SimulationClock clock{at(42)};
    const ConflictDetector detector{map, clock};

    const domain::Route leader = timed_route("R01", {"A", "B"}, {"E-AB"}, 10, 10);
    const domain::Route follower = timed_route("R02", {"A", "B"}, {"E-AB"}, 15, 10);

    const std::vector<domain::Conflict> conflicts = detector.detect_between(leader, follower);
    ASSERT_EQ(conflicts.size(), 1U);
    EXPECT_EQ(conflicts.front().detected_at, at(42));
}

TEST(ConflictDetector, an_empty_query_reports_nothing) {
    const map::Map map = build(plain_map());
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    EXPECT_TRUE(detector.detect(ConflictQuery{}).empty());
}

TEST(ConflictDetector, the_id_prefix_leads_every_conflict_id) {
    const map::Map map = build(plain_map());
    core::SimulationClock clock;

    ConflictDetectorConfig config;
    config.id_prefix = "RUN7";
    const ConflictDetector detector{map, clock, config};

    const domain::Route leader = timed_route("R01", {"A", "B"}, {"E-AB"}, 10, 10);
    const domain::Route follower = timed_route("R02", {"A", "B"}, {"E-AB"}, 15, 10);

    const std::vector<domain::Conflict> conflicts = detector.detect_between(leader, follower);
    ASSERT_EQ(conflicts.size(), 1U);
    EXPECT_EQ(conflicts.front().id.value().rfind("RUN7-", 0), 0U);
}

TEST(ConflictDetector, the_detector_is_named) {
    const map::Map map = build(plain_map());
    core::SimulationClock clock;
    const ConflictDetector detector{map, clock};

    EXPECT_EQ(detector.name(), "ConflictDetector");
}

// ------------------------------------------------------------- configuration

TEST(ConflictDetector, safety_buffer_defaults_are_accepted) {
    EXPECT_TRUE(make_safety_buffer(SafetyBuffer{}).has_value());
}

TEST(ConflictDetector, safety_buffer_rejects_a_negative_margin) {
    SafetyBuffer buffer;
    buffer.entry = seconds(-1);

    const auto result = make_safety_buffer(buffer);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), domain::DomainError::negative_value);
}

TEST(ConflictDetector, configuration_rejects_an_empty_id_prefix) {
    ConflictDetectorConfig config;
    config.id_prefix.clear();

    const auto result = make_conflict_detector_config(config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), domain::DomainError::empty_id);
}

TEST(ConflictDetector, configuration_rejects_crossed_severity_thresholds) {
    ConflictDetectorConfig config;
    config.severity.absorbed_lead = seconds(5);
    config.severity.critical_lead = seconds(10);

    const auto result = make_conflict_detector_config(config);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), domain::DomainError::invalid_time_window);
}

TEST(ConflictDetector, configuration_defaults_are_accepted) {
    const auto result = make_conflict_detector_config(ConflictDetectorConfig{});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().id_prefix, "CONF");
}

}  // namespace
}  // namespace traffic::reservation
