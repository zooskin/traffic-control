#include "traffic/planning/route_request.h"

namespace traffic::planning {

bool is_unconstrained(const RouteConstraints& constraints) noexcept {
    return constraints.blocked_edges.empty() && constraints.blocked_nodes.empty() &&
           constraints.blocked_resources.empty() && constraints.forbidden_resources.empty() &&
           constraints.preferred_resources.empty();
}

bool allows_node(const RouteConstraints& constraints, const core::NodeId& node) {
    return !constraints.blocked_nodes.contains(node);
}

bool allows_edge(const RouteConstraints& constraints,
                 const core::EdgeId& edge,
                 const core::ResourceId& resource) {
    if (constraints.blocked_edges.contains(edge)) {
        return false;
    }
    // The resource, not the edge, is what a corridor blockage names. Checking
    // only the edge would let a route through a corridor that was taken out of
    // service, one edge at a time.
    return !constraints.blocked_resources.contains(resource) &&
           !constraints.forbidden_resources.contains(resource);
}

bool prefers_resource(const RouteConstraints& constraints, const core::ResourceId& resource) {
    return constraints.preferred_resources.contains(resource);
}

std::size_t constraint_count(const RouteConstraints& constraints) noexcept {
    return constraints.blocked_edges.size() + constraints.blocked_nodes.size() +
           constraints.blocked_resources.size() + constraints.forbidden_resources.size() +
           constraints.preferred_resources.size();
}

std::string_view to_string(RouteFailure failure) noexcept {
    switch (failure) {
        case RouteFailure::invalid_request:
            return "INVALID_REQUEST";
        case RouteFailure::unknown_start:
            return "UNKNOWN_START";
        case RouteFailure::unknown_goal:
            return "UNKNOWN_GOAL";
        case RouteFailure::blocked_start:
            return "BLOCKED_START";
        case RouteFailure::blocked_goal:
            return "BLOCKED_GOAL";
        case RouteFailure::no_route:
            return "NO_ROUTE";
    }
    return "UNKNOWN";
}

bool is_retryable(RouteFailure failure) noexcept {
    switch (failure) {
        // All three are consequences of the constraints, and constraints are
        // what a blockage clearing changes. Retrying is the correct response.
        case RouteFailure::blocked_start:
        case RouteFailure::blocked_goal:
        case RouteFailure::no_route:
            return true;

        // A malformed request or a node that is not in the map will fail the
        // same way for as long as the map and the request stay as they are.
        case RouteFailure::invalid_request:
        case RouteFailure::unknown_start:
        case RouteFailure::unknown_goal:
            return false;
    }
    return false;
}

}  // namespace traffic::planning
