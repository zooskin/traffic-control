#pragma once

/// \file
/// The traffic map. docs/03_MAP_GRAPH.md §16~21.
///
/// A Map is an immutable snapshot of the graph plus the resources laid over it,
/// stamped with a version. Nothing mutates a Map after construction; a changed
/// site produces a new Map with a higher version, and every route records the
/// version it was planned against so a stale one can be caught before it is
/// committed (docs/23_SYSTEM_ARCHITECTURE.md §16).
///
/// The queries here are the ones docs/03_MAP_GRAPH.md §18~20 names: neighbours,
/// reachability, and resource lookup. They are read-only and const, so the map
/// can be shared across the planner workers of
/// docs/12_SOFTWARE_ARCHITECTURE.md §22 without a lock.
///
/// Lookups are indexed at construction rather than scanned. At 200 robots the
/// planner asks for neighbours tens of thousands of times a second, and a
/// linear scan over the edge list would dominate the decision loop.

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/graph.h"
#include "traffic/domain/resources.h"
#include "traffic/domain/values.h"

namespace traffic::map {

/// Everything a Map is built from. docs/03_MAP_GRAPH.md §16.
///
/// Passed to `make_map`, which validates it and takes ownership. Kept separate
/// from Map so that a loader can assemble the parts without ever holding a
/// half-built Map.
struct MapData {
    std::string map_id;
    domain::MapVersion version;

    std::vector<domain::Node> nodes;
    std::vector<domain::Edge> edges;

    std::vector<domain::Corridor> corridors;
    std::vector<domain::Intersection> intersections;
    std::vector<domain::WaitingBay> waiting_bays;

    core::TimePoint created_at{core::kTimeOrigin};
    core::TimePoint updated_at{core::kTimeOrigin};
};

/// An indexed, read-only traffic map.
class Map {
public:
    /// Builds the indices. Use `make_map`, which validates first.
    explicit Map(MapData data);

    [[nodiscard]] const std::string& id() const noexcept { return data_.map_id; }
    [[nodiscard]] domain::MapVersion version() const noexcept { return data_.version; }

    [[nodiscard]] std::span<const domain::Node> nodes() const noexcept { return data_.nodes; }
    [[nodiscard]] std::span<const domain::Edge> edges() const noexcept { return data_.edges; }
    [[nodiscard]] std::span<const domain::Corridor> corridors() const noexcept {
        return data_.corridors;
    }
    [[nodiscard]] std::span<const domain::Intersection> intersections() const noexcept {
        return data_.intersections;
    }
    [[nodiscard]] std::span<const domain::WaitingBay> waiting_bays() const noexcept {
        return data_.waiting_bays;
    }

    [[nodiscard]] std::size_t node_count() const noexcept { return data_.nodes.size(); }
    [[nodiscard]] std::size_t edge_count() const noexcept { return data_.edges.size(); }

    // ------------------------------------------------------------- lookups

    /// The node with this id, or nullptr.
    [[nodiscard]] const domain::Node* find_node(const core::NodeId& id) const;

    /// The edge with this id, or nullptr.
    [[nodiscard]] const domain::Edge* find_edge(const core::EdgeId& id) const;

    [[nodiscard]] bool has_node(const core::NodeId& id) const;
    [[nodiscard]] bool has_edge(const core::EdgeId& id) const;

    // ---------------------------------------------------------- traversal

    /// Edges incident to \p node, whichever way they run.
    /// docs/03_MAP_GRAPH.md §19.
    [[nodiscard]] std::span<const core::EdgeId> incident_edges(const core::NodeId& node) const;

    /// Edges a robot standing at \p node may actually take: incident, enabled,
    /// and traversable in that direction.
    ///
    /// Distinct from `incident_edges` on purpose. A planner that expands
    /// incident edges will happily route the wrong way down a one-way corridor
    /// and only discover it when the reservation is refused.
    [[nodiscard]] std::vector<core::EdgeId> traversable_edges(const core::NodeId& node) const;

    /// Nodes reachable from \p node in one legal move.
    [[nodiscard]] std::vector<core::NodeId> neighbours(const core::NodeId& node) const;

    /// True when some sequence of legal moves leads from \p start to \p goal.
    /// docs/03_MAP_GRAPH.md §18.
    ///
    /// Respects direction and the enabled flag, so a one-way graph is not
    /// reported as symmetric.
    [[nodiscard]] bool is_reachable(const core::NodeId& start, const core::NodeId& goal) const;

    /// Every node reachable from \p start, including it.
    [[nodiscard]] std::vector<core::NodeId> reachable_from(const core::NodeId& start) const;

    // ---------------------------------------------------------- resources

    /// The corridor with this resource id, or nullptr.
    [[nodiscard]] const domain::Corridor* find_corridor(const core::ResourceId& id) const;

    /// The intersection with this resource id, or nullptr.
    [[nodiscard]] const domain::Intersection* find_intersection(const core::ResourceId& id) const;

    /// The waiting bay with this resource id, or nullptr.
    [[nodiscard]] const domain::WaitingBay* find_waiting_bay(const core::ResourceId& id) const;

    /// The resource an edge belongs to. docs/03_MAP_GRAPH.md §20.
    ///
    /// The corridor's id when one claims the edge, otherwise the edge's own id
    /// — the edge is then its own resource.
    [[nodiscard]] core::ResourceId resource_for_edge(const core::EdgeId& edge) const;

    /// The resource a node belongs to, when one does.
    [[nodiscard]] std::optional<core::ResourceId> resource_for_node(const core::NodeId& node) const;

    /// Every edge that belongs to \p resource.
    [[nodiscard]] std::vector<core::EdgeId> edges_for_resource(
        const core::ResourceId& resource) const;

private:
    void build_indices();

    MapData data_;

    std::unordered_map<core::NodeId, std::size_t> node_index_;
    std::unordered_map<core::EdgeId, std::size_t> edge_index_;
    std::unordered_map<core::NodeId, std::vector<core::EdgeId>> incident_;

    std::unordered_map<core::ResourceId, std::size_t> corridor_index_;
    std::unordered_map<core::ResourceId, std::size_t> intersection_index_;
    std::unordered_map<core::ResourceId, std::size_t> waiting_bay_index_;
    std::unordered_map<core::ResourceId, std::vector<core::EdgeId>> resource_edges_;
};

}  // namespace traffic::map
