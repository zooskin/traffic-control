#include "traffic/planning/route_validator.h"

#include <utility>

#include "traffic/domain/graph.h"

namespace traffic::planning {
namespace {

/// Checks one segment. Split out because the whole-route pass would otherwise
/// carry more branches than docs/20_CODING_GUIDELINES.md allows in one
/// function, and because each check reads better next to its own reason.
void validate_segment(const map::Map& map,
                      const domain::RouteSegment& segment,
                      const RouteConstraints& constraints,
                      std::size_t index,
                      RouteValidation& validation) {
    const domain::Edge* edge = map.find_edge(segment.edge_id);
    if (edge == nullptr) {
        validation.add(
            RouteDefect::unknown_edge, segment.edge_id.value(), index, "the map has no such edge");
        return;
    }

    const bool joins_forward =
        edge->from_node == segment.from_node && edge->to_node == segment.to_node;
    const bool joins_reverse =
        edge->from_node == segment.to_node && edge->to_node == segment.from_node;

    if (!joins_forward && !joins_reverse) {
        validation.add(RouteDefect::edge_does_not_connect,
                       segment.edge_id.value(),
                       index,
                       "the edge does not join " + segment.from_node.value() + " and " +
                           segment.to_node.value());
        return;
    }

    if (!edge->enabled) {
        validation.add(
            RouteDefect::edge_disabled, segment.edge_id.value(), index, "the edge is disabled");
    }

    // The check a route built from incident edges passes and should not: the
    // walk is connected, and one of its steps runs the wrong way down a
    // one-way corridor.
    if (!domain::is_traversable_from(*edge, segment.from_node)) {
        validation.add(RouteDefect::wrong_direction,
                       segment.edge_id.value(),
                       index,
                       "the edge cannot be driven from " + segment.from_node.value());
    }

    const core::ResourceId resource = map.resource_for_edge(segment.edge_id);

    if (constraints.blocked_edges.contains(segment.edge_id)) {
        validation.add(RouteDefect::blocked_edge,
                       segment.edge_id.value(),
                       index,
                       "the edge is blocked by the request constraints");
    } else if (!allows_edge(constraints, segment.edge_id, resource)) {
        // Reported against the resource, not the edge: a corridor closure is
        // one fact about one resource, and naming each of its edges in turn
        // would bury it.
        validation.add(RouteDefect::blocked_resource,
                       resource.value(),
                       index,
                       "the resource this edge belongs to is blocked or forbidden");
    }
}

}  // namespace

std::string_view to_string(RouteDefect defect) noexcept {
    switch (defect) {
        case RouteDefect::empty_route:
            return "EMPTY_ROUTE";
        case RouteDefect::inconsistent_shape:
            return "INCONSISTENT_SHAPE";
        case RouteDefect::unknown_node:
            return "UNKNOWN_NODE";
        case RouteDefect::unknown_edge:
            return "UNKNOWN_EDGE";
        case RouteDefect::edge_does_not_connect:
            return "EDGE_DOES_NOT_CONNECT";
        case RouteDefect::edge_disabled:
            return "EDGE_DISABLED";
        case RouteDefect::wrong_direction:
            return "WRONG_DIRECTION";
        case RouteDefect::blocked_node:
            return "BLOCKED_NODE";
        case RouteDefect::blocked_edge:
            return "BLOCKED_EDGE";
        case RouteDefect::blocked_resource:
            return "BLOCKED_RESOURCE";
        case RouteDefect::stale_map_version:
            return "STALE_MAP_VERSION";
    }
    return "UNKNOWN";
}

void RouteValidation::add(RouteDefect defect,
                          std::string subject,
                          std::size_t segment,
                          std::string detail) {
    issues_.push_back(RouteIssue{defect, std::move(subject), segment, std::move(detail)});
}

bool RouteValidation::has(RouteDefect defect) const noexcept {
    for (const RouteIssue& issue : issues_) {
        if (issue.defect == defect) {
            return true;
        }
    }
    return false;
}

std::string RouteValidation::to_string() const {
    std::string text;
    for (const RouteIssue& issue : issues_) {
        text += planning::to_string(issue.defect);
        text += " [";
        text += issue.subject;
        text += "] ";
        text += issue.detail;
        text += '\n';
    }
    return text;
}

RouteValidation validate_route(const map::Map& map,
                               const domain::Route& route,
                               const RouteConstraints& constraints) {
    RouteValidation validation;

    if (route.nodes.empty()) {
        validation.add(RouteDefect::empty_route, route.id.value(), 0, "the route has no nodes");
        return validation;
    }

    if (route.segments.size() + 1 != route.nodes.size()) {
        // Nothing further can be trusted: the segments and the node list are
        // not describing the same walk, so which one is wrong is unknowable.
        validation.add(RouteDefect::inconsistent_shape,
                       route.id.value(),
                       route.segments.size(),
                       "a walk over n edges visits n+1 nodes");
        return validation;
    }

    if (domain::is_stale_against_map(route, map.version())) {
        validation.add(RouteDefect::stale_map_version,
                       route.id.value(),
                       route.segments.size(),
                       "the route was planned against an older map");
    }

    for (std::size_t i = 0; i < route.nodes.size(); ++i) {
        const core::NodeId& node = route.nodes[i];
        if (!map.has_node(node)) {
            validation.add(RouteDefect::unknown_node, node.value(), i, "the map has no such node");
            continue;
        }
        if (!allows_node(constraints, node)) {
            validation.add(RouteDefect::blocked_node,
                           node.value(),
                           i,
                           "the node is blocked by the request constraints");
        }
    }

    for (std::size_t i = 0; i < route.segments.size(); ++i) {
        validate_segment(map, route.segments[i], constraints, i, validation);
    }

    return validation;
}

bool is_route_valid(const map::Map& map,
                    const domain::Route& route,
                    const RouteConstraints& constraints) {
    return validate_route(map, route, constraints).is_valid();
}

}  // namespace traffic::planning
