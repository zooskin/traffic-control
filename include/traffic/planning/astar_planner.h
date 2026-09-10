#pragma once

/// \file
/// A*. docs/05_GLOBAL_ROUTING.md §4, §19~22.
///
/// One search serves both algorithms the spec names. §3 makes Dijkstra the
/// fallback, and A* with a zero heuristic *is* Dijkstra, so there is no second
/// implementation to keep correct — inject `ZeroHeuristic` (heuristic.h) and
/// the same code answers.
///
/// Three properties this class is responsible for, in the order they matter.
///
/// **It only ever returns drivable routes.** Expansion goes through
/// `Map::traversable_edges`, not `incident_edges`. The difference is a route
/// that runs the wrong way down a one-way corridor: connected, plausible, and
/// refused by the first reservation the robot asks for.
///
/// **The same input gives the same route.** §22. Three things could break that
/// and each is pinned: neighbours come back in edge-list order rather than hash
/// order, the open set breaks ties on node id, and equal-cost paths to the same
/// node are settled on edge id. Nothing here iterates an unordered container.
///
/// **It does not know when it is.** Every instant it uses arrives in the
/// request; the injected clock is read only to stamp the route and measure how
/// long the search took. Under a `SimulationClock` that measurement is zero,
/// which is correct — a run must not vary with how fast the machine is.
///
/// One assumption is worth stating because it is easy to break silently. A
/// node, once expanded, is never revisited — the usual A* optimisation, and it
/// is sound only when the heuristic is *consistent*, not merely admissible.
/// The heuristics in heuristic.h are consistent exactly when every edge is at
/// least as long as the straight line between its endpoints, which is what
/// `geometry_supports_distance_heuristic` checks. On a map that fails it, plan
/// with `ZeroHeuristic`: zero is consistent on any graph with non-negative
/// costs.
///
/// What it deliberately does not do is avoid other robots. §19 keeps routing
/// and reservation apart in the initial version: this answers "where", and the
/// traffic controller answers "when". Congestion and expected waiting reach the
/// cost through `ITrafficConditions`, which is the seam reservation-aware
/// planning arrives through later.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "traffic/core/clock.h"
#include "traffic/core/result.h"
#include "traffic/domain/route.h"
#include "traffic/map/map.h"
#include "traffic/planning/cost_model.h"
#include "traffic/planning/heuristic.h"
#include "traffic/planning/route_planner.h"
#include "traffic/planning/route_request.h"
#include "traffic/planning/route_stability.h"

namespace traffic::planning {

/// How the planner is tuned.
struct AStarConfig {
    CostWeights weights;

    /// When to keep an existing route rather than replace it.
    RouteStabilityPolicy stability;

    /// Cap on nodes expanded in one search. 0 means no cap.
    ///
    /// A bound on the work, not on the wall clock — a time limit would have to
    /// read a real clock mid-search and would make two runs of the same
    /// scenario diverge. The KPI in CLAUDE.md is a P99 planning latency, and on
    /// a fixed map a cap on expansions is what turns that into something a
    /// search can be held to.
    std::size_t max_expansions{0};
};

/// Plans one robot's route with A*.
///
/// Holds references to the map, clock, heuristic and conditions and owns none
/// of them (CLAUDE.md, Dependency Injection). All four must outlive it.
class AStarPlanner final : public IRoutePlanner {
public:
    AStarPlanner(const map::Map& map,
                 const core::IClock& clock,
                 const IHeuristic& heuristic,
                 const ITrafficConditions& conditions,
                 AStarConfig config);

    [[nodiscard]] std::string_view name() const noexcept override;

    [[nodiscard]] core::Result<RouteResponse, RouteFailure> plan_route(
        const RouteRequest& request) override;

    /// Plans an alternative and applies the stability policy of §16~17.
    ///
    /// Returns the route that should be in force afterwards, which may be
    /// \p current_route unchanged. `RouteResponse::replan_decision` says which
    /// happened and why.
    [[nodiscard]] core::Result<RouteResponse, RouteFailure> replan_route(
        const RouteRequest& request, const domain::Route& current_route) override;

    /// Infinity when the route uses an edge the map does not have — an
    /// unusable route is not cheap, and returning a finite cost for one would
    /// let it win a replan comparison.
    [[nodiscard]] double estimate_route_cost(const domain::Route& route,
                                             const RouteConstraints& constraints,
                                             core::TimePoint at) const override;

    [[nodiscard]] bool is_route_valid(const domain::Route& route,
                                      const RouteConstraints& constraints) const override;

    [[nodiscard]] const AStarConfig& config() const noexcept { return config_; }

private:
    /// The name reported for the log record of §23: "astar", or "dijkstra"
    /// when the heuristic estimates nothing.
    [[nodiscard]] std::string_view planner_name() const noexcept;

    const map::Map& map_;
    const core::IClock& clock_;
    const IHeuristic& heuristic_;
    const ITrafficConditions& conditions_;
    AStarConfig config_;

    /// Makes route ids unique within one planner. Deterministic: a fresh
    /// planner replaying the same requests issues the same ids.
    std::uint64_t route_sequence_{0};
};

}  // namespace traffic::planning
