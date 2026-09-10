#include "traffic/domain/route.h"

#include <utility>

namespace traffic::domain {
namespace {

using RouteResult = core::Result<Route, DomainError>;

}  // namespace

std::optional<core::NodeId> start_node(const Route& route) {
    if (route.nodes.empty()) {
        return std::nullopt;
    }
    return route.nodes.front();
}

std::optional<core::NodeId> goal_node(const Route& route) {
    if (route.nodes.empty()) {
        return std::nullopt;
    }
    return route.nodes.back();
}

bool is_trivial(const Route& route) noexcept {
    return route.segments.empty();
}

bool is_stale_against_map(const Route& route, MapVersion current) noexcept {
    return route.map_version.is_stale_against(current);
}

bool is_expired(const Route& route, core::TimePoint now) noexcept {
    return route.expires_at.has_value() && now >= *route.expires_at;
}

std::vector<core::EdgeId> traversed_edges(const Route& route) {
    std::vector<core::EdgeId> edges;
    edges.reserve(route.segments.size());
    for (const auto& segment : route.segments) {
        edges.push_back(segment.edge_id);
    }
    return edges;
}

core::Result<Route, DomainError> make_route(core::RouteId id,
                                            core::RobotId robot_id,
                                            std::vector<core::NodeId> nodes,
                                            std::vector<RouteSegment> segments,
                                            MapVersion map_version,
                                            core::TimePoint created_at) {
    if (id.empty() || robot_id.empty()) {
        return RouteResult::failure(DomainError::empty_id);
    }
    if (nodes.empty()) {
        return RouteResult::failure(DomainError::empty_route);
    }

    // A walk over n edges visits n+1 nodes. Anything else is not a path.
    if (segments.size() + 1 != nodes.size()) {
        return RouteResult::failure(DomainError::inconsistent_route);
    }

    for (std::size_t i = 0; i < segments.size(); ++i) {
        const RouteSegment& segment = segments[i];

        if (segment.edge_id.empty()) {
            return RouteResult::failure(DomainError::empty_id);
        }
        if (segment.sequence != i) {
            return RouteResult::failure(DomainError::inconsistent_route);
        }
        // Each segment must start where the previous one ended, and match the
        // node list. Without this a route can be reserved resource by resource
        // and only fail on the first move the robot cannot physically make.
        if (segment.from_node != nodes[i] || segment.to_node != nodes[i + 1]) {
            return RouteResult::failure(DomainError::inconsistent_route);
        }
    }

    Route route;
    route.id = std::move(id);
    route.robot_id = std::move(robot_id);
    route.nodes = std::move(nodes);
    route.segments = std::move(segments);
    route.map_version = map_version;
    route.created_at = created_at;
    return RouteResult::success(std::move(route));
}

}  // namespace traffic::domain
