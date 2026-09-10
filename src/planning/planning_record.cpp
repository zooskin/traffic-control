#include "traffic/planning/planning_record.h"

#include <sstream>

#include "traffic/planning/cost_model.h"

namespace traffic::planning {
namespace {

/// An id, or a dash. A log line with an empty field between two separators is
/// harder to read than one that says nothing is there.
[[nodiscard]] std::string or_dash(const std::optional<core::RouteId>& id) {
    return id.has_value() ? id->value() : "-";
}

}  // namespace

bool PlanningRecord::changed_route() const noexcept {
    if (!new_route.has_value()) {
        return false;
    }
    if (!old_route.has_value()) {
        return true;
    }
    return *new_route != *old_route;
}

PlanningRecord make_planning_record(const RouteRequest& request,
                                    const core::Result<RouteResponse, RouteFailure>& result,
                                    const domain::Route* current_route) {
    PlanningRecord record;
    record.robot_id = request.robot_id;
    record.start = request.start_node;
    record.goal = request.goal_node;
    record.constraints = constraint_count(request.constraints);
    record.reason = request.reason;

    if (current_route != nullptr) {
        record.old_route = current_route->id;
    }

    if (!result.has_value()) {
        record.failure = result.error();
        return record;
    }

    const RouteResponse& response = result.value();
    record.planner = response.planner;
    record.new_route = response.route.id;
    record.cost = response.cost;
    record.planning_latency = response.planning_time;
    record.expanded_nodes = response.expanded_nodes;
    record.decision = response.replan_decision;
    return record;
}

std::string to_log_line(const PlanningRecord& record) {
    std::ostringstream line;
    line << "robot=" << record.robot_id.value() << " start=" << record.start.value()
         << " goal=" << record.goal.value()
         << " planner=" << (record.planner.empty() ? "-" : record.planner)
         << " old_route=" << or_dash(record.old_route) << " new_route=" << or_dash(record.new_route)
         << " cost=" << record.cost << " latency_s=" << to_seconds(record.planning_latency)
         << " expanded=" << record.expanded_nodes << " constraints=" << record.constraints
         << " reason=" << (record.reason.empty() ? "-" : record.reason);

    if (record.failure.has_value()) {
        line << " failure=" << to_string(*record.failure);
    }
    if (record.decision.has_value()) {
        line << " decision=" << to_string(*record.decision);
    }
    return line.str();
}

}  // namespace traffic::planning
