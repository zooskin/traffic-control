/// Tests for reading and writing map files.
///
/// Naming follows docs/20_CODING_GUIDELINES.md §45. Covers test_map_load from
/// docs/03_MAP_GRAPH.md §25, and the corridor, intersection, conflict group and
/// waiting bay cases as they appear in a real file.

#include "traffic/map/map_loader.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/graph.h"
#include "traffic/domain/resources.h"
#include "traffic/map/map.h"

namespace traffic::map {
namespace {

using core::EdgeId;
using core::MovementId;
using core::NodeId;
using core::ResourceId;

constexpr const char* kMinimalMap = R"({
  "map_id": "minimal",
  "version": 3,
  "nodes": [
    { "id": "A", "x": 0.0, "y": 0.0 },
    { "id": "B", "x": 10.0, "y": 0.0 }
  ],
  "edges": [
    { "id": "E1", "from": "A", "to": "B", "length": 10.0, "speed_limit": 1.0 }
  ]
})";

// ===================================================================== load

TEST(MapLoader, load_reads_a_minimal_map) {
    auto result = load_map_from_json(kMinimalMap);
    ASSERT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().to_string());

    const Map map = std::move(result).value();
    EXPECT_EQ(map.id(), "minimal");
    EXPECT_EQ(map.version(), domain::MapVersion{3});
    EXPECT_EQ(map.node_count(), 2U);
    EXPECT_EQ(map.edge_count(), 1U);
    EXPECT_TRUE(map.is_reachable(NodeId{"A"}, NodeId{"B"}));
}

TEST(MapLoader, load_rejects_malformed_json) {
    const auto result = load_map_from_json("{ this is not json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, LoadError::Kind::malformed_json);
}

TEST(MapLoader, load_rejects_a_non_object_top_level) {
    const auto result = load_map_from_json("[1, 2, 3]");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, LoadError::Kind::schema_violation);
}

TEST(MapLoader, load_rejects_a_missing_required_field) {
    const auto result = load_map_from_json(R"({ "map_id": "x", "nodes": [] })");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, LoadError::Kind::schema_violation);
    EXPECT_NE(result.error().message.find("edges"), std::string::npos);
}

/// "expected a string" with no location is useless in a file with two thousand
/// edges.
TEST(MapLoader, load_error_names_the_offending_element) {
    const auto result = load_map_from_json(R"({
      "nodes": [{ "id": "A" }, { "id": "B" }],
      "edges": [{ "id": "E1", "from": "A", "length": 5.0 }]
    })");
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("E1"), std::string::npos) << result.error().message;
    EXPECT_NE(result.error().message.find("to"), std::string::npos);
}

TEST(MapLoader, load_rejects_an_unknown_node_type) {
    const auto result = load_map_from_json(R"({
      "nodes": [{ "id": "A", "type": "TELEPORTER" }],
      "edges": []
    })");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, LoadError::Kind::invalid_value);
    EXPECT_NE(result.error().message.find("TELEPORTER"), std::string::npos);
}

TEST(MapLoader, load_rejects_an_unknown_direction) {
    const auto result = load_map_from_json(R"({
      "nodes": [{ "id": "A" }, { "id": "B" }],
      "edges": [{ "id": "E1", "from": "A", "to": "B", "length": 1.0, "direction": "SIDEWAYS" }]
    })");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, LoadError::Kind::invalid_value);
}

/// A map that parses but is subtly wrong is worse than one that refuses to
/// load: the error surfaces hours later as a robot that cannot be routed.
TEST(MapLoader, load_rejects_a_map_that_parses_but_does_not_validate) {
    const auto result = load_map_from_json(R"({
      "nodes": [{ "id": "A" }, { "id": "B" }],
      "edges": [{ "id": "E1", "from": "A", "to": "GHOST", "length": 5.0 }]
    })");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, LoadError::Kind::validation_failed);
    EXPECT_FALSE(result.error().report.is_valid());
    EXPECT_NE(result.error().to_string().find("DANGLING_EDGE_ENDPOINT"), std::string::npos);
}

