#pragma once

/// \file
/// The map and the small builders the reservation tests share.
///
/// One map, carrying every resource shape Phase 6 has to get right:
///
///   * `CORRIDOR-01` — three edges, capacity one, bidirectional. The shape
///     docs/00_MASTER_PLAN.md §4.2 is about, and the one where a head-on is an
///     immediate collision.
///   * `CORRIDOR-02` — two edges, capacity **two**, bidirectional. This is what
///     tells a direction rule from a capacity rule: capacity alone would let
///     two robots in from opposite ends.
///   * `INTERSECTION-01` — capacity two, with one conflict group. Two robots
///     may cross together, but not on the two movements that cross each other.
///   * `BAY-01`, and `E-SOLO`, an edge that belongs to no larger resource and
///     is therefore its own.

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/graph.h"
#include "traffic/domain/reservation.h"
#include "traffic/domain/resources.h"
#include "traffic/domain/values.h"
#include "traffic/map/map.h"
#include "traffic/map/map_validator.h"
#include "traffic/reservation/reservation_policy.h"
#include "traffic/reservation/reservation_request.h"
#include "traffic/reservation/reservation_table.h"

namespace traffic::reservation::test {

inline core::Duration seconds(int count) {
    return std::chrono::duration_cast<core::Duration>(core::Seconds{count});
}

inline core::TimePoint at(int second) {
    return core::kTimeOrigin + seconds(second);
}

inline domain::Node node(std::string id, double x, double y) {
    domain::Node value;
    value.id = core::NodeId{std::move(id)};
    value.position = domain::Position{x, y, 0.0};
    value.type = domain::NodeType::normal;
    value.capacity = 1;
    return value;
}

inline domain::Edge edge(std::string id, std::string from, std::string to, double length) {
    domain::Edge value;
    value.id = core::EdgeId{std::move(id)};
    value.from_node = core::NodeId{std::move(from)};
    value.to_node = core::NodeId{std::move(to)};
    value.length = length;
    value.width = 1.5;
    value.speed_limit = 1.0;
    value.direction = domain::EdgeDirection::bidirectional;
    value.capacity = 1;
    return value;
}

inline map::Map build(map::MapData data) {
    auto result = map::make_map(std::move(data));
    EXPECT_TRUE(result.has_value()) << (result.has_value() ? "" : result.error().to_string());
    return std::move(result).value();
}

/// The one map the reservation tests use.
inline map::MapData reference_data() {
    map::MapData data;
    data.map_id = "reservation";
    data.version = domain::MapVersion{1};

    data.nodes = {
        // CORRIDOR-01
        node("A", 0.0, 0.0),
        node("M1", 10.0, 0.0),
        node("M2", 20.0, 0.0),
        node("B", 30.0, 0.0),
        // CORRIDOR-02
        node("P", 0.0, 20.0),
        node("Q", 10.0, 20.0),
        node("R", 20.0, 20.0),
        // BAY-01
        node("W", 30.0, 20.0),
        // INTERSECTION-01
        node("X", 50.0, 0.0),
        node("XN", 50.0, 10.0),
        node("XS", 50.0, -10.0),
        node("XW", 40.0, 0.0),
        node("XE", 60.0, 0.0),
    };

    data.edges = {
        edge("E-C1", "A", "M1", 10.0),
        edge("E-C2", "M1", "M2", 10.0),
        edge("E-C3", "M2", "B", 10.0),
        edge("E-D1", "P", "Q", 10.0),
        edge("E-D2", "Q", "R", 10.0),
        edge("E-RW", "R", "W", 10.0),
        edge("E-SOLO", "B", "P", 40.0),
        edge("E-BXW", "B", "XW", 15.0),
        edge("E-NX", "XN", "X", 10.0),
        edge("E-XS", "X", "XS", 10.0),
        edge("E-WX", "XW", "X", 10.0),
        edge("E-XE", "X", "XE", 10.0),
    };

    for (domain::Edge& value : data.edges) {
        const std::string& id = value.id.value();
        if (id == "E-C1" || id == "E-C2" || id == "E-C3") {
            value.resource_id = core::ResourceId{"CORRIDOR-01"};
        } else if (id == "E-D1" || id == "E-D2") {
            value.resource_id = core::ResourceId{"CORRIDOR-02"};
        }
    }

    for (domain::Node& value : data.nodes) {
        if (value.id.value() == "X") {
            value.type = domain::NodeType::intersection;
            value.resource_id = core::ResourceId{"INTERSECTION-01"};
        } else if (value.id.value() == "W") {
            value.type = domain::NodeType::waiting_bay;
            value.resource_id = core::ResourceId{"BAY-01"};
        }
    }

    domain::Corridor narrow;
    narrow.id = core::ResourceId{"CORRIDOR-01"};
    narrow.entry_node = core::NodeId{"A"};
    narrow.exit_node = core::NodeId{"B"};
    narrow.edges = {core::EdgeId{"E-C1"}, core::EdgeId{"E-C2"}, core::EdgeId{"E-C3"}};
    narrow.capacity = 1;
    narrow.direction = domain::EdgeDirection::bidirectional;

    domain::Corridor wide;
    wide.id = core::ResourceId{"CORRIDOR-02"};
    wide.entry_node = core::NodeId{"P"};
    wide.exit_node = core::NodeId{"R"};
    wide.edges = {core::EdgeId{"E-D1"}, core::EdgeId{"E-D2"}};
    wide.capacity = 2;
    wide.direction = domain::EdgeDirection::bidirectional;

    data.corridors = {narrow, wide};

    domain::Movement north_south;
    north_south.id = core::MovementId{"M-NS"};
    north_south.intersection_id = core::ResourceId{"INTERSECTION-01"};
    north_south.from_edge = core::EdgeId{"E-NX"};
    north_south.to_edge = core::EdgeId{"E-XS"};

    domain::Movement west_east;
    west_east.id = core::MovementId{"M-WE"};
    west_east.intersection_id = core::ResourceId{"INTERSECTION-01"};
    west_east.from_edge = core::EdgeId{"E-WX"};
    west_east.to_edge = core::EdgeId{"E-XE"};

    domain::Movement east_west;
    east_west.id = core::MovementId{"M-EW"};
    east_west.intersection_id = core::ResourceId{"INTERSECTION-01"};
    east_west.from_edge = core::EdgeId{"E-XE"};
    east_west.to_edge = core::EdgeId{"E-WX"};

    domain::ConflictGroup crossing;
    crossing.id = core::ConflictGroupId{"CG-CROSS"};
    crossing.movements = {core::MovementId{"M-NS"}, core::MovementId{"M-WE"}};

    domain::Intersection junction;
    junction.id = core::ResourceId{"INTERSECTION-01"};
    junction.nodes = {core::NodeId{"X"}};
    junction.edges = {
        core::EdgeId{"E-NX"}, core::EdgeId{"E-XS"}, core::EdgeId{"E-WX"}, core::EdgeId{"E-XE"}};
    junction.movements = {north_south, west_east, east_west};
    junction.conflict_groups = {crossing};
    junction.capacity = 2;

    data.intersections = {junction};

    domain::WaitingBay bay;
    bay.id = core::ResourceId{"BAY-01"};
    bay.node_id = core::NodeId{"W"};
    bay.capacity = 1;

    data.waiting_bays = {bay};

    return data;
}

inline map::Map reference_map() {
    return build(reference_data());
}

// ------------------------------------------------------------------ builders

inline core::ResourceId resource(std::string id) {
    return core::ResourceId{std::move(id)};
}

/// A request for \p resource_id from \p robot, running from \p start_second for
/// \p duration_seconds.
inline ReservationRequest request(std::string reservation_id,
                                  std::string robot_id,
                                  std::string resource_id,
                                  int start_second,
                                  int duration_seconds) {
    ReservationRequest value;
    value.reservation_id = core::ReservationId{std::move(reservation_id)};
    value.robot_id = core::RobotId{std::move(robot_id)};
    value.resource_id = core::ResourceId{std::move(resource_id)};
    value.requested_start = at(start_second);
    value.estimated_duration = seconds(duration_seconds);
    value.priority = domain::Priority{0};
    return value;
}

/// A record built straight from a window, for the table tests and for
/// recovery — it skips the manager on purpose, so a table test cannot be
/// passed by a manager that happens to agree with it.
inline ReservationRecord record(std::string reservation_id,
                                std::string robot_id,
                                std::string resource_id,
                                int start_second,
                                int end_second,
                                domain::ReservationState state = domain::ReservationState::pending,
                                ResourceUsage usage = ResourceUsage{}) {
    auto reservation =
        domain::make_reservation(core::ReservationId{std::move(reservation_id)},
                                 core::RobotId{std::move(robot_id)},
                                 core::ResourceId{std::move(resource_id)},
                                 domain::TimeWindow{at(start_second), at(end_second)},
                                 domain::Priority{0},
                                 at(start_second));
    EXPECT_TRUE(reservation.has_value());

    domain::Reservation built = std::move(reservation).value();
    built.state = state;

    return make_record(std::move(built),
                       std::move(usage),
                       at(start_second),
                       seconds(end_second - start_second),
                       at(start_second),
                       std::nullopt,
                       std::nullopt);
}

/// A policy with the buffers switched off, so that a test which is about
/// overlap says exactly which instants it means. The buffers have their own
/// tests.
inline ReservationPolicy unbuffered_policy() {
    ReservationPolicy policy;
    policy.entry_buffer = core::Duration::zero();
    policy.exit_buffer = core::Duration::zero();
    policy.horizon = seconds(3600);
    policy.grant_timeout = seconds(3600);
    return policy;
}

}  // namespace traffic::reservation::test
