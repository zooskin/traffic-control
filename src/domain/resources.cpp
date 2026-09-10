#include "traffic/domain/resources.h"

#include <algorithm>
#include <utility>

namespace traffic::domain {
namespace {

using CorridorResult = core::Result<Corridor, DomainError>;
using IntersectionResult = core::Result<Intersection, DomainError>;
using WaitingBayResult = core::Result<WaitingBay, DomainError>;

}  // namespace

// ------------------------------------------------------------------ Corridor

bool is_single_lane(const Corridor& corridor) noexcept {
    return corridor.capacity == 1;
}

bool is_bidirectional(const Corridor& corridor) noexcept {
    return corridor.direction == EdgeDirection::bidirectional;
}

bool can_deadlock_head_on(const Corridor& corridor) noexcept {
    // Both directions legal, one robot at a time. A one-way corridor cannot
    // produce a head-on; neither can a two-lane one.
    return is_bidirectional(corridor) && is_single_lane(corridor);
}

std::optional<core::NodeId> other_end(const Corridor& corridor, const core::NodeId& from) {
    if (from == corridor.entry_node) {
        return corridor.exit_node;
    }
    if (from == corridor.exit_node) {
        return corridor.entry_node;
    }
    return std::nullopt;
}

core::Result<Corridor, DomainError> make_corridor(core::ResourceId id,
                                                  core::NodeId entry_node,
                                                  core::NodeId exit_node,
                                                  std::vector<core::EdgeId> edges,
                                                  std::uint32_t capacity,
                                                  EdgeDirection direction) {
    if (id.empty() || entry_node.empty() || exit_node.empty()) {
        return CorridorResult::failure(DomainError::empty_id);
    }
    if (entry_node == exit_node) {
        // A corridor that starts and ends at the same node encloses no
        // passage, so nothing can be granted or released for it.
        return CorridorResult::failure(DomainError::same_source_and_destination);
    }
    if (edges.empty()) {
        return CorridorResult::failure(DomainError::empty_route);
    }
    if (capacity == 0) {
        // Capacity zero would refuse every robot forever. Disable the edges
        // instead, which is recoverable.
        return CorridorResult::failure(DomainError::non_positive_value);
    }
    if (std::any_of(edges.begin(), edges.end(), [](const core::EdgeId& e) { return e.empty(); })) {
        return CorridorResult::failure(DomainError::empty_id);
    }

    Corridor corridor;
    corridor.id = std::move(id);
    corridor.entry_node = std::move(entry_node);
    corridor.exit_node = std::move(exit_node);
    corridor.edges = std::move(edges);
    corridor.capacity = capacity;
    corridor.direction = direction;
    return CorridorResult::success(std::move(corridor));
}

// ------------------------------------------------------------- ConflictGroup

bool contains(const ConflictGroup& group, const core::MovementId& movement) noexcept {
    return std::find(group.movements.begin(), group.movements.end(), movement) !=
           group.movements.end();
}

// -------------------------------------------------------------- Intersection

bool movements_conflict(const Intersection& intersection,
                        const core::MovementId& lhs,
                        const core::MovementId& rhs) noexcept {
    if (lhs == rhs) {
        // The same movement is one traversal, not two competing ones.
        return false;
    }
    return std::any_of(intersection.conflict_groups.begin(),
                       intersection.conflict_groups.end(),
                       [&lhs, &rhs](const ConflictGroup& group) {
                           return contains(group, lhs) && contains(group, rhs);
                       });
}

std::optional<Movement> find_movement(const Intersection& intersection,
                                      const core::EdgeId& from_edge,
                                      const core::EdgeId& to_edge) {
    const auto it =
        std::find_if(intersection.movements.begin(),
                     intersection.movements.end(),
                     [&from_edge, &to_edge](const Movement& movement) {
                         return movement.from_edge == from_edge && movement.to_edge == to_edge;
                     });
    if (it == intersection.movements.end()) {
        return std::nullopt;
    }
    return *it;
}

core::Result<Intersection, DomainError> make_intersection(core::ResourceId id,
                                                          std::vector<core::NodeId> nodes,
                                                          std::vector<core::EdgeId> edges,
                                                          std::uint32_t capacity) {
    if (id.empty()) {
        return IntersectionResult::failure(DomainError::empty_id);
    }
    if (nodes.empty()) {
        return IntersectionResult::failure(DomainError::empty_route);
    }
    if (capacity == 0) {
        return IntersectionResult::failure(DomainError::non_positive_value);
    }
    if (std::any_of(nodes.begin(), nodes.end(), [](const core::NodeId& n) { return n.empty(); })) {
        return IntersectionResult::failure(DomainError::empty_id);
    }

    Intersection intersection;
    intersection.id = std::move(id);
    intersection.nodes = std::move(nodes);
    intersection.edges = std::move(edges);
    intersection.capacity = capacity;
    return IntersectionResult::success(std::move(intersection));
}

// ---------------------------------------------------------------- WaitingBay

bool accepts_robot_type(const WaitingBay& bay, std::string_view robot_type) {
    if (bay.compatible_robot_types.empty()) {
        return true;
    }
    return std::find(bay.compatible_robot_types.begin(),
                     bay.compatible_robot_types.end(),
                     robot_type) != bay.compatible_robot_types.end();
}

core::Result<WaitingBay, DomainError> make_waiting_bay(core::ResourceId id,
                                                       core::NodeId node_id,
                                                       std::uint32_t capacity) {
    if (id.empty() || node_id.empty()) {
        return WaitingBayResult::failure(DomainError::empty_id);
    }
    if (capacity == 0) {
        return WaitingBayResult::failure(DomainError::non_positive_value);
    }

    WaitingBay bay;
    bay.id = std::move(id);
    bay.node_id = std::move(node_id);
    bay.capacity = capacity;
    return WaitingBayResult::success(std::move(bay));
}

}  // namespace traffic::domain
