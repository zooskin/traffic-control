/// Tests for traffic resources.
///
/// Naming follows docs/20_CODING_GUIDELINES.md §45.
///
/// Covers the corridor, intersection, conflict group and waiting bay
/// requirements of docs/03_MAP_GRAPH.md §25.

#include "traffic/domain/resources.h"

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/graph.h"

namespace traffic::domain {
namespace {

using core::ConflictGroupId;
using core::EdgeId;
using core::MovementId;
using core::NodeId;
using core::ResourceId;

// ================================================================= Corridor

[[nodiscard]] Corridor single_lane_corridor() {
    const auto result = make_corridor(ResourceId{"CORRIDOR-01"},
                                      NodeId{"A"},
                                      NodeId{"B"},
                                      {EdgeId{"E1"}, EdgeId{"E2"}},
                                      1,
                                      EdgeDirection::bidirectional);
    EXPECT_TRUE(result.has_value());
    return result.value();
}

TEST(Corridor, make_corridor_accepts_a_valid_corridor) {
    const auto corridor = single_lane_corridor();
    EXPECT_EQ(corridor.id, ResourceId{"CORRIDOR-01"});
    EXPECT_EQ(corridor.edges.size(), 2U);
}

TEST(Corridor, make_corridor_rejects_empty_ids) {
    EXPECT_FALSE(
        make_corridor(
            ResourceId{""}, NodeId{"A"}, NodeId{"B"}, {EdgeId{"E1"}}, 1, EdgeDirection::forward)
            .has_value());
    EXPECT_FALSE(
        make_corridor(
            ResourceId{"C1"}, NodeId{""}, NodeId{"B"}, {EdgeId{"E1"}}, 1, EdgeDirection::forward)
            .has_value());
}

/// A corridor that starts and ends at the same node encloses no passage, so
/// nothing can be granted or released for it.
TEST(Corridor, make_corridor_rejects_identical_ends) {
    const auto result = make_corridor(
        ResourceId{"C1"}, NodeId{"A"}, NodeId{"A"}, {EdgeId{"E1"}}, 1, EdgeDirection::forward);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::same_source_and_destination);
}

TEST(Corridor, make_corridor_rejects_an_empty_edge_list) {
    const auto result =
        make_corridor(ResourceId{"C1"}, NodeId{"A"}, NodeId{"B"}, {}, 1, EdgeDirection::forward);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::empty_route);
}

/// Capacity zero would refuse every robot forever. Disabling the edges is the
/// recoverable way to close a corridor.
TEST(Corridor, make_corridor_rejects_zero_capacity) {
    const auto result = make_corridor(
        ResourceId{"C1"}, NodeId{"A"}, NodeId{"B"}, {EdgeId{"E1"}}, 0, EdgeDirection::forward);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::non_positive_value);
}

TEST(Corridor, corridor_reports_single_lane) {
    EXPECT_TRUE(is_single_lane(single_lane_corridor()));

    const auto wide = make_corridor(ResourceId{"C2"},
                                    NodeId{"A"},
                                    NodeId{"B"},
                                    {EdgeId{"E1"}},
                                    2,
                                    EdgeDirection::bidirectional);
    ASSERT_TRUE(wide.has_value());
    EXPECT_FALSE(is_single_lane(wide.value()));
}

/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8 — the corridors that need a held
/// direction rather than a simple occupancy count.
TEST(Corridor, corridor_head_on_risk_needs_both_directions_and_one_lane) {
    EXPECT_TRUE(can_deadlock_head_on(single_lane_corridor()));

    const auto one_way = make_corridor(
        ResourceId{"C2"}, NodeId{"A"}, NodeId{"B"}, {EdgeId{"E1"}}, 1, EdgeDirection::forward);
    ASSERT_TRUE(one_way.has_value());
    EXPECT_FALSE(can_deadlock_head_on(one_way.value())) << "one-way: no head-on";

    const auto two_lane = make_corridor(ResourceId{"C3"},
                                        NodeId{"A"},
                                        NodeId{"B"},
                                        {EdgeId{"E1"}},
                                        2,
                                        EdgeDirection::bidirectional);
    ASSERT_TRUE(two_lane.has_value());
    EXPECT_FALSE(can_deadlock_head_on(two_lane.value())) << "two lanes: they pass";
}

TEST(Corridor, corridor_other_end_returns_the_far_side) {
    const auto corridor = single_lane_corridor();

    EXPECT_EQ(other_end(corridor, NodeId{"A"}), NodeId{"B"});
    EXPECT_EQ(other_end(corridor, NodeId{"B"}), NodeId{"A"});
    EXPECT_FALSE(other_end(corridor, NodeId{"Z"}).has_value());
}

// ============================================================ Intersection

