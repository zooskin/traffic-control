#pragma once

/// \file
/// Traffic resources: the spaces robots compete for.
/// docs/03_MAP_GRAPH.md §9~14.
///
/// This is the file docs/00_MASTER_PLAN.md §4.2 is about. A narrow corridor is
/// not "an edge with a low capacity" — it is a resource that several edges lie
/// inside, and the resource, not the edge, is what admits one robot at a time.
/// Model it as edges and two robots will be granted opposite ends of the same
/// passage.
///
/// Intersections need more than a capacity. Two robots crossing an intersection
/// on paths that do not touch can proceed together; two whose paths cross
/// cannot. That is what conflict groups (§11) and movements (§12) express, and
/// a plain occupancy count cannot.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/graph.h"

namespace traffic::domain {

/// A narrow run of edges treated as one indivisible resource.
/// docs/03_MAP_GRAPH.md §9.
struct Corridor {
    core::ResourceId id;

    /// Where the corridor starts and ends. Robots enter at one and leave at
    /// the other; for a bidirectional corridor either end can serve as entry.
    core::NodeId entry_node;
    core::NodeId exit_node;

    /// The edges inside it, in order from entry to exit.
    std::vector<core::EdgeId> edges;

    /// How many robots may be inside at once. One for a single lane.
    std::uint32_t capacity{1};

    /// Which way traffic may flow.
    ///
    /// `bidirectional` with capacity 1 is the hard case this project exists
    /// for: both directions are legal but only one at a time, so the
    /// controller has to choose and hold a direction
    /// (docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §7).
    EdgeDirection direction{EdgeDirection::bidirectional};

    /// Fastest and slowest a robot is expected to take to clear it.
    ///
    /// The maximum is what lets a robot that has stalled inside be noticed:
    /// past it, something is wrong rather than merely slow.
    std::optional<core::Duration> min_travel_time;
    std::optional<core::Duration> max_travel_time;

    [[nodiscard]] friend bool operator==(const Corridor& lhs, const Corridor& rhs) noexcept {
        return lhs.id == rhs.id;
    }
};

/// True when the corridor admits only one robot at a time.
[[nodiscard]] bool is_single_lane(const Corridor& corridor) noexcept;

/// True when traffic may flow either way through it.
[[nodiscard]] bool is_bidirectional(const Corridor& corridor) noexcept;

/// True when a corridor can produce a head-on conflict: both directions are
/// legal, but not simultaneously.
///
/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8. These are the corridors that
/// need a held direction rather than a simple occupancy count.
[[nodiscard]] bool can_deadlock_head_on(const Corridor& corridor) noexcept;

/// The end opposite \p from. Empty when \p from is not an end of it.
[[nodiscard]] std::optional<core::NodeId> other_end(const Corridor& corridor,
                                                    const core::NodeId& from);

/// Builds a Corridor. Rejects empty ids, no edges, zero capacity, identical
/// ends, and a maximum travel time below the minimum.
[[nodiscard]] core::Result<Corridor, DomainError> make_corridor(core::ResourceId id,
                                                                core::NodeId entry_node,
                                                                core::NodeId exit_node,
                                                                std::vector<core::EdgeId> edges,
                                                                std::uint32_t capacity,
                                                                EdgeDirection direction);

/// One way through an intersection. docs/03_MAP_GRAPH.md §12.
///
/// "Enter on this edge, leave on that one" — the unit that conflict groups are
/// defined over.
struct Movement {
    core::MovementId id;
    core::ResourceId intersection_id;

    core::EdgeId from_edge;
    core::EdgeId to_edge;

    [[nodiscard]] friend bool operator==(const Movement& lhs, const Movement& rhs) noexcept {
        return lhs.id == rhs.id;
    }
};

/// A set of movements that cannot happen at the same time.
/// docs/03_MAP_GRAPH.md §11.
///
/// Membership is the whole meaning: two movements in one group exclude each
/// other. Movements in different groups may proceed together, which is what
/// lets an intersection carry more than one robot without a collision.
struct ConflictGroup {
    core::ConflictGroupId id;
    std::vector<core::MovementId> movements;

    [[nodiscard]] friend bool operator==(const ConflictGroup& lhs,
                                         const ConflictGroup& rhs) noexcept {
        return lhs.id == rhs.id;
    }
};

/// True when \p movement belongs to \p group.
[[nodiscard]] bool contains(const ConflictGroup& group, const core::MovementId& movement) noexcept;

/// Where several routes cross. docs/03_MAP_GRAPH.md §10.
struct Intersection {
    core::ResourceId id;

    /// The nodes that make it up.
    std::vector<core::NodeId> nodes;

    /// The edges that meet in it.
    std::vector<core::EdgeId> edges;

    /// The ways through it.
    std::vector<Movement> movements;

    /// Which of those ways exclude each other.
    std::vector<ConflictGroup> conflict_groups;

    /// How many robots may be inside at once.
    std::uint32_t capacity{1};

    [[nodiscard]] friend bool operator==(const Intersection& lhs,
                                         const Intersection& rhs) noexcept {
        return lhs.id == rhs.id;
    }
};

/// True when the two movements share a conflict group and so cannot run
/// together.
///
/// A movement never conflicts with itself: the same movement by the same robot
/// is one traversal, not two competing ones.
[[nodiscard]] bool movements_conflict(const Intersection& intersection,
                                      const core::MovementId& lhs,
                                      const core::MovementId& rhs) noexcept;

/// The movement entering on \p from_edge and leaving on \p to_edge.
/// Empty when the intersection defines no such way through.
[[nodiscard]] std::optional<Movement> find_movement(const Intersection& intersection,
                                                    const core::EdgeId& from_edge,
                                                    const core::EdgeId& to_edge);

/// Builds an Intersection. Rejects an empty id, no nodes, and zero capacity.
[[nodiscard]] core::Result<Intersection, DomainError> make_intersection(
    core::ResourceId id,
    std::vector<core::NodeId> nodes,
    std::vector<core::EdgeId> edges,
    std::uint32_t capacity);

/// Somewhere a robot can stand aside. docs/03_MAP_GRAPH.md §13.
///
/// The reason these are modelled separately: in a single-lane corridor with no
/// alternative route, a waiting bay is often the *only* way out of a head-on
/// deadlock. One robot backs into it and the other passes.
struct WaitingBay {
    core::ResourceId id;
    core::NodeId node_id;

    /// How many robots fit.
    std::uint32_t capacity{1};

    /// Robot types that may use it. Empty means any robot may.
    std::vector<std::string> compatible_robot_types;

    [[nodiscard]] friend bool operator==(const WaitingBay& lhs, const WaitingBay& rhs) noexcept {
        return lhs.id == rhs.id;
    }
};

/// True when a robot of \p robot_type may use \p bay.
[[nodiscard]] bool accepts_robot_type(const WaitingBay& bay, std::string_view robot_type);

/// Builds a WaitingBay. Rejects empty ids and zero capacity.
[[nodiscard]] core::Result<WaitingBay, DomainError> make_waiting_bay(core::ResourceId id,
                                                                     core::NodeId node_id,
                                                                     std::uint32_t capacity);

}  // namespace traffic::domain
