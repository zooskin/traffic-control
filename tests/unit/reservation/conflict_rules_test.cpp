/// The conflict rules. docs/24_DOMAIN_MODEL.md §15~16,
/// docs/06_TRAFFIC_RESERVATION.md §7~11, §13,
/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §7~8 and §11~12,
/// docs/03_MAP_GRAPH.md §9~12.
///
/// One rule at a time, with no map and no clock in sight. The detector's own
/// tests cover what happens when they are combined; what is checked here is
/// that each rule says what its specification says, because every one of them
/// is a place where answering "no conflict" too readily puts two robots in the
/// same metre of floor.
///
/// The two that carry the most weight are the head-on rules — the failure
/// docs/25 §8 is written about — and the intersection rule, where saying "no"
/// for two compatible movements is what keeps a crossing usable at all.

#include "traffic/reservation/conflict_rules.h"

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/domain/conflict.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/graph.h"
#include "traffic/domain/reservation.h"
#include "traffic/domain/reservation_state.h"
#include "traffic/domain/resources.h"
#include "traffic/domain/values.h"

namespace traffic::reservation {
namespace {

using core::ConflictGroupId;
using core::EdgeId;
using core::MovementId;
using core::NodeId;
using core::ResourceId;
using core::RobotId;
using domain::ConflictSeverity;
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

ResourceOccupancy occupancy(std::string robot, std::string resource, int start, int end) {
    ResourceOccupancy value;
    value.robot_id = RobotId{std::move(robot)};
    value.resource_id = ResourceId{std::move(resource)};
    value.window = window(start, end);
    return value;
}

NodeVisit visit(std::string robot, std::string node, int start, int end) {
    NodeVisit value;
    value.robot_id = RobotId{std::move(robot)};
    value.node_id = NodeId{std::move(node)};
    value.window = window(start, end);
    return value;
}

EdgeTraversal traversal(std::string robot, std::string from, std::string to, int start, int end) {
    EdgeTraversal value;
    value.robot_id = RobotId{std::move(robot)};
    value.edge_id = EdgeId{"E-AB"};
    value.from_node = NodeId{std::move(from)};
    value.to_node = NodeId{std::move(to)};
    value.resource_id = ResourceId{"E-AB"};
    value.window = window(start, end);
    return value;
}

CorridorPassage passage(std::string robot, PassageDirection direction, int start, int end) {
    CorridorPassage value;
    value.robot_id = RobotId{std::move(robot)};
    value.corridor_id = ResourceId{"CORRIDOR-01"};
    value.entry_node = NodeId{direction == PassageDirection::entry_to_exit ? "A" : "B"};
    value.exit_node = NodeId{direction == PassageDirection::entry_to_exit ? "B" : "A"};
    value.direction = direction;
    value.window = window(start, end);
    return value;
}

IntersectionCrossing crossing(std::string robot,
                              const std::optional<std::string>& way_through,
                              int start,
                              int end) {
    IntersectionCrossing value;
    value.robot_id = RobotId{std::move(robot)};
    value.intersection_id = ResourceId{"INTERSECTION-01"};
    if (way_through.has_value()) {
        value.movement_id = MovementId{way_through.value()};
    }
    value.window = window(start, end);
    return value;
}

/// A --E-AB-- B, held as one resource.
domain::Corridor corridor(std::uint32_t capacity, domain::EdgeDirection direction) {
    domain::Corridor value;
    value.id = ResourceId{"CORRIDOR-01"};
    value.entry_node = NodeId{"A"};
    value.exit_node = NodeId{"B"};
    value.edges = {EdgeId{"E-AB"}};
    value.capacity = capacity;
    value.direction = direction;
    return value;
}

domain::Movement movement(std::string id, std::string from_edge, std::string to_edge) {
    domain::Movement value;
    value.id = MovementId{std::move(id)};
    value.intersection_id = ResourceId{"INTERSECTION-01"};
    value.from_edge = EdgeId{std::move(from_edge)};
    value.to_edge = EdgeId{std::move(to_edge)};
    return value;
}

/// A four-way crossing where west-to-east and north-to-south exclude each
/// other, and south-to-north excludes neither. The second pair is the one that
/// must stay reportable as safe — docs/03_MAP_GRAPH.md §11.
domain::Intersection intersection() {
    domain::Intersection value;
    value.id = ResourceId{"INTERSECTION-01"};
    value.nodes = {NodeId{"X"}};
    value.edges = {EdgeId{"E-WX"}, EdgeId{"E-XE"}, EdgeId{"E-NX"}, EdgeId{"E-XS"}};
    value.movements = {movement("M-WE", "E-WX", "E-XE"),
                       movement("M-NS", "E-NX", "E-XS"),
                       movement("M-SN", "E-XS", "E-NX")};

    domain::ConflictGroup group;
    group.id = ConflictGroupId{"G-CROSS"};
    group.movements = {MovementId{"M-WE"}, MovementId{"M-NS"}};
    value.conflict_groups = {group};

    value.capacity = 1;
    return value;
}

/// Aggregate initialisation, because TimeWindow has no default constructor —
/// there is no sensible empty window, so a Reservation cannot be built empty
/// and filled in afterwards.
domain::Reservation reserve(const std::string& robot, std::string resource, int start, int end) {
    return domain::Reservation{
        .id = core::ReservationId{"RES-" + robot},
        .robot_id = RobotId{robot},
        .resource_id = ResourceId{std::move(resource)},
        .window = window(start, end),
        .state = domain::ReservationState::active,
        .priority = domain::Priority{50},
        .requested_at = core::kTimeOrigin,
    };
}

// ------------------------------------------------------------------ overlap

TEST(ConflictRules, overlap_of_disjoint_windows_is_empty) {
    EXPECT_FALSE(overlap_of(window(0, 10), window(20, 30)).has_value());
}

TEST(ConflictRules, overlap_of_touching_windows_is_empty) {
    // Half-open: one robot may take a corridor the instant the previous one
    // gives it back, without a fabricated gap. docs/24_DOMAIN_MODEL.md §34.
    EXPECT_FALSE(overlap_of(window(0, 10), window(10, 20)).has_value());
}

TEST(ConflictRules, overlap_of_crossing_windows_is_the_shared_span) {
    const std::optional<domain::TimeWindow> shared = overlap_of(window(0, 20), window(10, 30));
    ASSERT_TRUE(shared.has_value());
    EXPECT_EQ(shared.value(), window(10, 20));
}

// -------------------------------------------------------------------- nodes

TEST(ConflictRules, node_visited_by_two_robots_at_overlapping_times_conflicts) {
    EXPECT_TRUE(is_node_conflict(visit("R01", "N1", 0, 10), visit("R02", "N1", 5, 15)));
}

TEST(ConflictRules, node_visited_by_the_same_robot_twice_does_not_conflict) {
    // A robot passing the same node twice is one occupant. Treating it as two
    // makes a deadlock with a single participant, which nothing resolves.
    EXPECT_FALSE(is_node_conflict(visit("R01", "N1", 0, 10), visit("R01", "N1", 5, 15)));
}

TEST(ConflictRules, node_visited_at_disjoint_times_does_not_conflict) {
    EXPECT_FALSE(is_node_conflict(visit("R01", "N1", 0, 10), visit("R02", "N1", 20, 30)));
}

TEST(ConflictRules, different_nodes_never_conflict) {
    EXPECT_FALSE(is_node_conflict(visit("R01", "N1", 0, 10), visit("R02", "N2", 0, 10)));
}

// -------------------------------------------------------------------- edges

TEST(ConflictRules, edge_entered_from_the_same_end_is_a_following_conflict) {
    const EdgeTraversal leader = traversal("R01", "A", "B", 0, 10);
    const EdgeTraversal follower = traversal("R02", "A", "B", 5, 15);

    EXPECT_TRUE(is_following_conflict(leader, follower));
    EXPECT_FALSE(is_head_on_conflict(leader, follower));
    EXPECT_EQ(classify_edge_conflict(leader, follower), ConflictType::edge);
}

TEST(ConflictRules, edge_entered_from_opposite_ends_is_head_on) {
    // docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8: R01 -----> <----- R02.
    const EdgeTraversal eastbound = traversal("R01", "A", "B", 0, 10);
    const EdgeTraversal westbound = traversal("R02", "B", "A", 5, 15);

    EXPECT_TRUE(is_head_on_conflict(eastbound, westbound));
    EXPECT_FALSE(is_following_conflict(eastbound, westbound));
    EXPECT_EQ(classify_edge_conflict(eastbound, westbound), ConflictType::head_on);
}

TEST(ConflictRules, edge_used_at_disjoint_times_is_not_classified) {
    const EdgeTraversal first = traversal("R01", "A", "B", 0, 10);
    const EdgeTraversal second = traversal("R02", "B", "A", 20, 30);
    EXPECT_FALSE(classify_edge_conflict(first, second).has_value());
}

TEST(ConflictRules, different_edges_are_not_classified) {
    EdgeTraversal other = traversal("R02", "B", "C", 0, 10);
    other.edge_id = EdgeId{"E-BC"};
    EXPECT_FALSE(classify_edge_conflict(traversal("R01", "A", "B", 0, 10), other).has_value());
}

// ---------------------------------------------------------------- corridors

TEST(ConflictRules, corridor_run_in_opposite_directions_is_head_on) {
    const domain::Corridor lane = corridor(1, domain::EdgeDirection::bidirectional);
    EXPECT_TRUE(is_head_on_conflict(lane,
                                    passage("R01", PassageDirection::entry_to_exit, 0, 30),
                                    passage("R02", PassageDirection::exit_to_entry, 10, 40)));
}

TEST(ConflictRules, corridor_run_in_opposite_directions_at_disjoint_times_is_not_head_on) {
    const domain::Corridor lane = corridor(1, domain::EdgeDirection::bidirectional);
    EXPECT_FALSE(is_head_on_conflict(lane,
                                     passage("R01", PassageDirection::entry_to_exit, 0, 30),
                                     passage("R02", PassageDirection::exit_to_entry, 40, 70)));
}

TEST(ConflictRules, corridor_run_in_the_same_direction_is_not_head_on) {
    // A convoy is a capacity question, not a head-on one. Reporting it as
    // head-on would have the priority manager reverse a robot that only needs
    // to wait.
    const domain::Corridor lane = corridor(1, domain::EdgeDirection::bidirectional);
    EXPECT_FALSE(is_head_on_conflict(lane,
                                     passage("R01", PassageDirection::entry_to_exit, 0, 30),
                                     passage("R02", PassageDirection::entry_to_exit, 10, 40)));
}

TEST(ConflictRules, corridor_with_room_to_pass_is_not_head_on) {
    // Two lanes: opposing traffic is legal and simultaneous, so this is not the
    // deadlock docs/25 §8 describes.
    const domain::Corridor lane = corridor(2, domain::EdgeDirection::bidirectional);
    EXPECT_FALSE(is_head_on_conflict(lane,
                                     passage("R01", PassageDirection::entry_to_exit, 0, 30),
                                     passage("R02", PassageDirection::exit_to_entry, 10, 40)));
}

TEST(ConflictRules, one_way_corridor_is_not_head_on) {
    const domain::Corridor lane = corridor(1, domain::EdgeDirection::forward);
    EXPECT_FALSE(is_head_on_conflict(lane,
                                     passage("R01", PassageDirection::entry_to_exit, 0, 30),
                                     passage("R02", PassageDirection::exit_to_entry, 10, 40)));
}

// ------------------------------------------------------------ intersections

TEST(ConflictRules, movements_in_one_conflict_group_cannot_cross_together) {
    const domain::Intersection crossroads = intersection();
    EXPECT_TRUE(is_crossing_conflict(
        crossroads, crossing("R01", "M-WE", 0, 10), crossing("R02", "M-NS", 5, 15)));
}

TEST(ConflictRules, movements_in_different_conflict_groups_cross_together) {
    // The case a plain occupancy count gets wrong, and the reason
    // docs/03_MAP_GRAPH.md §11 exists.
    const domain::Intersection crossroads = intersection();
    EXPECT_FALSE(is_crossing_conflict(
        crossroads, crossing("R01", "M-WE", 0, 10), crossing("R02", "M-SN", 5, 15)));
}

TEST(ConflictRules, conflicting_movements_at_disjoint_times_do_not_conflict) {
    const domain::Intersection crossroads = intersection();
    EXPECT_FALSE(is_crossing_conflict(
        crossroads, crossing("R01", "M-WE", 0, 10), crossing("R02", "M-NS", 20, 30)));
}

TEST(ConflictRules, crossing_with_an_undefined_movement_conflicts) {
    // Fail-closed. A way through the map does not describe cannot be shown to
    // be compatible with anything, and the cheap mistake is the waiting one.
    const domain::Intersection crossroads = intersection();
    EXPECT_TRUE(is_crossing_conflict(
        crossroads, crossing("R01", std::nullopt, 0, 10), crossing("R02", "M-SN", 5, 15)));
}

// -------------------------------------------------------- resource capacity

TEST(ConflictRules, temporal_overlap_on_one_resource_conflicts) {
    EXPECT_TRUE(is_temporal_conflict(occupancy("R01", "C1", 0, 10), occupancy("R02", "C1", 5, 15)));
}

TEST(ConflictRules, temporal_overlap_by_the_same_robot_does_not_conflict) {
    const ResourceOccupancy earlier = occupancy("R01", "C1", 0, 10);
    const ResourceOccupancy later = occupancy("R01", "C1", 5, 15);
    EXPECT_FALSE(is_temporal_conflict(earlier, later));
}

TEST(ConflictRules, temporal_overlap_on_different_resources_does_not_conflict) {
    const ResourceOccupancy here = occupancy("R01", "C1", 0, 10);
    const ResourceOccupancy elsewhere = occupancy("R02", "C2", 5, 15);
    EXPECT_FALSE(is_temporal_conflict(here, elsewhere));
}

TEST(ConflictRules, single_lane_capacity_rejects_any_overlapping_pair) {
    const std::vector<ResourceOccupancy> holders{occupancy("R01", "C1", 0, 10),
                                                 occupancy("R02", "C1", 5, 15)};
    EXPECT_TRUE(is_capacity_conflict(holders[0], holders[1], holders, 1));
}

TEST(ConflictRules, capacity_of_two_admits_two_overlapping_holders) {
    const std::vector<ResourceOccupancy> holders{occupancy("R01", "C1", 0, 30),
                                                 occupancy("R02", "C1", 10, 40)};
    EXPECT_TRUE(is_temporal_conflict(holders[0], holders[1]));
    EXPECT_FALSE(is_capacity_conflict(holders[0], holders[1], holders, 2));
}

TEST(ConflictRules, capacity_of_two_rejects_three_overlapping_holders) {
    // docs/06_TRAFFIC_RESERVATION.md §7: the conflict is not the overlap, it is
    // "overlapping reservations > resource.capacity". Every pair is in it.
    const std::vector<ResourceOccupancy> holders{occupancy("R01", "C1", 0, 30),
                                                 occupancy("R02", "C1", 10, 40),
                                                 occupancy("R03", "C1", 20, 50)};
    EXPECT_TRUE(is_capacity_conflict(holders[0], holders[1], holders, 2));
    EXPECT_TRUE(is_capacity_conflict(holders[0], holders[2], holders, 2));
    EXPECT_TRUE(is_capacity_conflict(holders[1], holders[2], holders, 2));
}

TEST(ConflictRules, capacity_counts_one_robot_once_however_many_windows_it_holds) {
    // A robot moving between two edges of one corridor holds two adjacent
    // windows of it. Counting that as two occupants would report a full
    // corridor with one robot inside.
    const std::vector<ResourceOccupancy> holders{occupancy("R01", "C1", 0, 30),
                                                 occupancy("R01", "C1", 10, 40),
                                                 occupancy("R02", "C1", 15, 45)};
    EXPECT_FALSE(is_capacity_conflict(holders[0], holders[2], holders, 2));
}

TEST(ConflictRules, holders_of_other_resources_do_not_count_towards_capacity) {
    const std::vector<ResourceOccupancy> holders{occupancy("R01", "C1", 0, 30),
                                                 occupancy("R02", "C1", 10, 40),
                                                 occupancy("R03", "C2", 10, 40)};
    EXPECT_FALSE(is_capacity_conflict(holders[0], holders[1], holders, 2));
}

TEST(ConflictRules, reservation_projects_to_the_occupancy_it_claims) {
    const ResourceOccupancy projected = as_occupancy(reserve("R01", "C1", 5, 25));
    EXPECT_EQ(projected.robot_id, RobotId{"R01"});
    EXPECT_EQ(projected.resource_id, ResourceId{"C1"});
    EXPECT_EQ(projected.window, window(5, 25));
}

TEST(ConflictRules, node_visit_projects_to_the_node_as_its_own_resource) {
    EXPECT_EQ(as_occupancy(visit("R01", "N1", 0, 10)).resource_id, ResourceId{"N1"});
}

// ----------------------------------------------------------- classification

TEST(ConflictRules, precedence_puts_head_on_above_every_other_type) {
    EXPECT_GT(conflict_type_precedence(ConflictType::head_on),
              conflict_type_precedence(ConflictType::crossing));
    EXPECT_GT(conflict_type_precedence(ConflictType::head_on),
              conflict_type_precedence(ConflictType::corridor));
    EXPECT_GT(conflict_type_precedence(ConflictType::head_on),
              conflict_type_precedence(ConflictType::temporal));
}

TEST(ConflictRules, precedence_puts_the_corridor_above_the_edge_inside_it) {
    EXPECT_GT(conflict_type_precedence(ConflictType::corridor),
              conflict_type_precedence(ConflictType::edge));
}

TEST(ConflictRules, precedence_puts_temporal_last) {
    EXPECT_LT(conflict_type_precedence(ConflictType::temporal),
              conflict_type_precedence(ConflictType::resource));
}

// --------------------------------------------------------------- severity

TEST(ConflictRules, severity_is_low_when_the_meeting_is_far_ahead) {
    EXPECT_EQ(severity_for(ConflictType::node, window(100, 120), at(0), SeverityThresholds{}),
              ConflictSeverity::low);
}

TEST(ConflictRules, severity_is_medium_when_a_decision_is_due_this_cycle) {
    EXPECT_EQ(severity_for(ConflictType::corridor, window(5, 20), at(0), SeverityThresholds{}),
              ConflictSeverity::medium);
}

TEST(ConflictRules, severity_is_high_when_the_meeting_has_already_begun) {
    EXPECT_EQ(severity_for(ConflictType::edge, window(0, 20), at(10), SeverityThresholds{}),
              ConflictSeverity::high);
}

TEST(ConflictRules, severity_of_a_distant_head_on_is_never_low) {
    // Waiting does not resolve a head-on: once both robots are inside a
    // single-lane corridor no ordering of them exists
    // (docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8), so the decision has to be
    // taken before the second one enters.
    EXPECT_EQ(severity_for(ConflictType::head_on, window(100, 120), at(0), SeverityThresholds{}),
              ConflictSeverity::medium);
}

TEST(ConflictRules, severity_thresholds_are_configurable) {
    SeverityThresholds patient;
    patient.absorbed_lead = seconds(600);
    patient.critical_lead = seconds(60);

    EXPECT_EQ(severity_for(ConflictType::node, window(100, 120), at(0), patient),
              ConflictSeverity::medium);
}

TEST(ConflictRules, severity_thresholds_reject_a_critical_lead_above_the_absorbed_lead) {
    SeverityThresholds crossed;
    crossed.absorbed_lead = seconds(5);
    crossed.critical_lead = seconds(10);

    const auto result = make_severity_thresholds(crossed);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), domain::DomainError::invalid_time_window);
}

