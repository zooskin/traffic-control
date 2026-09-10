#pragma once

/// \file
/// Graph primitives: Node and Edge.
///
/// docs/24_DOMAIN_MODEL.md §7~8, docs/03_MAP_GRAPH.md.
///
/// These are the graph's *elements*. The graph itself — adjacency, lookup,
/// validation, loading — is Phase 2, as is Corridor and Intersection. What is
/// here is only what an entity needs to identify a place and a way between two
/// places.
///
/// Each edge names the traffic resource it belongs to. docs/00_MASTER_PLAN.md
/// §4.2 is emphatic that a narrow corridor is not merely an edge: several edges
/// can share one resource, and it is the resource, not the edge, that has a
/// capacity of one.

#include <cstdint>
#include <optional>
#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/values.h"

namespace traffic::domain {

/// What a node is for. docs/03_MAP_GRAPH.md §5.
///
/// This is the nine-value set from the map specification, not the six of
/// docs/24_DOMAIN_MODEL.md §7 — see decision D-006 in docs/00_INDEX.md. The
/// short version: docs/01 and docs/06 use this vocabulary too, so §7 was the
/// outlier.
enum class NodeType {
    /// Ordinary point on the graph.
    normal,

    /// Where routes cross. A primary target for conflict detection and
    /// reservation — docs/03_MAP_GRAPH.md §10.
    intersection,

    /// A fixed work position that is neither pickup nor dropoff.
    station,

    /// Where a robot collects a load.
    pickup,

    /// Where a robot delivers one.
    dropoff,

    /// Battery charging position.
    charger,

    /// Somewhere a robot can stand aside so others can pass.
    ///
    /// Central to deadlock recovery: docs/03_MAP_GRAPH.md §13 introduces
    /// waiting bays specifically to break corridor deadlocks, and
    /// docs/22_IMPLEMENTATION_WORKFLOW.md Phase 10 lists moving to one as a
    /// recovery strategy.
    waiting_bay,

    /// Where robots enter the controlled area.
    entry,

    /// Where they leave it.
    exit,
};

inline constexpr std::size_t kNodeTypeCount = 9;

[[nodiscard]] std::string_view to_string(NodeType type) noexcept;
[[nodiscard]] std::optional<NodeType> node_type_from_string(std::string_view name) noexcept;

/// A point a robot can occupy or pass through. docs/03_MAP_GRAPH.md §4.
struct Node {
    core::NodeId id;
    Position position;
    NodeType type{NodeType::normal};

    /// Heading a robot should hold at this node, in radians. Matters at a
    /// station or charger, where the robot has to dock facing a particular
    /// way; empty everywhere else.
    std::optional<double> orientation;

    /// How many robots may occupy it at once. Almost always 1.
    std::uint32_t capacity{1};

    /// The traffic resource this node belongs to.
    ///
    /// Empty when the node is not part of a larger resource. An intersection's
    /// nodes name the intersection resource, so that reserving the crossing
    /// covers every node in it rather than each node separately.
    core::ResourceId resource_id;

    [[nodiscard]] friend bool operator==(const Node& lhs, const Node& rhs) noexcept {
        // docs/24_DOMAIN_MODEL.md §33: entities compare by identity.
        return lhs.id == rhs.id;
    }
};

/// Which way robots may travel along an edge. docs/24_DOMAIN_MODEL.md §8.
enum class EdgeDirection {
    /// from_node -> to_node only.
    forward,

    /// to_node -> from_node only.
    reverse,

    /// Both, but not at once when the underlying resource has capacity 1.
    /// This is where head-on conflicts come from
    /// (docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8).
    bidirectional,
};

[[nodiscard]] std::string_view to_string(EdgeDirection direction) noexcept;
[[nodiscard]] std::optional<EdgeDirection> edge_direction_from_string(
    std::string_view name) noexcept;

/// A traversable connection between two nodes.
struct Edge {
    core::EdgeId id;
    core::NodeId from_node;
    core::NodeId to_node;

    /// Metres. Used as the base cost for planning.
    double length{0.0};

    /// Metres. Narrow edges are what force single-file traffic.
    double width{0.0};

    /// Metres per second.
    double speed_limit{0.0};

    EdgeDirection direction{EdgeDirection::bidirectional};

    /// How many robots may be on it at once.
    std::uint32_t capacity{1};

    /// The traffic resource this edge belongs to.
    ///
    /// Empty means the edge is its own resource. Several edges through one
    /// narrow corridor share a single resource id, which is what makes the
    /// corridor — not each edge — the thing that admits one robot at a time.
    core::ResourceId resource_id;

    /// Traversal time, when it is not simply length / speed_limit.
    ///
    /// Empty for an ordinary edge, where `nominal_travel_time` derives it.
    /// Set it for a lift or a powered door, where the time a robot spends is
    /// unrelated to the distance covered. Decision D-007 in docs/00_INDEX.md
    /// explains why the derived value is the default rather than a stored one.
    std::optional<core::Duration> travel_time_override;

    /// Whether traffic may use it at all. A blocked corridor is disabled
    /// rather than deleted, so the map version stays stable.
    bool enabled{true};

    [[nodiscard]] friend bool operator==(const Edge& lhs, const Edge& rhs) noexcept {
        return lhs.id == rhs.id;
    }
};

/// True when \p edge may be traversed from \p from towards its other end.
[[nodiscard]] bool is_traversable_from(const Edge& edge, const core::NodeId& from) noexcept;

/// The node at the far end of \p edge when entering from \p from.
/// Empty when \p from is not an endpoint of the edge.
[[nodiscard]] std::optional<core::NodeId> opposite_node(const Edge& edge,
                                                        const core::NodeId& from) noexcept;

/// The resource an edge belongs to: its own id when no corridor claims it.
[[nodiscard]] core::ResourceId effective_resource(const Edge& edge);

/// How long traversing \p edge is expected to take.
///
/// The override when one is set, otherwise length / speed_limit. Route
/// segments' expected entry and exit times are built from this, and those are
/// what make temporal conflict detection possible at all.
[[nodiscard]] core::Duration nominal_travel_time(const Edge& edge) noexcept;

/// Builds a Node, rejecting an empty id.
[[nodiscard]] core::Result<Node, DomainError> make_node(core::NodeId id,
                                                        Position position,
                                                        NodeType type,
                                                        std::uint32_t capacity);

/// Builds an Edge, checking the invariants of docs/24_DOMAIN_MODEL.md §34.
///
/// Rejects: empty ids, a self-loop, non-positive length or speed limit, zero
/// capacity, negative width.
[[nodiscard]] core::Result<Edge, DomainError> make_edge(core::EdgeId id,
                                                        core::NodeId from_node,
                                                        core::NodeId to_node,
                                                        double length,
                                                        double speed_limit,
                                                        EdgeDirection direction);

}  // namespace traffic::domain