/// A crossroads: A-C runs east-west, B-D north-south. The two cross, so they
/// share a conflict group; a right turn from A to B does not.
[[nodiscard]] Intersection crossroads() {
    auto result =
        make_intersection(ResourceId{"X1"}, {NodeId{"X"}}, {EdgeId{"EA"}, EdgeId{"EB"}}, 2);
    EXPECT_TRUE(result.has_value());
    Intersection intersection = result.value();

    intersection.movements = {
        Movement{MovementId{"M_AC"}, ResourceId{"X1"}, EdgeId{"EA"}, EdgeId{"EC"}},
        Movement{MovementId{"M_BD"}, ResourceId{"X1"}, EdgeId{"EB"}, EdgeId{"ED"}},
        Movement{MovementId{"M_AB"}, ResourceId{"X1"}, EdgeId{"EA"}, EdgeId{"EB"}},
    };
    intersection.conflict_groups = {
        ConflictGroup{ConflictGroupId{"G1"}, {MovementId{"M_AC"}, MovementId{"M_BD"}}},
    };
    return intersection;
}

TEST(Intersection, make_intersection_accepts_a_valid_intersection) {
    const auto result = make_intersection(ResourceId{"X1"}, {NodeId{"X"}}, {}, 1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().capacity, 1U);
}

TEST(Intersection, make_intersection_rejects_an_empty_node_list) {
    const auto result = make_intersection(ResourceId{"X1"}, {}, {}, 1);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::empty_route);
}

TEST(Intersection, make_intersection_rejects_zero_capacity) {
    const auto result = make_intersection(ResourceId{"X1"}, {NodeId{"X"}}, {}, 0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::non_positive_value);
}

/// The reason conflict groups exist. A capacity count alone would either admit
/// the crossing pair or refuse the compatible one.
TEST(Intersection, intersection_crossing_movements_conflict) {
    const auto x = crossroads();
    EXPECT_TRUE(movements_conflict(x, MovementId{"M_AC"}, MovementId{"M_BD"}));
    EXPECT_TRUE(movements_conflict(x, MovementId{"M_BD"}, MovementId{"M_AC"}))
        << "conflict is symmetric";
}

TEST(Intersection, intersection_compatible_movements_do_not_conflict) {
    const auto x = crossroads();
    EXPECT_FALSE(movements_conflict(x, MovementId{"M_AC"}, MovementId{"M_AB"}));
}

/// The same movement is one traversal, not two competing ones.
TEST(Intersection, intersection_movement_does_not_conflict_with_itself) {
    const auto x = crossroads();
    EXPECT_FALSE(movements_conflict(x, MovementId{"M_AC"}, MovementId{"M_AC"}));
}

TEST(Intersection, intersection_unknown_movements_do_not_conflict) {
    const auto x = crossroads();
    EXPECT_FALSE(movements_conflict(x, MovementId{"M_ZZ"}, MovementId{"M_AC"}));
}

TEST(Intersection, intersection_finds_a_movement_by_its_edges) {
    const auto x = crossroads();

    const auto found = find_movement(x, EdgeId{"EA"}, EdgeId{"EC"});
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, MovementId{"M_AC"});

    EXPECT_FALSE(find_movement(x, EdgeId{"EA"}, EdgeId{"EZ"}).has_value());
}

TEST(ConflictGroup, conflict_group_reports_membership) {
    const ConflictGroup group{ConflictGroupId{"G1"}, {MovementId{"M1"}, MovementId{"M2"}}};

    EXPECT_TRUE(contains(group, MovementId{"M1"}));
    EXPECT_TRUE(contains(group, MovementId{"M2"}));
    EXPECT_FALSE(contains(group, MovementId{"M3"}));
}

// ============================================================== WaitingBay

TEST(WaitingBay, make_waiting_bay_accepts_a_valid_bay) {
    const auto result = make_waiting_bay(ResourceId{"BAY-01"}, NodeId{"N9"}, 1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().node_id, NodeId{"N9"});
}

TEST(WaitingBay, make_waiting_bay_rejects_empty_ids) {
    EXPECT_FALSE(make_waiting_bay(ResourceId{""}, NodeId{"N9"}, 1).has_value());
    EXPECT_FALSE(make_waiting_bay(ResourceId{"BAY-01"}, NodeId{""}, 1).has_value());
}

TEST(WaitingBay, make_waiting_bay_rejects_zero_capacity) {
    const auto result = make_waiting_bay(ResourceId{"BAY-01"}, NodeId{"N9"}, 0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::non_positive_value);
}

TEST(WaitingBay, waiting_bay_with_no_restriction_accepts_any_robot) {
    const auto bay = make_waiting_bay(ResourceId{"BAY-01"}, NodeId{"N9"}, 1);
    ASSERT_TRUE(bay.has_value());

    EXPECT_TRUE(accepts_robot_type(bay.value(), "AMR-SMALL"));
    EXPECT_TRUE(accepts_robot_type(bay.value(), "anything"));
}

TEST(WaitingBay, waiting_bay_honours_its_compatibility_list) {
    auto result = make_waiting_bay(ResourceId{"BAY-01"}, NodeId{"N9"}, 1);
    ASSERT_TRUE(result.has_value());

    WaitingBay bay = result.value();
    bay.compatible_robot_types = {"AMR-SMALL"};

    EXPECT_TRUE(accepts_robot_type(bay, "AMR-SMALL"));
    EXPECT_FALSE(accepts_robot_type(bay, "AMR-LARGE"))
        << "a bay too small for the robot is not an escape route";
}

}  // namespace
}  // namespace traffic::domain