TEST(ConflictRules, severity_thresholds_reject_a_negative_lead) {
    SeverityThresholds negative;
    negative.critical_lead = seconds(-1);

    const auto result = make_severity_thresholds(negative);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), domain::DomainError::negative_value);
}

TEST(ConflictRules, severity_thresholds_accept_the_defaults) {
    EXPECT_TRUE(make_severity_thresholds(SeverityThresholds{}).has_value());
}

// --------------------------------------------------------------- identity

TEST(ConflictRules, conflict_id_is_the_same_whichever_order_the_robots_are_given) {
    // The property that makes A-versus-B and B-versus-A one conflict rather
    // than two.
    const core::ConflictId forward = make_conflict_id(
        "CONF", ConflictType::head_on, ResourceId{"C1"}, RobotId{"R01"}, RobotId{"R02"}, at(15));
    const core::ConflictId reversed = make_conflict_id(
        "CONF", ConflictType::head_on, ResourceId{"C1"}, RobotId{"R02"}, RobotId{"R01"}, at(15));

    EXPECT_EQ(forward, reversed);
}

TEST(ConflictRules, conflict_id_differs_when_the_resource_differs) {
    const core::ConflictId first = make_conflict_id(
        "CONF", ConflictType::head_on, ResourceId{"C1"}, RobotId{"R01"}, RobotId{"R02"}, at(15));
    const core::ConflictId second = make_conflict_id(
        "CONF", ConflictType::head_on, ResourceId{"C2"}, RobotId{"R01"}, RobotId{"R02"}, at(15));

    EXPECT_NE(first, second);
}

