#include "traffic/planning/sequential_fleet_planner.h"

#include <utility>

namespace traffic::planning {
namespace {

/// True when the planner has run out of the budget \p request gave it.
///
/// Two independent limits, both from docs/24_DOMAIN_MODEL.md §20: a deadline
/// is a point on the traffic timeline, a timeout is a span from when the batch
/// started. A caller may give either, both, or neither.
[[nodiscard]] bool out_of_time(const PlanningRequest& request,
                               core::TimePoint started,
                               core::TimePoint now) {
    if (request.deadline.has_value() && now >= *request.deadline) {
        return true;
    }
    return request.timeout.has_value() && (now - started) >= *request.timeout;
}

}  // namespace

SequentialFleetPlanner::SequentialFleetPlanner(IRoutePlanner& planner, const core::IClock& clock)
    : planner_(planner), clock_(clock) {}

std::string_view SequentialFleetPlanner::name() const noexcept {
    // Reports the planner it wraps, not itself. What the log record of
    // docs/05_GLOBAL_ROUTING.md §23 wants to know is which algorithm produced
    // the routes, and "sequential" would not answer that.
    return planner_.name();
}

PlanningResult SequentialFleetPlanner::plan(const PlanningRequest& request) {
    const core::TimePoint started = clock_.now();

    PlanningResult result;
    result.request_id = request.request_id;
    result.map_version = request.map_version;
    result.traffic_state_version = request.traffic_state_version;
    result.outcomes.reserve(request.routes.size());

    bool timed_out = false;

    for (const RouteRequest& route_request : request.routes) {
        if (out_of_time(request, started, clock_.now())) {
            timed_out = true;
            break;
        }

        RouteOutcome outcome;
        outcome.robot_id = route_request.robot_id;

        auto planned = planner_.plan_route(route_request);
        if (planned.has_value()) {
            outcome.response = std::move(planned).value();
        } else {
            outcome.failure = planned.error();
        }
        result.outcomes.push_back(std::move(outcome));
    }

    if (timed_out) {
        // The robots already planned keep their routes. Throwing them away
        // because the batch ran long would waste work that is perfectly good,
        // and the caller can re-request the rest.
        for (std::size_t i = result.outcomes.size(); i < request.routes.size(); ++i) {
            RouteOutcome skipped;
            skipped.robot_id = request.routes[i].robot_id;
            result.outcomes.push_back(std::move(skipped));
        }
        result.status = PlanningStatus::timeout;
    } else {
        result.status = status_for(result.outcomes);
    }

    result.planning_time = clock_.now() - started;
    return result;
}

}  // namespace traffic::planning
