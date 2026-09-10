#include "traffic/domain/graph.h"

#include <array>
#include <chrono>
#include <utility>

namespace traffic::domain {
namespace {

constexpr std::array<std::pair<NodeType, std::string_view>, kNodeTypeCount> kNodeTypeNames{{
    {NodeType::normal, "NORMAL"},
    {NodeType::intersection, "INTERSECTION"},
    {NodeType::station, "STATION"},
    {NodeType::pickup, "PICKUP"},
    {NodeType::dropoff, "DROPOFF"},
    {NodeType::charger, "CHARGER"},
    {NodeType::waiting_bay, "WAITING_BAY"},
    {NodeType::entry, "ENTRY"},
    {NodeType::exit, "EXIT"},
}};

constexpr std::array<std::pair<EdgeDirection, std::string_view>, 3> kEdgeDirectionNames{{
    {EdgeDirection::forward, "FORWARD"},
    {EdgeDirection::reverse, "REVERSE"},
    {EdgeDirection::bidirectional, "BIDIRECTIONAL"},
}};

using NodeResult = core::Result<Node, DomainError>;
using EdgeResult = core::Result<Edge, DomainError>;

}  // namespace

std::string_view to_string(NodeType type) noexcept {
    for (const auto& [value, name] : kNodeTypeNames) {
        if (value == type) {
            return name;
        }
    }
    return "NORMAL";
}

std::optional<NodeType> node_type_from_string(std::string_view name) noexcept {
    for (const auto& [value, candidate] : kNodeTypeNames) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}

std::string_view to_string(EdgeDirection direction) noexcept {
    for (const auto& [value, name] : kEdgeDirectionNames) {
        if (value == direction) {
            return name;
        }
    }
    return "BIDIRECTIONAL";
}

std::optional<EdgeDirection> edge_direction_from_string(std::string_view name) noexcept {
    for (const auto& [value, candidate] : kEdgeDirectionNames) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}

bool is_traversable_from(const Edge& edge, const core::NodeId& from) noexcept {
    if (!edge.enabled) {
        return false;
    }
    switch (edge.direction) {
        case EdgeDirection::forward:
            return from == edge.from_node;
        case EdgeDirection::reverse:
            return from == edge.to_node;
        case EdgeDirection::bidirectional:
            return from == edge.from_node || from == edge.to_node;
    }
    return false;
}

std::optional<core::NodeId> opposite_node(const Edge& edge, const core::NodeId& from) noexcept {
    if (from == edge.from_node) {
        return edge.to_node;
    }
    if (from == edge.to_node) {
        return edge.from_node;
    }
    return std::nullopt;
}

core::Duration nominal_travel_time(const Edge& edge) noexcept {
    if (edge.travel_time_override.has_value()) {
        return *edge.travel_time_override;
    }
    if (edge.speed_limit <= 0.0) {
        // make_edge refuses this, but an Edge built by hand could carry it.
        return core::Duration::zero();
    }
    const double seconds = edge.length / edge.speed_limit;
    return std::chrono::duration_cast<core::Duration>(std::chrono::duration<double>{seconds});
}

core::ResourceId effective_resource(const Edge& edge) {
    if (!edge.resource_id.empty()) {
        return edge.resource_id;
    }
    return core::ResourceId{edge.id.value()};
}

core::Result<Node, DomainError> make_node(core::NodeId id,
                                          Position position,
                                          NodeType type,
                                          std::uint32_t capacity) {
    if (id.empty()) {
        return NodeResult::failure(DomainError::empty_id);
    }
    if (capacity == 0) {
        return NodeResult::failure(DomainError::non_positive_value);
    }

    Node node;
    node.id = std::move(id);
    node.position = position;
    node.type = type;
    node.capacity = capacity;
    return NodeResult::success(std::move(node));
}

core::Result<Edge, DomainError> make_edge(core::EdgeId id,
                                          core::NodeId from_node,
                                          core::NodeId to_node,
                                          double length,
                                          double speed_limit,
                                          EdgeDirection direction) {
    if (id.empty() || from_node.empty() || to_node.empty()) {
        return EdgeResult::failure(DomainError::empty_id);
    }
    if (from_node == to_node) {
        // A self-loop would let a planner "move" without going anywhere, which
        // shows up much later as a robot that never reaches its goal.
        return EdgeResult::failure(DomainError::same_source_and_destination);
    }
    if (length <= 0.0) {
        return EdgeResult::failure(DomainError::non_positive_value);
    }
    if (speed_limit <= 0.0) {
        // A zero speed limit makes traversal time infinite; disable the edge
        // instead of describing it as untraversable-but-present.
        return EdgeResult::failure(DomainError::non_positive_value);
    }

    Edge edge;
    edge.id = std::move(id);
    edge.from_node = std::move(from_node);
    edge.to_node = std::move(to_node);
    edge.length = length;
    edge.speed_limit = speed_limit;
    edge.direction = direction;
    return EdgeResult::success(std::move(edge));
}

}  // namespace traffic::domain
