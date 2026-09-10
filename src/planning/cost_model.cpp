#include "traffic/planning/cost_model.h"

#include <chrono>

namespace traffic::planning {
namespace {

using WeightsResult = core::Result<CostWeights, domain::DomainError>;

}  // namespace

core::Result<CostWeights, domain::DomainError> make_cost_weights(CostWeights weights) {
    if (weights.distance < 0.0 || weights.travel_time < 0.0 || weights.congestion < 0.0 ||
        weights.waiting < 0.0) {
        // A negative weight does not make A* fail — it makes it return a route
        // that is not the cheapest, quietly. Refuse it at the boundary.
        return WeightsResult::failure(domain::DomainError::negative_value);
    }
    if (weights.preferred_factor <= 0.0 || weights.preferred_factor > 1.0) {
        return WeightsResult::failure(domain::DomainError::non_positive_value);
    }
    return WeightsResult::success(weights);
}

double FreeFlowConditions::congestion(const core::ResourceId& /*resource*/) const {
    return 0.0;
}

core::Duration FreeFlowConditions::expected_wait(const core::ResourceId& /*resource*/,
                                                 core::TimePoint /*at*/) const {
    return core::Duration::zero();
}

double to_seconds(core::Duration duration) noexcept {
    return std::chrono::duration_cast<std::chrono::duration<double>>(duration).count();
}

double edge_cost(const domain::Edge& edge,
                 const core::ResourceId& resource,
                 const CostWeights& weights,
                 const RouteConstraints& constraints,
                 const ITrafficConditions& conditions,
                 core::TimePoint at) {
    const double travel_seconds = to_seconds(domain::nominal_travel_time(edge));
    const double wait_seconds = to_seconds(conditions.expected_wait(resource, at));

    // §9 defines congestion as occupancy / capacity, so it is already a ratio
    // and needs no normalisation here. A value above 1 means the resource is
    // oversubscribed; letting the cost grow is the right response.
    const double congestion = conditions.congestion(resource);

    double cost = (weights.distance * edge.length) + (weights.travel_time * travel_seconds) +
                  (weights.congestion * congestion) + (weights.waiting * wait_seconds);

    if (prefers_resource(constraints, resource)) {
        cost *= weights.preferred_factor;
    }
    return cost;
}

double min_cost_per_metre(const CostWeights& weights, double max_speed_limit) noexcept {
    // Without a positive speed there is no bound on the time term, so drop the
    // heuristic to zero. A* then behaves as Dijkstra: slower, still correct.
    if (max_speed_limit <= 0.0) {
        return 0.0;
    }
    const double per_metre = weights.distance + (weights.travel_time / max_speed_limit);
    return weights.preferred_factor * per_metre;
}

}  // namespace traffic::planning
