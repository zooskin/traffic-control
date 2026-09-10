/// Tests for the domain entities.
///
/// Naming follows docs/20_CODING_GUIDELINES.md §45.
///
/// The emphasis is on the validation docs/24_DOMAIN_MODEL.md §34 asks for —
/// invalid state should be impossible to construct, not merely discouraged.

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/conflict.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/graph.h"
#include "traffic/domain/reservation.h"
#include "traffic/domain/robot.h"
#include "traffic/domain/route.h"
#include "traffic/domain/task.h"
#include "traffic/domain/traffic_decision.h"
#include "traffic/domain/traffic_event.h"
#include "traffic/domain/values.h"

namespace traffic::domain {
namespace {

using core::EdgeId;
using core::kTimeOrigin;
using core::NodeId;
using core::ReservationId;
using core::ResourceId;
using core::RobotId;
using core::RouteId;
using core::Seconds;
using core::TaskId;

// ==================================================================== Node

TEST(Node, make_node_accepts_a_valid_node) {
    const auto result = make_node(NodeId{"N001"}, Position{1.0, 2.0}, NodeType::normal, 1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().id, NodeId{"N001"});
    EXPECT_EQ(result.value().capacity, 1U);
}

TEST(Node, make_node_rejects_an_empty_id) {
    const auto result = make_node(NodeId{""}, Position{}, NodeType::normal, 1);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::empty_id);
}

TEST(Node, make_node_rejects_zero_capacity) {
    const auto result = make_node(NodeId{"N001"}, Position{}, NodeType::normal, 0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::non_positive_value);
}

TEST(Node, node_compares_by_identity) {
    // docs/24_DOMAIN_MODEL.md §33 — same id, different position, same node.
    Node a;
    a.id = NodeId{"N001"};
    a.position = Position{0.0, 0.0};

    Node b;
    b.id = NodeId{"N001"};
    b.position = Position{99.0, 99.0};

    EXPECT_EQ(a, b);
}

/// The nine types of docs/03_MAP_GRAPH.md §5 — see decision D-006.
TEST(Node, node_type_names_round_trip) {
    for (const auto type : {NodeType::normal,
                            NodeType::intersection,
                            NodeType::station,
                            NodeType::pickup,
                            NodeType::dropoff,
                            NodeType::charger,
                            NodeType::waiting_bay,
                            NodeType::entry,
                            NodeType::exit}) {
        const auto parsed = node_type_from_string(to_string(type));
        ASSERT_TRUE(parsed.has_value()) << to_string(type);
        EXPECT_EQ(*parsed, type);
    }
}

TEST(Node, node_type_uses_the_vocabulary_of_the_map_specification) {
    // D-006: docs/01 and docs/06 use these names too; docs/24 §7 was the
    // outlier and does not win here.
    EXPECT_EQ(to_string(NodeType::waiting_bay), "WAITING_BAY");
    EXPECT_EQ(to_string(NodeType::charger), "CHARGER");
    EXPECT_EQ(to_string(NodeType::station), "STATION");
}

// ------------------------------------------------------------ travel time

TEST(Edge, edge_travel_time_derives_from_length_and_speed) {
    const auto edge =
        make_edge(EdgeId{"E1"}, NodeId{"A"}, NodeId{"B"}, 10.0, 2.0, EdgeDirection::forward);
    ASSERT_TRUE(edge.has_value());

    EXPECT_EQ(nominal_travel_time(edge.value()), core::Duration{Seconds{5}});
}

/// A lift or a powered door takes a time unrelated to the distance covered —
/// decision D-007.
TEST(Edge, edge_travel_time_override_wins) {
    auto edge =
        make_edge(EdgeId{"E1"}, NodeId{"A"}, NodeId{"B"}, 10.0, 2.0, EdgeDirection::forward);
    ASSERT_TRUE(edge.has_value());

    Edge lift = edge.value();
    lift.travel_time_override = core::Duration{Seconds{30}};

    EXPECT_EQ(nominal_travel_time(lift), core::Duration{Seconds{30}});
}

// ==================================================================== Edge

TEST(Edge, make_edge_accepts_a_valid_edge) {
    const auto result = make_edge(
        EdgeId{"E001"}, NodeId{"N001"}, NodeId{"N002"}, 10.0, 1.5, EdgeDirection::forward);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().length, 10.0);
}

TEST(Edge, make_edge_rejects_a_self_loop) {
    const auto result = make_edge(
        EdgeId{"E001"}, NodeId{"N001"}, NodeId{"N001"}, 10.0, 1.5, EdgeDirection::forward);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::same_source_and_destination);
}

