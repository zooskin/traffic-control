#pragma once

/// \file
/// The planner contracts. docs/05_GLOBAL_ROUTING.md §20,
/// docs/24_DOMAIN_MODEL.md §20~21.
///
/// There are two, because the two documents describe two different callers.
///
///   IRoutePlanner   one robot, one route. docs/05_GLOBAL_ROUTING.md §20.
///   IFleetPlanner   a batch. docs/24_DOMAIN_MODEL.md §20~21.
///
/// A* answers one robot at a time and knows nothing about the others. PIBT and
/// ECBS — deferred past Phase 15 by decision D-003 — solve the batch jointly
/// and cannot be expressed as a loop over single requests. If the traffic
/// controller called the single-robot interface directly it would have to be
/// rewritten to adopt any of them, and CLAUDE.md's Algorithm Isolation says it
/// must not be able to tell them apart. So the controller calls the batch, and
/// `SequentialFleetPlanner` (sequential_fleet_planner.h) bridges the two for
/// planners that work one robot at a time.
///
/// Neither interface reads a clock, allocates an id from a global, or touches
/// the reservation table. Everything time-dependent arrives in the request.

#include <string_view>

#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/route.h"
#include "traffic/planning/planning_request.h"
#include "traffic/planning/route_request.h"

namespace traffic::planning {

/// Plans one robot's route. docs/05_GLOBAL_ROUTING.md §20.
class IRoutePlanner {
public:
    IRoutePlanner() = default;
    virtual ~IRoutePlanner() = default;

    IRoutePlanner(const IRoutePlanner&) = delete;
    IRoutePlanner& operator=(const IRoutePlanner&) = delete;
    IRoutePlanner(IRoutePlanner&&) = delete;
    IRoutePlanner& operator=(IRoutePlanner&&) = delete;

    /// Which algorithm this is, for the log record of §23 and for the
    /// benchmark of docs/15_ALGORITHM_BENCHMARK.md. Callers may record it;
    /// they must not branch on it.
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /// Computes a route from scratch.
    [[nodiscard]] virtual core::Result<RouteResponse, RouteFailure> plan_route(
        const RouteRequest& request) = 0;

    /// Recomputes a route for a robot that already has one.
    ///
    /// Separate from `plan_route` because it can answer "keep what you have".
    /// docs/05_GLOBAL_ROUTING.md §16~17: a new route is adopted only when it is
    /// enough of an improvement, and not if the current one is too fresh.
    /// Replanning on every small change produces route oscillation, which
    /// costs more in re-reservation than the shorter path saves.
    [[nodiscard]] virtual core::Result<RouteResponse, RouteFailure> replan_route(
        const RouteRequest& request, const domain::Route& current_route) = 0;

    /// What \p route costs under this planner's weights and current
    /// conditions. docs/05_GLOBAL_ROUTING.md §20.
    ///
    /// Comparable with `RouteResponse::cost` from the same planner and with
    /// nothing else.
    ///
    /// §20 writes this as `estimate_route_cost(route)`. The instant is an
    /// added parameter, not an omission from the spec: congestion and expected
    /// waiting are evaluated at a point in time (§9~10), and a planner that
    /// picked that instant itself would have to read a clock — which
    /// docs/20_CODING_GUIDELINES.md §17 forbids.
    [[nodiscard]] virtual double estimate_route_cost(const domain::Route& route,
                                                     const RouteConstraints& constraints,
                                                     core::TimePoint at) const = 0;

    /// True when \p route is still traversable on the current map under
    /// \p constraints. docs/05_GLOBAL_ROUTING.md §20.
    ///
    /// `validate_route` (route_validator.h) answers the same question and says
    /// why. Use this when only the answer matters.
    [[nodiscard]] virtual bool is_route_valid(const domain::Route& route,
                                              const RouteConstraints& constraints) const = 0;
};

/// Plans for several robots at once. docs/24_DOMAIN_MODEL.md §20~21.
class IFleetPlanner {
public:
    IFleetPlanner() = default;
    virtual ~IFleetPlanner() = default;

    IFleetPlanner(const IFleetPlanner&) = delete;
    IFleetPlanner& operator=(const IFleetPlanner&) = delete;
    IFleetPlanner(IFleetPlanner&&) = delete;
    IFleetPlanner& operator=(IFleetPlanner&&) = delete;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /// Answers every request in \p request, in order.
    ///
    /// Never throws on a robot that cannot be routed: that robot's outcome
    /// carries the failure and the rest are still returned. One unreachable
    /// goal must not stall a fleet of two hundred.
    [[nodiscard]] virtual PlanningResult plan(const PlanningRequest& request) = 0;
};

}  // namespace traffic::planning
