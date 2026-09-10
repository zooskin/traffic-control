#pragma once

/// \file
/// What an edge costs. docs/05_GLOBAL_ROUTING.md §6~10.
///
///     cost = w_distance   * distance
///          + w_time       * travel_time
///          + w_congestion * congestion
///          + w_wait       * expected_wait
///
/// Two things about this file decide whether A* is correct.
///
/// **Costs are non-negative.** Weights are validated on the way in rather than
/// trusted, because a negative weight turns a shortest-path search into a
/// silently wrong one — A* does not fail on a negative edge, it returns a route
/// that is not the cheapest.
///
/// **Costs are static for the duration of one search.** Congestion and waiting
/// are read once, at the request's `current_time`, not at the time the search
/// projects the robot to arrive. §10 defines expected_wait against
/// `current_time`, and holding the costs still is what keeps A* optimal and
/// reproducible. Time-varying edge costs are reservation-aware planning, which
/// §19 defers.

#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/graph.h"
#include "traffic/planning/route_request.h"

namespace traffic::planning {

/// The weights of docs/05_GLOBAL_ROUTING.md §6.
///
/// The defaults are pure shortest-distance: distance 1, everything else 0.
/// That is deliberate. §6 says the weights are configuration, and no benchmark
/// has run yet — docs/15_ALGORITHM_BENCHMARK.md is Phase 15. A weight invented
/// before there is anything to measure it against would be a number with no
/// justification that later tuning has to argue with.
///
/// Note the units do not agree: distance is metres, travel time is seconds,
/// congestion is a ratio, waiting is seconds. The weights are what reconcile
/// them, which is exactly why they cannot be guessed.
struct CostWeights {
    /// Per metre of edge length.
    double distance{1.0};

    /// Per second of nominal travel time.
    double travel_time{0.0};

    /// Per unit of occupancy ratio.
    double congestion{0.0};

    /// Per second of expected wait.
    double waiting{0.0};

    /// Multiplier applied to an edge whose resource the request prefers.
    ///
    /// Below 1 makes a preferred resource cheaper. It is a factor rather than
    /// a subtraction so a preference can never drive a cost negative.
    double preferred_factor{1.0};

    [[nodiscard]] friend bool operator==(const CostWeights&, const CostWeights&) = default;
};

/// Validates weights. Rejects negatives, and a preference factor outside (0, 1].
///
/// A factor above 1 would mean "prefer this by making it more expensive"; zero
/// would make a preferred resource free and let a route loop through it
/// forever at no cost.
[[nodiscard]] core::Result<CostWeights, domain::DomainError> make_cost_weights(CostWeights weights);

/// Traffic conditions the planner reads but does not own.
///
/// docs/05_GLOBAL_ROUTING.md §9~10 wants congestion and expected waiting in the
/// cost, and both come from the reservation table — which is Phase 6, above
/// planning in the dependency order of docs/12_SOFTWARE_ARCHITECTURE.md §16.
/// This interface is how the cost gets the numbers without the dependency
/// pointing the wrong way: Phase 6 implements it, planning only calls it.
///
/// It is also the seam where reservation-aware planning arrives (§19) without
/// the planner being rewritten.
class ITrafficConditions {
public:
    ITrafficConditions() = default;
    virtual ~ITrafficConditions() = default;

    ITrafficConditions(const ITrafficConditions&) = delete;
    ITrafficConditions& operator=(const ITrafficConditions&) = delete;
    ITrafficConditions(ITrafficConditions&&) = delete;
    ITrafficConditions& operator=(ITrafficConditions&&) = delete;

    /// occupancy / capacity for \p resource. docs/05_GLOBAL_ROUTING.md §9.
    ///
    /// 0 when empty, 1 when full. May exceed 1 only if a resource is
    /// oversubscribed, which is a bug elsewhere; the cost handles it by
    /// growing rather than by asserting.
    [[nodiscard]] virtual double congestion(const core::ResourceId& resource) const = 0;

    /// How long a robot arriving at \p at is expected to wait for \p resource.
    /// docs/05_GLOBAL_ROUTING.md §10: `next_available_time - current_time`.
    ///
    /// Never negative. A resource already free returns zero.
    [[nodiscard]] virtual core::Duration expected_wait(const core::ResourceId& resource,
                                                       core::TimePoint at) const = 0;
};

/// Conditions with no traffic in them: nothing is congested, nothing waits.
///
/// The Phase 3 default, and the right one — until reservations exist there is
/// nothing to report, and reporting a guess would make routes vary for reasons
/// that are not real. Also the control in tests: any route that changes under
/// these conditions changed because of the graph, not the traffic.
class FreeFlowConditions final : public ITrafficConditions {
public:
    [[nodiscard]] double congestion(const core::ResourceId& resource) const override;
    [[nodiscard]] core::Duration expected_wait(const core::ResourceId& resource,
                                               core::TimePoint at) const override;
};

/// Seconds, as a double. Costs are real-valued; durations are integral
/// nanoseconds. This is the one place the two meet.
[[nodiscard]] double to_seconds(core::Duration duration) noexcept;

/// The cost of traversing \p edge, which belongs to \p resource.
///
/// \p at is the request's `current_time`, held fixed across a search.
[[nodiscard]] double edge_cost(const domain::Edge& edge,
                               const core::ResourceId& resource,
                               const CostWeights& weights,
                               const RouteConstraints& constraints,
                               const ITrafficConditions& conditions,
                               core::TimePoint at);

/// The least a metre of travel can possibly cost under these weights.
///
/// This is what makes the heuristic admissible. A* may not overestimate, and
/// the cheapest any real edge can be, per metre, is
///
///     preferred_factor * (w_distance + w_time / max_speed)
///
/// because congestion and waiting only ever add. Scaling straight-line
/// distance by this can therefore never exceed the true remaining cost.
///
/// \p max_speed_limit is the fastest edge in the map; a faster edge would
/// spend less time per metre and break the bound, so it must be the maximum
/// over the whole map and not a local value.
///
/// Returns 0 when \p max_speed_limit is not positive, which degrades the
/// heuristic to zero — A* becomes Dijkstra, slower but still correct.
[[nodiscard]] double min_cost_per_metre(const CostWeights& weights,
                                        double max_speed_limit) noexcept;

}  // namespace traffic::planning