TEST(Edge, make_edge_rejects_non_positive_length) {
    const auto zero =
        make_edge(EdgeId{"E001"}, NodeId{"N001"}, NodeId{"N002"}, 0.0, 1.5, EdgeDirection::forward);
    ASSERT_FALSE(zero.has_value());
    EXPECT_EQ(zero.error(), DomainError::non_positive_value);

    const auto negative = make_edge(
        EdgeId{"E001"}, NodeId{"N001"}, NodeId{"N002"}, -1.0, 1.5, EdgeDirection::forward);
    ASSERT_FALSE(negative.has_value());
}

TEST(Edge, make_edge_rejects_non_positive_speed_limit) {
    const auto result = make_edge(
        EdgeId{"E001"}, NodeId{"N001"}, NodeId{"N002"}, 10.0, 0.0, EdgeDirection::forward);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::non_positive_value);
}

TEST(Edge, edge_forward_is_traversable_only_from_its_start) {
    const auto edge =
        make_edge(EdgeId{"E001"}, NodeId{"A"}, NodeId{"B"}, 10.0, 1.0, EdgeDirection::forward);
    ASSERT_TRUE(edge.has_value());

    EXPECT_TRUE(is_traversable_from(edge.value(), NodeId{"A"}));
    EXPECT_FALSE(is_traversable_from(edge.value(), NodeId{"B"}));
}

TEST(Edge, edge_reverse_is_traversable_only_from_its_end) {
    const auto edge =
        make_edge(EdgeId{"E001"}, NodeId{"A"}, NodeId{"B"}, 10.0, 1.0, EdgeDirection::reverse);
    ASSERT_TRUE(edge.has_value());

    EXPECT_FALSE(is_traversable_from(edge.value(), NodeId{"A"}));
    EXPECT_TRUE(is_traversable_from(edge.value(), NodeId{"B"}));
}

TEST(Edge, edge_bidirectional_is_traversable_from_both_ends) {
    const auto edge = make_edge(
        EdgeId{"E001"}, NodeId{"A"}, NodeId{"B"}, 10.0, 1.0, EdgeDirection::bidirectional);
    ASSERT_TRUE(edge.has_value());

    EXPECT_TRUE(is_traversable_from(edge.value(), NodeId{"A"}));
    EXPECT_TRUE(is_traversable_from(edge.value(), NodeId{"B"}));
}

/// A blocked corridor is disabled rather than deleted, so the map version can
/// stay stable while traffic is kept out of it.
TEST(Edge, edge_disabled_is_traversable_from_neither_end) {
    auto edge = make_edge(
        EdgeId{"E001"}, NodeId{"A"}, NodeId{"B"}, 10.0, 1.0, EdgeDirection::bidirectional);
    ASSERT_TRUE(edge.has_value());

    Edge disabled = edge.value();
    disabled.enabled = false;

    EXPECT_FALSE(is_traversable_from(disabled, NodeId{"A"}));
    EXPECT_FALSE(is_traversable_from(disabled, NodeId{"B"}));
}

TEST(Edge, edge_opposite_node_returns_the_far_end) {
    const auto edge = make_edge(
        EdgeId{"E001"}, NodeId{"A"}, NodeId{"B"}, 10.0, 1.0, EdgeDirection::bidirectional);
    ASSERT_TRUE(edge.has_value());

    EXPECT_EQ(opposite_node(edge.value(), NodeId{"A"}), NodeId{"B"});
    EXPECT_EQ(opposite_node(edge.value(), NodeId{"B"}), NodeId{"A"});
    EXPECT_FALSE(opposite_node(edge.value(), NodeId{"Z"}).has_value());
}

/// Several edges through one narrow corridor share a resource id; that is what
/// makes the corridor, not each edge, admit one robot at a time
/// (docs/00_MASTER_PLAN.md §4.2).
TEST(Edge, edge_without_a_corridor_is_its_own_resource) {
    const auto edge =
        make_edge(EdgeId{"E001"}, NodeId{"A"}, NodeId{"B"}, 10.0, 1.0, EdgeDirection::forward);
    ASSERT_TRUE(edge.has_value());

    EXPECT_EQ(effective_resource(edge.value()), ResourceId{"E001"});
}

TEST(Edge, edge_in_a_corridor_reports_the_corridor_resource) {
    auto edge =
        make_edge(EdgeId{"E001"}, NodeId{"A"}, NodeId{"B"}, 10.0, 1.0, EdgeDirection::forward);
    ASSERT_TRUE(edge.has_value());

    Edge in_corridor = edge.value();
    in_corridor.resource_id = ResourceId{"CORRIDOR-01"};

    EXPECT_EQ(effective_resource(in_corridor), ResourceId{"CORRIDOR-01"});
}

// =================================================================== Robot

TEST(Robot, make_robot_accepts_a_valid_robot) {
    const auto result = make_robot(RobotId{"R001"}, RobotCapabilities{0.8, 1.2, 1.5, 100.0});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().id, RobotId{"R001"});
}

TEST(Robot, make_robot_rejects_an_empty_id) {
    const auto result = make_robot(RobotId{""}, RobotCapabilities{});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::empty_id);
}