/// docs/03_MAP_GRAPH.md §23 — a site sets its defaults once instead of
/// repeating the same speed limit on every edge.
TEST(MapLoader, load_applies_the_default_speed_limit) {
    auto result = load_map_from_json(R"({
      "defaults": { "speed_limit": 2.5 },
      "nodes": [{ "id": "A" }, { "id": "B" }],
      "edges": [{ "id": "E1", "from": "A", "to": "B", "length": 10.0 }]
    })");
    ASSERT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().to_string());

    const Map map = std::move(result).value();
    ASSERT_NE(map.find_edge(EdgeId{"E1"}), nullptr);
    EXPECT_EQ(map.find_edge(EdgeId{"E1"})->speed_limit, 2.5);
}

TEST(MapLoader, load_prefers_an_explicit_speed_limit_over_the_default) {
    auto result = load_map_from_json(R"({
      "defaults": { "speed_limit": 2.5 },
      "nodes": [{ "id": "A" }, { "id": "B" }],
      "edges": [{ "id": "E1", "from": "A", "to": "B", "length": 10.0, "speed_limit": 0.5 }]
    })");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(std::move(result).value().find_edge(EdgeId{"E1"})->speed_limit, 0.5);
}

TEST(MapLoader, load_reads_a_travel_time_override) {
    auto result = load_map_from_json(R"({
      "nodes": [{ "id": "A" }, { "id": "B" }],
      "edges": [{ "id": "LIFT", "from": "A", "to": "B", "length": 3.0,
                  "speed_limit": 1.0, "travel_time_s": 25.0 }]
    })");
    ASSERT_TRUE(result.has_value());

    const Map map = std::move(result).value();
    EXPECT_EQ(domain::nominal_travel_time(*map.find_edge(EdgeId{"LIFT"})),
              core::Duration{core::Seconds{25}});
}

TEST(MapLoader, load_from_a_missing_file_reports_it) {
    const auto result = load_map_from_file("this/path/does/not/exist.json");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().kind, LoadError::Kind::file_not_readable);
}

// ============================================================ reference map

/// The map of docs/17_SIMULATION_SCENARIOS.md §2, which the scenarios of
/// docs/26_TEST_SCENARIOS.md run against. If this stops loading, every
/// simulation scenario loses its ground.
TEST(MapLoader, reference_map_loads_and_validates) {
    const std::filesystem::path path{TC_REFERENCE_MAP_PATH};
    ASSERT_TRUE(std::filesystem::exists(path)) << path.string();

    auto result = load_map_from_file(path);
    ASSERT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().to_string());

    const Map map = std::move(result).value();
    EXPECT_EQ(map.id(), "reference");
    EXPECT_GT(map.node_count(), 5U);
}

TEST(MapLoader, reference_map_has_a_single_lane_bidirectional_corridor) {
    auto result = load_map_from_file(std::filesystem::path{TC_REFERENCE_MAP_PATH});
    ASSERT_TRUE(result.has_value());
    const Map map = std::move(result).value();

    const domain::Corridor* corridor = map.find_corridor(ResourceId{"CORRIDOR-01"});
    ASSERT_NE(corridor, nullptr);

    // The head-on case the project exists for.
    EXPECT_TRUE(domain::can_deadlock_head_on(*corridor));
    EXPECT_EQ(corridor->edges.size(), 3U);
}

/// Three edges, one resource. This is what makes the corridor — not each edge —
/// admit one robot at a time (docs/00_MASTER_PLAN.md §4.2).
TEST(MapLoader, reference_map_corridor_edges_share_one_resource) {
    auto result = load_map_from_file(std::filesystem::path{TC_REFERENCE_MAP_PATH});
    ASSERT_TRUE(result.has_value());
    const Map map = std::move(result).value();

    for (const auto& edge_id : {EdgeId{"E-C1"}, EdgeId{"E-C2"}, EdgeId{"E-C3"}}) {
        EXPECT_EQ(map.resource_for_edge(edge_id), ResourceId{"CORRIDOR-01"}) << edge_id;
    }
    EXPECT_EQ(map.edges_for_resource(ResourceId{"CORRIDOR-01"}).size(), 3U);
}