TEST(ConflictRules, conflict_id_differs_when_the_meeting_starts_elsewhere) {
    const core::ConflictId first = make_conflict_id(
        "CONF", ConflictType::corridor, ResourceId{"C1"}, RobotId{"R01"}, RobotId{"R02"}, at(15));
    const core::ConflictId second = make_conflict_id(
        "CONF", ConflictType::corridor, ResourceId{"C1"}, RobotId{"R01"}, RobotId{"R02"}, at(16));

    EXPECT_NE(first, second);
}

TEST(ConflictRules, conflict_id_reads_as_the_conflict_it_names) {
    // docs/24_DOMAIN_MODEL.md §26 asks for identifiers a person can read in a
    // log without a lookup table.
    const core::ConflictId id = make_conflict_id("CONF",
                                                 ConflictType::head_on,
                                                 ResourceId{"CORRIDOR-01"},
                                                 RobotId{"R02"},
                                                 RobotId{"R01"},
                                                 core::kTimeOrigin);
    EXPECT_EQ(id.value(), "CONF-HEAD_ON-CORRIDOR-01-R01-R02-0");
}

TEST(ConflictRules, passage_direction_names_are_stable) {
    EXPECT_EQ(to_string(PassageDirection::entry_to_exit), "ENTRY_TO_EXIT");
    EXPECT_EQ(to_string(PassageDirection::exit_to_entry), "EXIT_TO_ENTRY");
}

}  // namespace
}  // namespace traffic::reservation