TEST(Robot, make_robot_rejects_negative_capabilities) {
    const auto result = make_robot(RobotId{"R001"}, RobotCapabilities{-1.0, 1.0, 1.0, 1.0});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::negative_value);
}

// ====================================================== RobotStateSnapshot

TEST(RobotStateSnapshot, snapshot_starts_at_the_requested_state) {
    const auto result = make_robot_state_snapshot(RobotId{"R001"}, RobotState::idle, kTimeOrigin);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().state, RobotState::idle);
    EXPECT_EQ(result.value().version, StateVersion{0});
}

TEST(RobotStateSnapshot, snapshot_transition_advances_state_and_version) {
    const auto initial = make_robot_state_snapshot(RobotId{"R001"}, RobotState::idle, kTimeOrigin);
    ASSERT_TRUE(initial.has_value());

    const auto moved = with_state(initial.value(), RobotState::reserving, kTimeOrigin + Seconds{1});
    ASSERT_TRUE(moved.has_value());

    EXPECT_EQ(moved.value().state, RobotState::reserving);
    EXPECT_EQ(moved.value().version, StateVersion{1});
    EXPECT_EQ(moved.value().observed_at, kTimeOrigin + Seconds{1});
}

TEST(RobotStateSnapshot, snapshot_transition_leaves_the_original_untouched) {
    const auto initial = make_robot_state_snapshot(RobotId{"R001"}, RobotState::idle, kTimeOrigin);
    ASSERT_TRUE(initial.has_value());

    const auto moved = with_state(initial.value(), RobotState::moving, kTimeOrigin);
    ASSERT_TRUE(moved.has_value());

    EXPECT_EQ(initial.value().state, RobotState::idle);
    EXPECT_EQ(initial.value().version, StateVersion{0});
}

TEST(RobotStateSnapshot, snapshot_rejects_an_illegal_transition) {
    auto initial = make_robot_state_snapshot(RobotId{"R001"}, RobotState::idle, kTimeOrigin);
    ASSERT_TRUE(initial.has_value());

    const auto blocked = with_state(initial.value(), RobotState::blocked, kTimeOrigin);
    ASSERT_FALSE(blocked.has_value());
    EXPECT_EQ(blocked.error(), DomainError::invalid_transition);
}

TEST(RobotStateSnapshot, snapshot_is_stale_after_its_max_age) {
    const auto snapshot =
        make_robot_state_snapshot(RobotId{"R001"}, RobotState::moving, kTimeOrigin);
    ASSERT_TRUE(snapshot.has_value());

    EXPECT_FALSE(is_stale(snapshot.value(), kTimeOrigin + Seconds{1}, Seconds{2}));
    EXPECT_FALSE(is_stale(snapshot.value(), kTimeOrigin + Seconds{2}, Seconds{2}))
        << "exactly at the limit is not yet stale";
    EXPECT_TRUE(is_stale(snapshot.value(), kTimeOrigin + Seconds{3}, Seconds{2}));
}

/// A robot whose clock runs ahead sends observations stamped in the future.
/// Calling those stale would discard perfectly good data.
TEST(RobotStateSnapshot, snapshot_from_the_future_is_not_stale) {
    const auto snapshot =
        make_robot_state_snapshot(RobotId{"R001"}, RobotState::moving, kTimeOrigin + Seconds{10});
    ASSERT_TRUE(snapshot.has_value());

    EXPECT_FALSE(is_stale(snapshot.value(), kTimeOrigin, Seconds{1}));
}

// ==================================================================== Task

TEST(Task, make_task_accepts_a_valid_task) {
    const auto result =
        make_task(TaskId{"TASK-00001"}, NodeId{"A"}, NodeId{"B"}, Priority{50}, kTimeOrigin);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().status, TaskStatus::created);
    EXPECT_EQ(result.value().priority, Priority{50});
}

/// A task that asks a robot to travel nowhere would occupy it and complete
/// instantly, inflating throughput while moving nothing.
TEST(Task, make_task_rejects_source_equal_to_destination) {
    const auto result =
        make_task(TaskId{"TASK-00001"}, NodeId{"A"}, NodeId{"A"}, Priority{}, kTimeOrigin);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::same_source_and_destination);
}

TEST(Task, make_task_rejects_empty_ids) {
    EXPECT_FALSE(
        make_task(TaskId{""}, NodeId{"A"}, NodeId{"B"}, Priority{}, kTimeOrigin).has_value());
    EXPECT_FALSE(
        make_task(TaskId{"T1"}, NodeId{""}, NodeId{"B"}, Priority{}, kTimeOrigin).has_value());
    EXPECT_FALSE(
        make_task(TaskId{"T1"}, NodeId{"A"}, NodeId{""}, Priority{}, kTimeOrigin).has_value());
}

