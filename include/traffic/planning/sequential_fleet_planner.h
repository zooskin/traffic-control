#pragma once

/// \file
/// A batch answered one robot at a time. docs/24_DOMAIN_MODEL.md §20~21.
///
/// The bridge between the two planner interfaces of route_planner.h. It takes
/// an `IRoutePlanner` and presents an `IFleetPlanner`, so the traffic
/// controller can call the batch contract from the start without a joint
/// planner existing yet. When PIBT or ECBS arrives (past Phase 15, decision
/// D-003) it implements `IFleetPlanner` directly and this drops out of the
/// path — the controller does not change, which is what CLAUDE.md's Algorithm
/// Isolation asks for.
///
/// Planning each robot independently is not a stand-in for joint planning: it
/// is the baseline docs/15_ALGORITHM_BENCHMARK.md measures the alternatives
/// against. docs/05_GLOBAL_ROUTING.md §19 is explicit that the initial version
/// separates routing from reservation, and interaction between robots is the
/// traffic controller's to resolve.

#include <string>
#include <string_view>

#include "traffic/core/clock.h"
#include "traffic/planning/planning_request.h"
#include "traffic/planning/route_planner.h"

namespace traffic::planning {

/// Answers a `PlanningRequest` by calling an `IRoutePlanner` once per robot.
class SequentialFleetPlanner final : public IFleetPlanner {
public:
    SequentialFleetPlanner(IRoutePlanner& planner, const core::IClock& clock);

    [[nodiscard]] std::string_view name() const noexcept override;

    /// Plans every robot in request order.
    ///
    /// One robot that cannot be routed does not stop the others: its outcome
    /// carries the failure and the batch keeps going. At two hundred robots,
    /// abandoning a batch because one goal was unreachable would stall the
    /// floor over a single bad task.
    ///
    /// The deadline and timeout of §20 are checked between robots, not inside
    /// a search. A search that stopped partway would produce a different
    /// answer depending on how loaded the machine was, and
    /// docs/01_REQUIREMENTS.md NFR-003 does not allow that — a per-search
    /// bound belongs in `AStarConfig::max_expansions`, which counts work
    /// rather than time.
    [[nodiscard]] PlanningResult plan(const PlanningRequest& request) override;

private:
    IRoutePlanner& planner_;
    const core::IClock& clock_;
};

}  // namespace traffic::planning
