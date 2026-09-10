#include "traffic/planning/route_stability.h"

namespace traffic::planning {
namespace {

using PolicyResult = core::Result<RouteStabilityPolicy, domain::DomainError>;

}  // namespace

RouteStabilityPolicy always_replan_policy() noexcept {
    return RouteStabilityPolicy{0.0, core::Duration::zero()};
}

core::Result<RouteStabilityPolicy, domain::DomainError> make_stability_policy(
    RouteStabilityPolicy policy) {
    if (policy.minimum_improvement < 0.0 || policy.minimum_hold_time < core::Duration::zero()) {
        return PolicyResult::failure(domain::DomainError::negative_value);
    }
    if (policy.minimum_improvement >= 1.0) {
        // Demanding a 100% improvement means demanding a free route. No replan
        // would ever be adopted, and a robot would keep driving a route that
        // had become impossible.
        return PolicyResult::failure(domain::DomainError::non_positive_value);
    }
    return PolicyResult::success(policy);
}

std::string_view to_string(ReplanDecision decision) noexcept {
    switch (decision) {
        case ReplanDecision::adopted_current_invalid:
            return "ADOPTED_CURRENT_INVALID";
        case ReplanDecision::adopted_improved:
            return "ADOPTED_IMPROVED";
        case ReplanDecision::kept_insufficient_improvement:
            return "KEPT_INSUFFICIENT_IMPROVEMENT";
        case ReplanDecision::kept_within_hold_time:
            return "KEPT_WITHIN_HOLD_TIME";
    }
    return "UNKNOWN";
}

bool is_adopted(ReplanDecision decision) noexcept {
    return decision == ReplanDecision::adopted_current_invalid ||
           decision == ReplanDecision::adopted_improved;
}

ReplanDecision decide_replan(const RouteStabilityPolicy& policy,
                             double current_cost,
                             double candidate_cost,
                             bool current_valid,
                             core::Duration held_for) noexcept {
    // Order matters. A route that cannot be driven is replaced no matter what
    // the thresholds say; asking whether the alternative is 10% cheaper than a
    // route through a closed corridor is asking the wrong question.
    if (!current_valid) {
        return ReplanDecision::adopted_current_invalid;
    }

    if (held_for < policy.minimum_hold_time) {
        // §17. This is the only check that can see an oscillation: every step
        // of an A -> B -> A cycle is an improvement when it is taken, so the
        // improvement threshold alone will not stop one.
        return ReplanDecision::kept_within_hold_time;
    }

    const double threshold = current_cost * (1.0 - policy.minimum_improvement);
    if (candidate_cost < threshold) {
        return ReplanDecision::adopted_improved;
    }
    return ReplanDecision::kept_insufficient_improvement;
}

core::Duration route_age(const domain::Route& route, core::TimePoint now) noexcept {
    if (now <= route.created_at) {
        return core::Duration::zero();
    }
    return now - route.created_at;
}

}  // namespace traffic::planning