TEST(Task, task_assignment_sets_the_robot_and_status) {
    const auto task = make_task(TaskId{"T1"}, NodeId{"A"}, NodeId{"B"}, Priority{}, kTimeOrigin);
    ASSERT_TRUE(task.has_value());

    const auto assigned = assign_to(task.value(), RobotId{"R001"}, kTimeOrigin + Seconds{1});
    ASSERT_TRUE(assigned.has_value());

    EXPECT_EQ(assigned.value().status, TaskStatus::assigned);
    ASSERT_TRUE(assigned.value().robot_id.has_value());
    EXPECT_EQ(*assigned.value().robot_id, RobotId{"R001"});
}

TEST(Task, task_assignment_rejects_an_empty_robot_id) {
    const auto task = make_task(TaskId{"T1"}, NodeId{"A"}, NodeId{"B"}, Priority{}, kTimeOrigin);
    ASSERT_TRUE(task.has_value());

    const auto assigned = assign_to(task.value(), RobotId{""}, kTimeOrigin);
    ASSERT_FALSE(assigned.has_value());
    EXPECT_EQ(assigned.error(), DomainError::empty_id);
}

/// Stamping the timestamps inside the transition is what stops a status and its
/// timestamp from disagreeing about what happened.
TEST(Task, task_records_start_and_completion_times) {
    auto task = make_task(TaskId{"T1"}, NodeId{"A"}, NodeId{"B"}, Priority{}, kTimeOrigin);
    ASSERT_TRUE(task.has_value());

    auto current = assign_to(task.value(), RobotId{"R001"}, kTimeOrigin);
    ASSERT_TRUE(current.has_value());
    current = with_status(current.value(), TaskStatus::planning, kTimeOrigin + Seconds{1});
    ASSERT_TRUE(current.has_value());

    current = with_status(current.value(), TaskStatus::running, kTimeOrigin + Seconds{2});
    ASSERT_TRUE(current.has_value());
    ASSERT_TRUE(current.value().started_at.has_value());
    EXPECT_EQ(*current.value().started_at, kTimeOrigin + Seconds{2});
    EXPECT_FALSE(current.value().completed_at.has_value());

    current = with_status(current.value(), TaskStatus::completed, kTimeOrigin + Seconds{9});
    ASSERT_TRUE(current.has_value());
    ASSERT_TRUE(current.value().completed_at.has_value());
    EXPECT_EQ(*current.value().completed_at, kTimeOrigin + Seconds{9});
}

TEST(Task, task_rejects_an_illegal_status_change) {
    const auto task = make_task(TaskId{"T1"}, NodeId{"A"}, NodeId{"B"}, Priority{}, kTimeOrigin);
    ASSERT_TRUE(task.has_value());

    // created -> running skips assignment and planning.
    const auto running = with_status(task.value(), TaskStatus::running, kTimeOrigin);
    ASSERT_FALSE(running.has_value());
    EXPECT_EQ(running.error(), DomainError::invalid_transition);
}

TEST(Task, task_overdue_only_when_a_deadline_has_passed) {
    auto task = make_task(TaskId{"T1"}, NodeId{"A"}, NodeId{"B"}, Priority{}, kTimeOrigin);
    ASSERT_TRUE(task.has_value());

    Task without_deadline = task.value();
    EXPECT_FALSE(is_overdue(without_deadline, kTimeOrigin + Seconds{10000}));

    Task with_deadline = task.value();
    with_deadline.deadline = kTimeOrigin + Seconds{10};
    EXPECT_FALSE(is_overdue(with_deadline, kTimeOrigin + Seconds{10}));
    EXPECT_TRUE(is_overdue(with_deadline, kTimeOrigin + Seconds{11}));
}

// =================================================================== Route

/// Builds nodes A -> B -> C with matching segments.
[[nodiscard]] std::vector<RouteSegment> make_segments() {
    RouteSegment first;
    first.edge_id = EdgeId{"E1"};
    first.from_node = NodeId{"A"};
    first.to_node = NodeId{"B"};
    first.sequence = 0;

    RouteSegment second;
    second.edge_id = EdgeId{"E2"};
    second.from_node = NodeId{"B"};
    second.to_node = NodeId{"C"};
    second.sequence = 1;

    return {first, second};
}

TEST(Route, make_route_accepts_a_connected_path) {
    const auto result = make_route(RouteId{"RT1"},
                                   RobotId{"R001"},
                                   {NodeId{"A"}, NodeId{"B"}, NodeId{"C"}},
                                   make_segments(),
                                   MapVersion{7},
                                   kTimeOrigin);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(start_node(result.value()), NodeId{"A"});
    EXPECT_EQ(goal_node(result.value()), NodeId{"C"});
    EXPECT_FALSE(is_trivial(result.value()));
}

TEST(Route, make_route_rejects_an_empty_node_list) {
    const auto result =
        make_route(RouteId{"RT1"}, RobotId{"R001"}, {}, {}, MapVersion{}, kTimeOrigin);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::empty_route);
}

