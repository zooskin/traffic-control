#pragma once

/// \file
/// Small maps the planning tests share.
///
/// Every map here keeps its geometry honest: an edge's `length` is at least the
/// straight-line distance between the coordinates of its ends. That is the
/// precondition a distance heuristic rests on
/// (`geometry_supports_distance_heuristic`), and a test fixture that quietly
/// broke it would make A* look wrong for a reason that had nothing to do with
/// A*.

#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/domain/graph.h"
#include "traffic/domain/resources.h"
#include "traffic/domain/values.h"
#include "traffic/map/map.h"
#include "traffic/map/map_validator.h"

namespace traffic::planning::test {

inline domain::Node node(std::string id, double x, double y) {
    domain::Node n;
    n.id = core::NodeId{std::move(id)};
    n.position = domain::Position{x, y, 0.0};
    n.type = domain::NodeType::normal;
    n.capacity = 1;
    return n;
}

inline domain::Edge edge(std::string id,
                         std::string from,
                         std::string to,
                         double length,
                         domain::EdgeDirection direction = domain::EdgeDirection::bidirectional) {
    domain::Edge e;
    e.id = core::EdgeId{std::move(id)};
    e.from_node = core::NodeId{std::move(from)};
    e.to_node = core::NodeId{std::move(to)};
    e.length = length;
    e.width = 1.5;
    e.speed_limit = 1.0;
    e.direction = direction;
    e.capacity = 1;
    return e;
}

inline map::Map build(map::MapData data) {
    auto result = map::make_map(std::move(data));
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().to_string());
    return std::move(result).value();
}

/// A --E-AB(10)-- B --E-BC(10)-- C, in a straight line.
inline map::MapData line_map() {
    map::MapData data;
    data.map_id = "line";
    data.version = domain::MapVersion{1};
    data.nodes = {node("A", 0.0, 0.0), node("B", 10.0, 0.0), node("C", 20.0, 0.0)};
    data.edges = {edge("E-AB", "A", "B", 10.0), edge("E-BC", "B", "C", 10.0)};
    return data;
}

/// Two ways from A to D, one clearly shorter.
///
///     A --10-- B --10-- D          cost 20
///     A --10-- C --20-- E --10-- D cost 40
inline map::MapData two_route_map() {
    map::MapData data;
    data.map_id = "two-route";
    data.version = domain::MapVersion{1};
    data.nodes = {node("A", 0.0, 0.0),
                  node("B", 10.0, 0.0),
                  node("D", 20.0, 0.0),
                  node("C", 0.0, 10.0),
                  node("E", 20.0, 10.0)};
    data.edges = {edge("E-AB", "A", "B", 10.0),
                  edge("E-BD", "B", "D", 10.0),
                  edge("E-AC", "A", "C", 10.0),
                  edge("E-CE", "C", "E", 20.0),
                  edge("E-ED", "E", "D", 10.0)};
    return data;
}

/// A symmetric diamond: both ways from A to G cost exactly 40.
///
/// The fixture for docs/05_GLOBAL_ROUTING.md §22. With two routes of equal
/// cost, which one comes back is decided by the tie-breaker and by nothing
/// else — so if the answer ever varies, the tie-breaker is not doing its job.
inline map::MapData symmetric_map() {
    map::MapData data;
    data.map_id = "symmetric";
    data.version = domain::MapVersion{1};
    data.nodes = {
        node("A", 0.0, 0.0), node("U", 10.0, 10.0), node("L", 10.0, -10.0), node("G", 20.0, 0.0)};
    data.edges = {edge("E-AU", "A", "U", 20.0),
                  edge("E-UG", "U", "G", 20.0),
                  edge("E-AL", "A", "L", 20.0),
                  edge("E-LG", "L", "G", 20.0)};
    return data;
}

/// A one-way loop: A -> B -> C -> A, and no edge runs the other way.
///
/// Getting from C to B means going the long way round, C -> A -> B, because
/// E-BC may not be driven backwards. A planner that expands incident edges
/// instead of traversable ones answers with the single reversed edge and looks
/// perfectly correct doing it — the route is connected, and undrivable.
inline map::MapData one_way_map() {
    map::MapData data;
    data.map_id = "one-way";
    data.version = domain::MapVersion{1};
    data.nodes = {node("A", 0.0, 0.0), node("B", 10.0, 0.0), node("C", 10.0, 10.0)};
    data.edges = {edge("E-AB", "A", "B", 10.0, domain::EdgeDirection::forward),
                  edge("E-BC", "B", "C", 10.0, domain::EdgeDirection::forward),
                  edge("E-CA", "C", "A", 15.0, domain::EdgeDirection::forward)};
    return data;
}

/// A --E-AB-- B, and an island at Z that nothing connects to.
inline map::MapData island_map() {
    map::MapData data;
    data.map_id = "island";
    data.version = domain::MapVersion{1};
    data.nodes = {
        node("A", 0.0, 0.0), node("B", 10.0, 0.0), node("Z", 100.0, 0.0), node("Y", 110.0, 0.0)};
    data.edges = {edge("E-AB", "A", "B", 10.0), edge("E-ZY", "Z", "Y", 10.0)};
    return data;
}

/// The corridor shape this project exists for: three edges, one resource.
///
///     A --E-C1-- M1 --E-C2-- M2 --E-C3-- B   all in CORRIDOR-01
///     A --E-LONG------------------------ B   a longer way round
inline map::MapData corridor_map() {
    map::MapData data;
    data.map_id = "corridor";
    data.version = domain::MapVersion{1};
    data.nodes = {node("A", 0.0, 0.0),
                  node("M1", 10.0, 0.0),
                  node("M2", 20.0, 0.0),
                  node("B", 30.0, 0.0),
                  node("W", 15.0, 40.0)};

    data.edges = {edge("E-C1", "A", "M1", 10.0),
                  edge("E-C2", "M1", "M2", 10.0),
                  edge("E-C3", "M2", "B", 10.0),
                  edge("E-AW", "A", "W", 45.0),
                  edge("E-WB", "W", "B", 45.0)};

    for (domain::Edge& e : data.edges) {
        if (e.id.value().rfind("E-C", 0) == 0) {
            e.resource_id = core::ResourceId{"CORRIDOR-01"};
        }
    }

    domain::Corridor corridor;
    corridor.id = core::ResourceId{"CORRIDOR-01"};
    corridor.entry_node = core::NodeId{"A"};
    corridor.exit_node = core::NodeId{"B"};
    corridor.edges = {core::EdgeId{"E-C1"}, core::EdgeId{"E-C2"}, core::EdgeId{"E-C3"}};
    corridor.capacity = 1;
    corridor.direction = domain::EdgeDirection::bidirectional;
    data.corridors = {corridor};

    return data;
}

}  // namespace traffic::planning::test
