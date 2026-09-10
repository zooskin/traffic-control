#include "traffic/map/map.h"

#include <algorithm>
#include <deque>
#include <unordered_set>
#include <utility>

namespace traffic::map {
namespace {

/// Returned for a node the map does not have, so callers can iterate the
/// result without a null check.
const std::vector<core::EdgeId>& empty_edge_list() {
    static const std::vector<core::EdgeId> kEmpty;
    return kEmpty;
}

}  // namespace

Map::Map(MapData data) : data_(std::move(data)) {
    build_indices();
}

void Map::build_indices() {
    node_index_.reserve(data_.nodes.size());
    for (std::size_t i = 0; i < data_.nodes.size(); ++i) {
        node_index_.emplace(data_.nodes[i].id, i);
    }

    edge_index_.reserve(data_.edges.size());
    // Adjacency and resource membership are built by walking the edge list in
    // order, so both lists come out in a stable order regardless of how the
    // hash maps happen to be arranged. docs/20_CODING_GUIDELINES.md §23
    // requires the same input to produce the same decision, and a planner that
    // expands neighbours in a hash-dependent order does not.
    for (std::size_t i = 0; i < data_.edges.size(); ++i) {
        const domain::Edge& edge = data_.edges[i];
        edge_index_.emplace(edge.id, i);

        incident_[edge.from_node].push_back(edge.id);
        if (edge.to_node != edge.from_node) {
            incident_[edge.to_node].push_back(edge.id);
        }

        resource_edges_[domain::effective_resource(edge)].push_back(edge.id);
    }

    for (std::size_t i = 0; i < data_.corridors.size(); ++i) {
        corridor_index_.emplace(data_.corridors[i].id, i);
    }
    for (std::size_t i = 0; i < data_.intersections.size(); ++i) {
        intersection_index_.emplace(data_.intersections[i].id, i);
    }
    for (std::size_t i = 0; i < data_.waiting_bays.size(); ++i) {
        waiting_bay_index_.emplace(data_.waiting_bays[i].id, i);
    }
}

// ------------------------------------------------------------------ lookups

const domain::Node* Map::find_node(const core::NodeId& id) const {
    const auto it = node_index_.find(id);
    return it == node_index_.end() ? nullptr : &data_.nodes[it->second];
}

const domain::Edge* Map::find_edge(const core::EdgeId& id) const {
    const auto it = edge_index_.find(id);
    return it == edge_index_.end() ? nullptr : &data_.edges[it->second];
}

bool Map::has_node(const core::NodeId& id) const {
    return node_index_.contains(id);
}

bool Map::has_edge(const core::EdgeId& id) const {
    return edge_index_.contains(id);
}

// --------------------------------------------------------------- traversal

std::span<const core::EdgeId> Map::incident_edges(const core::NodeId& node) const {
    const auto it = incident_.find(node);
    return it == incident_.end() ? std::span<const core::EdgeId>{empty_edge_list()}
                                 : std::span<const core::EdgeId>{it->second};
}

std::vector<core::EdgeId> Map::traversable_edges(const core::NodeId& node) const {
    std::vector<core::EdgeId> usable;
    for (const core::EdgeId& edge_id : incident_edges(node)) {
        const domain::Edge* edge = find_edge(edge_id);
        if (edge != nullptr && domain::is_traversable_from(*edge, node)) {
            usable.push_back(edge_id);
        }
    }
    return usable;
}

std::vector<core::NodeId> Map::neighbours(const core::NodeId& node) const {
    std::vector<core::NodeId> result;
    for (const core::EdgeId& edge_id : traversable_edges(node)) {
        const domain::Edge* edge = find_edge(edge_id);
        if (edge == nullptr) {
            continue;
        }
        if (const auto other = domain::opposite_node(*edge, node); other.has_value()) {
            result.push_back(*other);
        }
    }
    return result;
}

std::vector<core::NodeId> Map::reachable_from(const core::NodeId& start) const {
    std::vector<core::NodeId> visited_order;
    if (!has_node(start)) {
        return visited_order;
    }

    std::unordered_set<core::NodeId> seen;
    std::deque<core::NodeId> queue;

    seen.insert(start);
    queue.push_back(start);
    visited_order.push_back(start);

    while (!queue.empty()) {
        const core::NodeId current = queue.front();
        queue.pop_front();

        // neighbours() walks the edge list in map order, so the traversal
        // order is reproducible.
        for (const core::NodeId& next : neighbours(current)) {
            if (seen.insert(next).second) {
                queue.push_back(next);
                visited_order.push_back(next);
            }
        }
    }

    return visited_order;
}

bool Map::is_reachable(const core::NodeId& start, const core::NodeId& goal) const {
    if (!has_node(start) || !has_node(goal)) {
        return false;
    }
    if (start == goal) {
        return true;
    }

    std::unordered_set<core::NodeId> seen;
    std::deque<core::NodeId> queue;
    seen.insert(start);
    queue.push_back(start);

    while (!queue.empty()) {
        const core::NodeId current = queue.front();
        queue.pop_front();

        for (const core::NodeId& next : neighbours(current)) {
            if (next == goal) {
                return true;
            }
            if (seen.insert(next).second) {
                queue.push_back(next);
            }
        }
    }

    return false;
}

// ---------------------------------------------------------------- resources

const domain::Corridor* Map::find_corridor(const core::ResourceId& id) const {
    const auto it = corridor_index_.find(id);
    return it == corridor_index_.end() ? nullptr : &data_.corridors[it->second];
}

const domain::Intersection* Map::find_intersection(const core::ResourceId& id) const {
    const auto it = intersection_index_.find(id);
    return it == intersection_index_.end() ? nullptr : &data_.intersections[it->second];
}

const domain::WaitingBay* Map::find_waiting_bay(const core::ResourceId& id) const {
    const auto it = waiting_bay_index_.find(id);
    return it == waiting_bay_index_.end() ? nullptr : &data_.waiting_bays[it->second];
}

core::ResourceId Map::resource_for_edge(const core::EdgeId& edge_id) const {
    const domain::Edge* edge = find_edge(edge_id);
    if (edge == nullptr) {
        return core::ResourceId{};
    }
    return domain::effective_resource(*edge);
}

std::optional<core::ResourceId> Map::resource_for_node(const core::NodeId& node_id) const {
    const domain::Node* node = find_node(node_id);
    if (node == nullptr || node->resource_id.empty()) {
        return std::nullopt;
    }
    return node->resource_id;
}

std::vector<core::EdgeId> Map::edges_for_resource(const core::ResourceId& resource) const {
    const auto it = resource_edges_.find(resource);
    return it == resource_edges_.end() ? std::vector<core::EdgeId>{} : it->second;
}

}  // namespace traffic::map
