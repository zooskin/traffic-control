#pragma once

/// \file
/// What gets recorded about a planning decision.
/// docs/05_GLOBAL_ROUTING.md §23.
///
/// §23 lists ten fields every planning request must record. This is that list
/// as a struct rather than as a format string, for two reasons.
///
/// Planning must not depend on infrastructure. CLAUDE.md keeps domain and
/// infrastructure apart, and a planner that called into a logger would drag
/// spdlog below the line and make every planning test need a sink.
///
/// And a record that is data can be asserted on. "Did the planner record why
/// it kept the old route" is a test; "did it print the right line" is not.
/// The caller — the traffic controller — decides where it goes.

#include <cstddef>
#include <optional>
#include <string>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/route.h"
#include "traffic/planning/route_request.h"
#include "traffic/planning/route_stability.h"

namespace traffic::planning {

/// One planning decision, in the fields docs/05_GLOBAL_ROUTING.md §23 names.
struct PlanningRecord {
    core::RobotId robot_id;
    core::NodeId start;
    core::NodeId goal;

    /// The algorithm that ran — "astar", "dijkstra".
    std::string planner;

    /// The route in force before. Empty on a first plan.
    std::optional<core::RouteId> old_route;

    /// The route in force after. Empty when planning failed.
    std::optional<core::RouteId> new_route;

    double cost{0.0};

    /// §23's planning_latency, measured on the injected clock.
    core::Duration planning_latency{core::Duration::zero()};

    /// How many restrictions the request carried. The ids themselves are in
    /// the request; what a log line needs is whether there were any.
    std::size_t constraints{0};

    /// Why the route was planned — the replan trigger of §14, when there was
    /// one.
    std::string reason;

    /// Set when planning failed.
    std::optional<RouteFailure> failure;

    /// Set on a replan: whether the alternative was taken.
    std::optional<ReplanDecision> decision;

    std::size_t expanded_nodes{0};

    /// True when the robot's route is not what it was.
    ///
    /// Not the same as "planning succeeded": a replan can succeed and
    /// deliberately change nothing (§16~17).
    [[nodiscard]] bool changed_route() const noexcept;
};

/// Builds the record for a planning call.
///
/// \p current_route is the route the robot held beforehand, or nullptr on a
/// first plan.
[[nodiscard]] PlanningRecord make_planning_record(
    const RouteRequest& request,
    const core::Result<RouteResponse, RouteFailure>& result,
    const domain::Route* current_route);

/// The record as one line, fields in a fixed order.
///
/// Fixed order matters: these lines are compared between runs to show that a
/// scenario replayed identically, and a field order that varied would make
/// two identical runs look different.
[[nodiscard]] std::string to_log_line(const PlanningRecord& record);

}  // namespace traffic::planning