/// A walk over n edges visits n+1 nodes. Anything else is not a path.
TEST(Route, make_route_rejects_a_node_and_segment_count_mismatch) {
    const auto result = make_route(RouteId{"RT1"},
                                   RobotId{"R001"},
                                   {NodeId{"A"}, NodeId{"B"}},
                                   make_segments(),  // two segments, two nodes
                                   MapVersion{},
                                   kTimeOrigin);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::inconsistent_route);
}

/// Without this check a route is reserved resource by resource and only fails
/// on the first move the robot cannot physically make.
TEST(Route, make_route_rejects_segments_that_do_not_join_up) {
    auto segments = make_segments();
    segments[1].from_node = NodeId{"Z"};  // does not continue from B

    const auto result = make_route(RouteId{"RT1"},
                                   RobotId{"R001"},
                                   {NodeId{"A"}, NodeId{"B"}, NodeId{"C"}},
                                   std::move(segments),
                                   MapVersion{},
                                   kTimeOrigin);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::inconsistent_route);
}

TEST(Route, make_route_rejects_out_of_order_sequence_numbers) {
    auto segments = make_segments();
    segments[0].sequence = 5;

    const auto result = make_route(RouteId{"RT1"},
                                   RobotId{"R001"},
                                   {NodeId{"A"}, NodeId{"B"}, NodeId{"C"}},
                                   std::move(segments),
                                   MapVersion{},
                                   kTimeOrigin);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::inconsistent_route);
}

TEST(Route, route_with_a_single_node_is_trivial) {
    const auto result =
        make_route(RouteId{"RT1"}, RobotId{"R001"}, {NodeId{"A"}}, {}, MapVersion{}, kTimeOrigin);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(is_trivial(result.value())) << "the robot is already at its goal";
    EXPECT_EQ(start_node(result.value()), goal_node(result.value()));
}

/// docs/23_SYSTEM_ARCHITECTURE.md §16: catch this before committing the route,
/// not after the robot has started moving along it.
TEST(Route, route_is_stale_against_a_newer_map) {
    const auto result = make_route(RouteId{"RT1"},
                                   RobotId{"R001"},
                                   {NodeId{"A"}, NodeId{"B"}, NodeId{"C"}},
                                   make_segments(),
                                   MapVersion{7},
                                   kTimeOrigin);
    ASSERT_TRUE(result.has_value());

    EXPECT_FALSE(is_stale_against_map(result.value(), MapVersion{7}));
    EXPECT_TRUE(is_stale_against_map(result.value(), MapVersion{8}));
}

TEST(Route, route_lists_the_edges_it_traverses) {
    const auto result = make_route(RouteId{"RT1"},
                                   RobotId{"R001"},
                                   {NodeId{"A"}, NodeId{"B"}, NodeId{"C"}},
                                   make_segments(),
                                   MapVersion{},
                                   kTimeOrigin);
    ASSERT_TRUE(result.has_value());

    const auto edges = traversed_edges(result.value());
    ASSERT_EQ(edges.size(), 2U);
    EXPECT_EQ(edges[0], EdgeId{"E1"});
    EXPECT_EQ(edges[1], EdgeId{"E2"});
}

TEST(Route, route_expires_at_its_expiry_time) {
    auto result = make_route(RouteId{"RT1"},
                             RobotId{"R001"},
                             {NodeId{"A"}, NodeId{"B"}, NodeId{"C"}},
                             make_segments(),
                             MapVersion{},
                             kTimeOrigin);
    ASSERT_TRUE(result.has_value());

    Route route = result.value();
    EXPECT_FALSE(is_expired(route, kTimeOrigin + Seconds{1000})) << "no expiry set";

    route.expires_at = kTimeOrigin + Seconds{10};
    EXPECT_FALSE(is_expired(route, kTimeOrigin + Seconds{9}));
    EXPECT_TRUE(is_expired(route, kTimeOrigin + Seconds{10}));
}

// ============================================================= Reservation

[[nodiscard]] Reservation reserve(std::string_view id,
                                  std::string_view robot,
                                  std::string_view resource,
                                  int start_s,
                                  int end_s) {
    const auto result =
        make_reservation(ReservationId{std::string(id)},
                         RobotId{std::string(robot)},
                         ResourceId{std::string(resource)},
                         TimeWindow{kTimeOrigin + Seconds{start_s}, kTimeOrigin + Seconds{end_s}},
                         Priority{50},
                         kTimeOrigin);
    EXPECT_TRUE(result.has_value());
    return result.value();
}

TEST(Reservation, make_reservation_accepts_a_valid_request) {
    const auto result = make_reservation(ReservationId{"RES-00001"},
                                         RobotId{"R001"},
                                         ResourceId{"CORRIDOR-01"},
                                         TimeWindow{kTimeOrigin, kTimeOrigin + Seconds{5}},
                                         Priority{50},
                                         kTimeOrigin);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().state, ReservationState::pending);
}

