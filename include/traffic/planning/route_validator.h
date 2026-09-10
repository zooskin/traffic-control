#pragma once

/// \file
/// Is this route still usable? docs/05_GLOBAL_ROUTING.md §20, §24.
///
/// A route is planned once and then acted on for as long as the robot takes to
/// drive it. In between, a corridor can be taken out of service, an edge can be
/// disabled, and the map can be replaced. `make_route` checked that the route
/// was a connected walk when it was built; it could not check that the world
/// still agrees.
///
/// This is the check that stands between a stale route and a robot driving into
/// a closed aisle, so it reports every defect rather than the first — a route
/// rejected for one reason is usually wrong for several, and a controller
/// deciding between WAIT and REPLAN (§13) needs to know which.
///
/// Deliberately separate from the planner. Validating a route is not planning
/// one: the traffic controller, the reservation manager and the replanner all
/// need the answer, and none of them should have to hold a planner to get it.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "traffic/domain/route.h"
#include "traffic/map/map.h"
#include "traffic/planning/route_request.h"

namespace traffic::planning {

/// What is wrong with a route.
enum class RouteDefect {
    /// No nodes at all.
    empty_route,

    /// Segment and node counts do not describe a walk.
    inconsistent_shape,

    /// A node on the route is not in the map.
    unknown_node,

    /// A segment names an edge the map does not contain.
    unknown_edge,

    /// The edge does not join the two nodes the segment claims.
    edge_does_not_connect,

    /// The edge exists and is switched off.
    edge_disabled,

    /// The edge cannot legally be driven in the direction the route uses it.
    ///
    /// The one-way check. A route built from `incident_edges` rather than
    /// `traversable_edges` looks perfectly connected and is wrong here.
    wrong_direction,

    /// A node on the route is blocked by the constraints.
    blocked_node,

    /// An edge on the route is blocked by the constraints.
    blocked_edge,

    /// The resource an edge belongs to is blocked or forbidden.
    blocked_resource,

    /// The route was planned against an older map.
    /// docs/23_SYSTEM_ARCHITECTURE.md §16.
    stale_map_version,
};

[[nodiscard]] std::string_view to_string(RouteDefect defect) noexcept;

/// One problem with a route.
struct RouteIssue {
    RouteDefect defect;

    /// The node, edge or resource concerned.
    std::string subject;

    /// Where in the route, as a segment index. `route.segments.size()` when
    /// the finding is about the route as a whole.
    std::size_t segment{0};

    std::string detail;
};

/// Everything found in one pass over a route.
class RouteValidation {
public:
    void add(RouteDefect defect, std::string subject, std::size_t segment, std::string detail);

    [[nodiscard]] const std::vector<RouteIssue>& issues() const noexcept { return issues_; }

    /// True when nothing is wrong. Unlike map validation there is no warning
    /// tier: a route is either drivable now or it is not.
    [[nodiscard]] bool is_valid() const noexcept { return issues_.empty(); }

    /// True when at least one issue is of this kind.
    [[nodiscard]] bool has(RouteDefect defect) const noexcept;

    /// The findings, one per line.
    [[nodiscard]] std::string to_string() const;

private:
    std::vector<RouteIssue> issues_;
};

/// Checks \p route against \p map and \p constraints.
[[nodiscard]] RouteValidation validate_route(const map::Map& map,
                                             const domain::Route& route,
                                             const RouteConstraints& constraints);

/// The same question, answered yes or no.
[[nodiscard]] bool is_route_valid(const map::Map& map,
                                  const domain::Route& route,
                                  const RouteConstraints& constraints);

}  // namespace traffic::planning