TEST(MapLoader, reference_map_intersection_defines_conflicting_movements) {
    auto result = load_map_from_file(std::filesystem::path{TC_REFERENCE_MAP_PATH});
    ASSERT_TRUE(result.has_value());
    const Map map = std::move(result).value();

    const domain::Intersection* crossing = map.find_intersection(ResourceId{"INT-01"});
    ASSERT_NE(crossing, nullptr);
    EXPECT_EQ(crossing->movements.size(), 6U);

    // Everything using the corridor mouth excludes everything else using it.
    EXPECT_TRUE(domain::movements_conflict(
        *crossing, MovementId{"M-CORRIDOR-TO-A"}, MovementId{"M-A-TO-CORRIDOR"}));

    const auto found = domain::find_movement(*crossing, EdgeId{"E-C3"}, EdgeId{"E-XA"});
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, MovementId{"M-CORRIDOR-TO-A"});
}

/// A head-on deadlock in a corridor with no alternative route needs somewhere
/// for one robot to stand aside.
TEST(MapLoader, reference_map_provides_a_waiting_bay) {
    auto result = load_map_from_file(std::filesystem::path{TC_REFERENCE_MAP_PATH});
    ASSERT_TRUE(result.has_value());
    const Map map = std::move(result).value();

    const domain::WaitingBay* bay = map.find_waiting_bay(ResourceId{"BAY-01"});
    ASSERT_NE(bay, nullptr);
    EXPECT_EQ(bay->node_id, NodeId{"BAY"});
    EXPECT_TRUE(domain::accepts_robot_type(*bay, "ANY"));
}

TEST(MapLoader, reference_map_is_fully_connected) {
    auto result = load_map_from_file(std::filesystem::path{TC_REFERENCE_MAP_PATH});
    ASSERT_TRUE(result.has_value());
    const Map map = std::move(result).value();

    EXPECT_EQ(map.reachable_from(NodeId{"ENTRY"}).size(), map.node_count());
    EXPECT_TRUE(map.is_reachable(NodeId{"ENTRY"}, NodeId{"A-DOCK"}));
    EXPECT_TRUE(map.is_reachable(NodeId{"ENTRY"}, NodeId{"CHARGE"}));
}

// ================================================================ round trip

/// Round-tripping is what lets a map be edited by a tool and still reviewed as
/// text in a pull request.
TEST(MapLoader, save_and_load_round_trips) {
    auto original = load_map_from_file(std::filesystem::path{TC_REFERENCE_MAP_PATH});
    ASSERT_TRUE(original.has_value());
    const Map before = std::move(original).value();

    const std::string json = save_map_to_json(before);

    auto reloaded = load_map_from_json(json);
    ASSERT_TRUE(reloaded.has_value()) << (reloaded.has_value() ? "" : reloaded.error().to_string());
    const Map after = std::move(reloaded).value();

    EXPECT_EQ(after.id(), before.id());
    EXPECT_EQ(after.version(), before.version());
    EXPECT_EQ(after.node_count(), before.node_count());
    EXPECT_EQ(after.edge_count(), before.edge_count());
    EXPECT_EQ(after.corridors().size(), before.corridors().size());
    EXPECT_EQ(after.intersections().size(), before.intersections().size());
    EXPECT_EQ(after.waiting_bays().size(), before.waiting_bays().size());

    const domain::Intersection* crossing = after.find_intersection(ResourceId{"INT-01"});
    ASSERT_NE(crossing, nullptr);
    EXPECT_TRUE(domain::movements_conflict(
        *crossing, MovementId{"M-CORRIDOR-TO-A"}, MovementId{"M-A-TO-CORRIDOR"}))
        << "conflict groups must survive the round trip";

    EXPECT_EQ(after.resource_for_edge(EdgeId{"E-C1"}), ResourceId{"CORRIDOR-01"});
}

TEST(MapLoader, save_to_file_then_load_it_back) {
    auto original = load_map_from_json(kMinimalMap);
    ASSERT_TRUE(original.has_value());

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "tc_map_round_trip.json";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    const auto saved = save_map_to_file(std::move(original).value(), path);
    ASSERT_TRUE(saved.has_value());

    auto reloaded = load_map_from_file(path);
    ASSERT_TRUE(reloaded.has_value());
    EXPECT_EQ(std::move(reloaded).value().id(), "minimal");

    std::filesystem::remove(path, ec);
}

}  // namespace
}  // namespace traffic::map