TEST(Reservation, make_reservation_rejects_an_empty_window) {
    const auto result = make_reservation(ReservationId{"RES-00001"},
                                         RobotId{"R001"},
                                         ResourceId{"C1"},
                                         TimeWindow{kTimeOrigin, kTimeOrigin},
                                         Priority{},
                                         kTimeOrigin);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::invalid_time_window);
}

TEST(Reservation, make_reservation_rejects_empty_ids) {
    const TimeWindow window{kTimeOrigin, kTimeOrigin + Seconds{1}};
    EXPECT_FALSE(
        make_reservation(
            ReservationId{""}, RobotId{"R1"}, ResourceId{"C1"}, window, Priority{}, kTimeOrigin)
            .has_value());
    EXPECT_FALSE(
        make_reservation(
            ReservationId{"RES1"}, RobotId{""}, ResourceId{"C1"}, window, Priority{}, kTimeOrigin)
            .has_value());
    EXPECT_FALSE(
        make_reservation(
            ReservationId{"RES1"}, RobotId{"R1"}, ResourceId{""}, window, Priority{}, kTimeOrigin)
            .has_value());
}

TEST(Reservation, reservation_conflicts_when_windows_overlap_on_one_resource) {
    const auto a = reserve("RES1", "R001", "CORRIDOR-01", 0, 10);
    const auto b = reserve("RES2", "R002", "CORRIDOR-01", 5, 15);

    EXPECT_TRUE(conflicts_with(a, b));
    EXPECT_TRUE(conflicts_with(b, a)) << "conflict is symmetric";
}

TEST(Reservation, reservation_does_not_conflict_on_different_resources) {
    const auto a = reserve("RES1", "R001", "CORRIDOR-01", 0, 10);
    const auto b = reserve("RES2", "R002", "CORRIDOR-02", 0, 10);

    EXPECT_FALSE(conflicts_with(a, b));
}

TEST(Reservation, reservation_does_not_conflict_when_windows_are_disjoint) {
    const auto a = reserve("RES1", "R001", "CORRIDOR-01", 0, 10);
    const auto b = reserve("RES2", "R002", "CORRIDOR-01", 10, 20);

    EXPECT_FALSE(conflicts_with(a, b)) << "back-to-back handover, not a conflict";
}

/// Treating a robot's consecutive claims as conflicting produces a wait-for
/// cycle of length one — a deadlock with a single participant.
TEST(Reservation, reservation_does_not_conflict_with_itself) {
    const auto a = reserve("RES1", "R001", "CORRIDOR-01", 0, 10);
    const auto b = reserve("RES2", "R001", "CORRIDOR-01", 5, 15);

    EXPECT_FALSE(conflicts_with(a, b));
}

TEST(Reservation, reservation_released_no_longer_conflicts) {
    const auto a = reserve("RES1", "R001", "CORRIDOR-01", 0, 10);
    const auto b = reserve("RES2", "R002", "CORRIDOR-01", 5, 15);
    ASSERT_TRUE(conflicts_with(a, b));

    auto active = with_state(a, ReservationState::active);
    ASSERT_TRUE(active.has_value());
    const auto released = with_state(active.value(), ReservationState::released);
    ASSERT_TRUE(released.has_value());

    EXPECT_FALSE(conflicts_with(released.value(), b));
}

TEST(Reservation, reservation_pending_still_conflicts) {
    // A request under consideration must block conflicting grants.
    const auto a = reserve("RES1", "R001", "CORRIDOR-01", 0, 10);
    const auto b = reserve("RES2", "R002", "CORRIDOR-01", 0, 10);

    ASSERT_EQ(a.state, ReservationState::pending);
    ASSERT_EQ(b.state, ReservationState::pending);
    EXPECT_TRUE(conflicts_with(a, b));
}

TEST(Reservation, reservation_rejects_an_illegal_state_change) {
    const auto a = reserve("RES1", "R001", "CORRIDOR-01", 0, 10);

    const auto released = with_state(a, ReservationState::released);
    ASSERT_FALSE(released.has_value()) << "pending was never held";
    EXPECT_EQ(released.error(), DomainError::invalid_transition);
}

TEST(Reservation, reservation_is_past_its_window_after_the_end) {
    const auto a = reserve("RES1", "R001", "CORRIDOR-01", 0, 10);

    EXPECT_FALSE(is_past_window(a, kTimeOrigin + Seconds{9}));
    EXPECT_TRUE(is_past_window(a, kTimeOrigin + Seconds{10}));
}

TEST(Reservation, resource_type_names_round_trip) {
    for (const auto type : {ResourceType::node,
                            ResourceType::edge,
                            ResourceType::corridor,
                            ResourceType::intersection,
                            ResourceType::charging_area,
                            ResourceType::loading_area}) {
        const auto parsed = resource_type_from_string(to_string(type));
        ASSERT_TRUE(parsed.has_value()) << to_string(type);
        EXPECT_EQ(*parsed, type);
    }
}

