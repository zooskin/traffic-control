#include "traffic/planning/planning_request.h"

namespace traffic::planning {

std::string_view to_string(PlanningStatus status) noexcept {
    switch (status) {
        case PlanningStatus::success:
            return "SUCCESS";
        case PlanningStatus::no_path:
            return "NO_PATH";
        case PlanningStatus::timeout:
            return "TIMEOUT";
        case PlanningStatus::cancelled:
            return "CANCELLED";
        case PlanningStatus::failed:
            return "FAILED";
    }
    return "UNKNOWN";
}

std::size_t succeeded_count(const PlanningResult& result) noexcept {
    std::size_t count = 0;
    for (const RouteOutcome& outcome : result.outcomes) {
        if (outcome.succeeded()) {
            ++count;
        }
    }
    return count;
}

std::vector<const RouteResponse*> successful_routes(const PlanningResult& result) {
    std::vector<const RouteResponse*> routes;
    routes.reserve(result.outcomes.size());
    for (const RouteOutcome& outcome : result.outcomes) {
        if (outcome.response.has_value()) {
            routes.push_back(&*outcome.response);
        }
    }
    return routes;
}

PlanningStatus status_for(const std::vector<RouteOutcome>& outcomes) noexcept {
    // An empty batch succeeded at planning nothing. Treating it as a failure
    // would make an idle fleet look broken.
    if (outcomes.empty()) {
        return PlanningStatus::success;
    }

    bool any_failed = false;
    bool any_hard_failure = false;

    for (const RouteOutcome& outcome : outcomes) {
        if (outcome.succeeded()) {
            continue;
        }
        any_failed = true;

        // A request that will never succeed is a different kind of answer from
        // a goal that is merely unreachable right now. NO_PATH invites a retry
        // once the traffic moves; FAILED does not.
        if (outcome.failure.has_value() && !is_retryable(*outcome.failure)) {
            any_hard_failure = true;
        }
    }

    if (any_hard_failure) {
        return PlanningStatus::failed;
    }
    if (any_failed) {
        return PlanningStatus::no_path;
    }
    return PlanningStatus::success;
}

}  // namespace traffic::planning
