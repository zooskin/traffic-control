#include "traffic/reservation/conflict_rules.h"

#include <cstddef>
#include <set>
#include <string>
#include <vector>

namespace traffic::reservation {
namespace {

using ThresholdResult = core::Result<SeverityThresholds, domain::DomainError>;

/// Robots holding \p resource at \p instant, counted once each.
///
/// A set rather than a counter: one robot can hold two adjacent windows of the
/// same corridor while it moves from one of its edges to the next, and counting
/// that as two occupants would report a full corridor with one robot in it.
[[nodiscard]] std::size_t distinct_robots_at(std::span<const ResourceOccupancy> holders,
                                             const core::ResourceId& resource,
                                             core::TimePoint instant) {
    std::set<core::RobotId> present;
    for (const ResourceOccupancy& holder : holders) {
        if (holder.resource_id == resource && holder.window.contains(instant)) {
            present.insert(holder.robot_id);
        }
    }
    return present.size();
}

}  // namespace

std::string_view to_string(PassageDirection direction) noexcept {
    switch (direction) {
        case PassageDirection::entry_to_exit:
            return "ENTRY_TO_EXIT";
        case PassageDirection::exit_to_entry:
            return "EXIT_TO_ENTRY";
    }
    return "ENTRY_TO_EXIT";
}

// ----------------------------------------------------------------- projection

ResourceOccupancy as_occupancy(const NodeVisit& visit) {
    ResourceOccupancy occupancy;
    occupancy.robot_id = visit.robot_id;
    // A node is a resource in its own right — docs/24_DOMAIN_MODEL.md §14 lists
    // NODE among the resource types — and it keeps its own id, so two different
    // nodes of one intersection stay distinguishable here.
    occupancy.resource_id = core::ResourceId{visit.node_id.value()};
    occupancy.window = visit.window;
    return occupancy;
}

ResourceOccupancy as_occupancy(const EdgeTraversal& traversal) {
    ResourceOccupancy occupancy;
    occupancy.robot_id = traversal.robot_id;
    occupancy.resource_id = traversal.resource_id;
    occupancy.window = traversal.window;
    return occupancy;
}

ResourceOccupancy as_occupancy(const CorridorPassage& passage) {
    ResourceOccupancy occupancy;
    occupancy.robot_id = passage.robot_id;
    occupancy.resource_id = passage.corridor_id;
    occupancy.window = passage.window;
    return occupancy;
}

ResourceOccupancy as_occupancy(const IntersectionCrossing& crossing) {
    ResourceOccupancy occupancy;
    occupancy.robot_id = crossing.robot_id;
    occupancy.resource_id = crossing.intersection_id;
    occupancy.window = crossing.window;
    return occupancy;
}

ResourceOccupancy as_occupancy(const domain::Reservation& reservation) {
    ResourceOccupancy occupancy;
    occupancy.robot_id = reservation.robot_id;
    occupancy.resource_id = reservation.resource_id;
    occupancy.window = reservation.window;
    return occupancy;
}

// ---------------------------------------------------------------------- rules

std::optional<domain::TimeWindow> overlap_of(const domain::TimeWindow& lhs,
                                             const domain::TimeWindow& rhs) noexcept {
    if (!lhs.overlaps(rhs)) {
        return std::nullopt;
    }
    const core::TimePoint start = lhs.start() > rhs.start() ? lhs.start() : rhs.start();
    const core::TimePoint end = lhs.end() < rhs.end() ? lhs.end() : rhs.end();
    return domain::TimeWindow{start, end};
}

bool is_temporal_conflict(const ResourceOccupancy& lhs, const ResourceOccupancy& rhs) noexcept {
    if (lhs.robot_id == rhs.robot_id) {
        // A robot does not block itself. Treating it as if it did produces a
        // deadlock with one participant, which nothing can resolve.
        return false;
    }
    if (lhs.resource_id.empty() || lhs.resource_id != rhs.resource_id) {
        // Two unnamed resources are not the same resource. Without this an
        // occupancy that failed to resolve its resource would conflict with
        // every other one that failed the same way.
        return false;
    }
    return lhs.window.overlaps(rhs.window);
}

bool is_capacity_conflict(const ResourceOccupancy& lhs,
                          const ResourceOccupancy& rhs,
                          std::span<const ResourceOccupancy> holders,
                          std::uint32_t capacity) {
    if (!is_temporal_conflict(lhs, rhs)) {
        return false;
    }
    if (capacity <= 1) {
        // The single-lane case, and the common one: two overlapping holders
        // already exceed a capacity of one, so nothing else needs counting.
        return true;
    }

    const std::optional<domain::TimeWindow> shared = overlap_of(lhs.window, rhs.window);
    if (!shared.has_value()) {
        return false;
    }
    const domain::TimeWindow window = shared.value();

    // The number of intervals covering a point can only rise where one of them
    // begins, so testing the start of the shared window plus every holder start
    // inside it finds the maximum exactly.
    std::vector<core::TimePoint> instants;
    instants.push_back(window.start());
    for (const ResourceOccupancy& holder : holders) {
        if (holder.resource_id == lhs.resource_id && window.contains(holder.window.start())) {
            instants.push_back(holder.window.start());
        }
    }

    for (const core::TimePoint instant : instants) {
        if (distinct_robots_at(holders, lhs.resource_id, instant) > capacity) {
            return true;
        }
    }
    return false;
}

bool is_node_conflict(const NodeVisit& lhs, const NodeVisit& rhs) noexcept {
    if (lhs.robot_id == rhs.robot_id) {
        return false;
    }
    if (lhs.node_id.empty() || lhs.node_id != rhs.node_id) {
        return false;
    }
    return lhs.window.overlaps(rhs.window);
}

bool is_following_conflict(const EdgeTraversal& lhs, const EdgeTraversal& rhs) noexcept {
    if (lhs.robot_id == rhs.robot_id) {
        return false;
    }
    if (lhs.edge_id.empty() || lhs.edge_id != rhs.edge_id) {
        return false;
    }
    return lhs.from_node == rhs.from_node && lhs.window.overlaps(rhs.window);
}

bool is_head_on_conflict(const EdgeTraversal& lhs, const EdgeTraversal& rhs) noexcept {
    if (lhs.robot_id == rhs.robot_id) {
        return false;
    }
    if (lhs.edge_id.empty() || lhs.edge_id != rhs.edge_id) {
        return false;
    }
    return lhs.from_node != rhs.from_node && lhs.window.overlaps(rhs.window);
}

std::optional<domain::ConflictType> classify_edge_conflict(const EdgeTraversal& lhs,
                                                           const EdgeTraversal& rhs) noexcept {
    if (is_head_on_conflict(lhs, rhs)) {
        return domain::ConflictType::head_on;
    }
    if (is_following_conflict(lhs, rhs)) {
        return domain::ConflictType::edge;
    }
    return std::nullopt;
}

bool is_head_on_conflict(const domain::Corridor& corridor,
                         const CorridorPassage& lhs,
                         const CorridorPassage& rhs) noexcept {
    if (lhs.robot_id == rhs.robot_id) {
        return false;
    }
    if (lhs.corridor_id != corridor.id || rhs.corridor_id != corridor.id) {
        return false;
    }
    if (!domain::can_deadlock_head_on(corridor)) {
        return false;
    }
    return lhs.direction != rhs.direction && lhs.window.overlaps(rhs.window);
}

bool is_crossing_conflict(const domain::Intersection& intersection,
                          const IntersectionCrossing& lhs,
                          const IntersectionCrossing& rhs) noexcept {
    if (lhs.robot_id == rhs.robot_id) {
        return false;
    }
    if (lhs.intersection_id != intersection.id || rhs.intersection_id != intersection.id) {
        return false;
    }
    if (!lhs.window.overlaps(rhs.window)) {
        return false;
    }

    const std::optional<core::MovementId>& left = lhs.movement_id;
    const std::optional<core::MovementId>& right = rhs.movement_id;
    if (!left.has_value() || !right.has_value()) {
        return true;
    }
    return domain::movements_conflict(intersection, left.value(), right.value());
}

// ------------------------------------------------------------ classification

int conflict_type_precedence(domain::ConflictType type) noexcept {
    switch (type) {
        case domain::ConflictType::head_on:
            // Says the most: not merely that the resource is taken, but that no
            // ordering of the two robots inside it exists.
            return 6;
        case domain::ConflictType::crossing:
            return 5;
        case domain::ConflictType::corridor:
            // Above `edge` because the corridor is the thing that admits one
            // robot; the shared edge is a symptom of being in it together.
            return 4;
        case domain::ConflictType::edge:
            return 3;
        case domain::ConflictType::node:
            return 2;
        case domain::ConflictType::resource:
            return 1;
        case domain::ConflictType::temporal:
            // Says the least: the windows meet. Every other value also implies
            // this one.
            return 0;
    }
    return 0;
}

// ---------------------------------------------------------------- severity

core::Result<SeverityThresholds, domain::DomainError> make_severity_thresholds(
    SeverityThresholds thresholds) {
    if (thresholds.absorbed_lead < core::Duration::zero() ||
        thresholds.critical_lead < core::Duration::zero()) {
        return ThresholdResult::failure(domain::DomainError::negative_value);
    }
    if (thresholds.critical_lead >= thresholds.absorbed_lead) {
        return ThresholdResult::failure(domain::DomainError::invalid_time_window);
    }
    return ThresholdResult::success(thresholds);
}

domain::ConflictSeverity severity_for(domain::ConflictType type,
                                      const domain::TimeWindow& overlap,
                                      core::TimePoint now,
                                      const SeverityThresholds& thresholds) noexcept {
    const core::Duration lead = overlap.start() - now;

    domain::ConflictSeverity severity = domain::ConflictSeverity::low;
    if (lead < thresholds.critical_lead) {
        severity = domain::ConflictSeverity::high;
    } else if (lead < thresholds.absorbed_lead) {
        severity = domain::ConflictSeverity::medium;
    }

    if (type == domain::ConflictType::head_on && severity == domain::ConflictSeverity::low) {
        return domain::ConflictSeverity::medium;
    }
    return severity;
}

// ------------------------------------------------------------------ identity

core::ConflictId make_conflict_id(std::string_view prefix,
                                  domain::ConflictType type,
                                  const core::ResourceId& resource,
                                  const core::RobotId& first,
                                  const core::RobotId& second,
                                  core::TimePoint overlap_start) {
    const bool ordered = first < second;
    const core::RobotId& low = ordered ? first : second;
    const core::RobotId& high = ordered ? second : first;

    std::string id;
    id.append(prefix);
    id.push_back('-');
    id.append(domain::to_string(type));
    id.push_back('-');
    id.append(resource.value());
    id.push_back('-');
    id.append(low.value());
    id.push_back('-');
    id.append(high.value());
    id.push_back('-');
    id.append(std::to_string(overlap_start.time_since_epoch().count()));
    return core::ConflictId{std::move(id)};
}

}  // namespace traffic::reservation