// ================================================================ Conflict

TEST(Conflict, make_conflict_accepts_two_distinct_robots) {
    const auto result = make_conflict(core::ConflictId{"CF1"},
                                      RobotId{"R001"},
                                      RobotId{"R002"},
                                      ResourceId{"CORRIDOR-01"},
                                      ConflictType::head_on,
                                      ConflictSeverity::high,
                                      kTimeOrigin);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().type, ConflictType::head_on);
}

/// A self-conflict enters the wait-for graph as a self-loop, which every cycle
/// detector reports as a deadlock.
TEST(Conflict, make_conflict_rejects_a_robot_conflicting_with_itself) {
    const auto result = make_conflict(core::ConflictId{"CF1"},
                                      RobotId{"R001"},
                                      RobotId{"R001"},
                                      ResourceId{"C1"},
                                      ConflictType::node,
                                      ConflictSeverity::low,
                                      kTimeOrigin);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::same_source_and_destination);
}

TEST(Conflict, conflict_identifies_its_participants) {
    const auto result = make_conflict(core::ConflictId{"CF1"},
                                      RobotId{"R001"},
                                      RobotId{"R002"},
                                      ResourceId{"C1"},
                                      ConflictType::node,
                                      ConflictSeverity::low,
                                      kTimeOrigin);
    ASSERT_TRUE(result.has_value());
    const Conflict& conflict = result.value();

    EXPECT_TRUE(involves(conflict, RobotId{"R001"}));
    EXPECT_TRUE(involves(conflict, RobotId{"R002"}));
    EXPECT_FALSE(involves(conflict, RobotId{"R003"}));

    EXPECT_EQ(counterpart(conflict, RobotId{"R001"}), RobotId{"R002"});
    EXPECT_EQ(counterpart(conflict, RobotId{"R002"}), RobotId{"R001"});
    EXPECT_FALSE(counterpart(conflict, RobotId{"R003"}).has_value());
}

TEST(Conflict, conflict_type_names_round_trip) {
    for (const auto type : {ConflictType::node,
                            ConflictType::edge,
                            ConflictType::head_on,
                            ConflictType::crossing,
                            ConflictType::corridor,
                            ConflictType::resource,
                            ConflictType::temporal}) {
        const auto parsed = conflict_type_from_string(to_string(type));
        ASSERT_TRUE(parsed.has_value()) << to_string(type);
        EXPECT_EQ(*parsed, type);
    }
}

// ============================================================ TrafficEvent

TEST(TrafficEvent, make_traffic_event_accepts_a_fleet_event) {
    const auto result = make_traffic_event(core::EventId{"EV1"},
                                           TrafficEventType::map_updated,
                                           kTimeOrigin,
                                           std::nullopt,
                                           StateVersion{3},
                                           MapVersion{9});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().type(), TrafficEventType::map_updated);
    EXPECT_FALSE(result.value().robot_id().has_value());
}

/// "A robot stopped" without saying which one cannot be acted on; it would
/// reach the controller only to be silently dropped.
TEST(TrafficEvent, make_traffic_event_rejects_a_robot_event_with_no_robot) {
    const auto result = make_traffic_event(core::EventId{"EV1"},
                                           TrafficEventType::robot_stopped,
                                           kTimeOrigin,
                                           std::nullopt,
                                           StateVersion{},
                                           MapVersion{});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::empty_id);
}

TEST(TrafficEvent, make_traffic_event_rejects_an_empty_id) {
    const auto result = make_traffic_event(core::EventId{""},
                                           TrafficEventType::map_updated,
                                           kTimeOrigin,
                                           std::nullopt,
                                           StateVersion{},
                                           MapVersion{});
    ASSERT_FALSE(result.has_value());
}

TEST(TrafficEvent, traffic_event_defaults_its_correlation_to_its_own_id) {
    const auto result = make_traffic_event(core::EventId{"EV1"},
                                           TrafficEventType::map_updated,
                                           kTimeOrigin,
                                           std::nullopt,
                                           StateVersion{},
                                           MapVersion{});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().correlation_id(), core::CorrelationId{"EV1"});
}

/// An event raised against an older world has to be recognised rather than
/// acted on — docs/10_TRAFFIC_CONTROLLER.md §22.
TEST(TrafficEvent, traffic_event_detects_a_stale_state_version) {
    const auto result = make_traffic_event(core::EventId{"EV1"},
                                           TrafficEventType::map_updated,
                                           kTimeOrigin,
                                           std::nullopt,
                                           StateVersion{100},
                                           MapVersion{});
    ASSERT_TRUE(result.has_value());

    EXPECT_FALSE(result.value().is_stale(StateVersion{100}));
    EXPECT_TRUE(result.value().is_stale(StateVersion{105}));
}

/// docs/20_CODING_GUIDELINES.md §39 — events are immutable records. That is
/// enforced by there being no mutators, not by const members: events arrive out
/// of order and have to be sortable by timestamp
/// (docs/10_TRAFFIC_CONTROLLER.md §22), which const members would prevent.
TEST(TrafficEvent, traffic_event_survives_being_stored_and_reordered) {
    static_assert(std::is_copy_constructible_v<TrafficEvent>);
    static_assert(std::is_move_constructible_v<TrafficEvent>);

    std::vector<TrafficEvent> queue;
    for (const int second : {30, 10, 20}) {
        auto event = make_traffic_event(core::EventId{"EV" + std::to_string(second)},
                                        TrafficEventType::map_updated,
                                        kTimeOrigin + Seconds{second},
                                        std::nullopt,
                                        StateVersion{},
                                        MapVersion{});
        ASSERT_TRUE(event.has_value());
        queue.push_back(std::move(event).value());
    }

    std::sort(queue.begin(), queue.end(), [](const TrafficEvent& a, const TrafficEvent& b) {
        return a.timestamp() < b.timestamp();
    });

    ASSERT_EQ(queue.size(), 3U);
    EXPECT_EQ(queue[0].timestamp(), kTimeOrigin + Seconds{10});
    EXPECT_EQ(queue[2].timestamp(), kTimeOrigin + Seconds{30});
    EXPECT_EQ(queue[0].id(), core::EventId{"EV10"}) << "reordering must not corrupt the record";
}

TEST(TrafficEvent, traffic_event_type_names_round_trip) {
    for (const auto type : {TrafficEventType::robot_state_updated,
                            TrafficEventType::robot_stopped,
                            TrafficEventType::robot_blocked,
                            TrafficEventType::robot_recovered,
                            TrafficEventType::robot_failed,
                            TrafficEventType::task_created,
                            TrafficEventType::task_completed,
                            TrafficEventType::task_cancelled,
                            TrafficEventType::reservation_expired,
                            TrafficEventType::human_detected,
                            TrafficEventType::human_cleared,
                            TrafficEventType::map_updated}) {
        const auto parsed = traffic_event_type_from_string(to_string(type));
        ASSERT_TRUE(parsed.has_value()) << to_string(type);
        EXPECT_EQ(*parsed, type);
    }
}

// ========================================================= TrafficDecision

TEST(TrafficDecision, make_traffic_decision_accepts_a_valid_decision) {
    const auto result = make_traffic_decision(core::TrafficDecisionId{"D1"},
                                              RobotId{"R001"},
                                              TrafficAction::go,
                                              "NO_CONFLICT",
                                              kTimeOrigin,
                                              StateVersion{4});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().action, TrafficAction::go);
    EXPECT_EQ(result.value().reason, "NO_CONFLICT");
}

/// NFR-004 makes decisions traceable by requirement; "the reason field was
/// blank" is not an answer to why a robot waited.
TEST(TrafficDecision, make_traffic_decision_rejects_an_empty_reason) {
    const auto result = make_traffic_decision(core::TrafficDecisionId{"D1"},
                                              RobotId{"R001"},
                                              TrafficAction::wait,
                                              "",
                                              kTimeOrigin,
                                              StateVersion{});
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::empty_id);
}

TEST(TrafficDecision, traffic_decision_detects_a_stale_state_version) {
    const auto result = make_traffic_decision(core::TrafficDecisionId{"D1"},
                                              RobotId{"R001"},
                                              TrafficAction::go,
                                              "NO_CONFLICT",
                                              kTimeOrigin,
                                              StateVersion{100});
    ASSERT_TRUE(result.has_value());

    EXPECT_FALSE(is_stale(result.value(), StateVersion{100}));
    EXPECT_TRUE(is_stale(result.value(), StateVersion{101}));
}

TEST(TrafficDecision, traffic_action_halting_covers_the_stationary_actions) {
    EXPECT_TRUE(is_halting(TrafficAction::wait));
    EXPECT_TRUE(is_halting(TrafficAction::stop));
    EXPECT_TRUE(is_halting(TrafficAction::replan));

    EXPECT_FALSE(is_halting(TrafficAction::go));
    EXPECT_FALSE(is_halting(TrafficAction::hold)) << "hold moves to a holding area";
    EXPECT_FALSE(is_halting(TrafficAction::recover));
}

TEST(TrafficDecision, traffic_action_names_round_trip) {
    for (const auto action : {TrafficAction::go,
                              TrafficAction::wait,
                              TrafficAction::stop,
                              TrafficAction::replan,
                              TrafficAction::hold,
                              TrafficAction::recover}) {
        const auto parsed = traffic_action_from_string(to_string(action));
        ASSERT_TRUE(parsed.has_value()) << to_string(action);
        EXPECT_EQ(*parsed, action);
    }
}

}  // namespace
}  // namespace traffic::domain
