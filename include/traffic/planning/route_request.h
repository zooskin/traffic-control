#pragma once

/// \file
/// What the planner is asked for, and what it answers.
/// docs/05_GLOBAL_ROUTING.md §11~13, §18.
///
/// This is the single-robot shape. docs/24_DOMAIN_MODEL.md §20~21 describes a
/// *batch* request over several robots at once; that is a different type
/// (planning_request.h), not a contradiction. One route is planned here; a
/// fleet-wide plan is a collection of them.
///
/// Constraints are part of the request rather than planner state. The same
/// planner instance serves every robot, and a robot forbidden from a resource
/// must not leak that restriction into the next robot's route.

#include <cstddef>
#include <optional>
#include <set>
#include <string>
#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/route.h"
#include "traffic/domain/values.h"
#include "traffic/planning/route_stability.h"

namespace traffic::planning {

/// Restrictions the planner must respect. docs/05_GLOBAL_ROUTING.md §18.
///
/// `blocked_*` and `forbidden_resources` both exclude, and they are kept apart
/// because they mean different things and expire differently:
///
///   blocked    — unusable *right now*: a human in the aisle, a failed robot,
///                a corridor taken out of service. Transient, and the reason a
///                replan was triggered in the first place (§14).
///   forbidden  — this robot may never use it: a lift it does not fit in, a
///                zone its licence excludes. A property of the robot.
///
/// Collapsing them would work today and lose the distinction the moment
/// anything wants to ask why a route avoided a resource.
///
/// Ordered sets, not hashed. docs/20_CODING_GUIDELINES.md §23: iteration order
/// must be reproducible, and these are iterated when a decision is logged.
struct RouteConstraints {
    std::set<core::EdgeId> blocked_edges;
    std::set<core::NodeId> blocked_nodes;
    std::set<core::ResourceId> blocked_resources;

    std::set<core::ResourceId> forbidden_resources;

    /// Resources to route through when it is not much more expensive.
    /// Applied as a cost factor, never as a hard requirement — a preference
    /// that could make a route impossible is a constraint, not a preference.
    std::set<core::ResourceId> preferred_resources;

    [[nodiscard]] friend bool operator==(const RouteConstraints&,
                                         const RouteConstraints&) = default;
};

/// True when nothing is restricted.
[[nodiscard]] bool is_unconstrained(const RouteConstraints& constraints) noexcept;

/// True when a robot may stand on \p node.
[[nodiscard]] bool allows_node(const RouteConstraints& constraints, const core::NodeId& node);

/// True when a robot may traverse \p edge, which belongs to \p resource.
///
/// Both are needed: blocking a corridor blocks every edge inside it, and
/// docs/00_MASTER_PLAN.md §4.2 means an edge's own id is generally not the
/// resource that admits robots.
[[nodiscard]] bool allows_edge(const RouteConstraints& constraints,
                               const core::EdgeId& edge,
                               const core::ResourceId& resource);

/// True when \p resource is on the preferred list.
[[nodiscard]] bool prefers_resource(const RouteConstraints& constraints,
                                    const core::ResourceId& resource);

/// How many restrictions are set. For the log record of §23.
[[nodiscard]] std::size_t constraint_count(const RouteConstraints& constraints) noexcept;

/// A request for one robot's route. docs/05_GLOBAL_ROUTING.md §11.
struct RouteRequest {
    core::RobotId robot_id;
    core::NodeId start_node;
    core::NodeId goal_node;

    /// The moment the route is planned for.
    ///
    /// Supplied by the caller from its `IClock`, never read from a clock in
    /// here. Congestion and expected-wait costs are evaluated at this instant
    /// (§10), so two runs given the same value plan identically — which is
    /// what docs/01_REQUIREMENTS.md NFR-003 asks for.
    core::TimePoint current_time{core::kTimeOrigin};

    domain::Priority priority{};

    RouteConstraints constraints;

    /// Why this route is being planned. Carried into the log record of §23;
    /// on a replan (§14) this is the trigger, such as "human blockage".
    std::string reason;
};

/// Why a route could not be produced. docs/05_GLOBAL_ROUTING.md §13.
enum class RouteFailure {
    /// robot_id, start or goal was empty.
    invalid_request,

    /// The start node is not in the map.
    unknown_start,

    /// The goal node is not in the map.
    unknown_goal,

    /// The robot is standing somewhere the constraints exclude. Distinct from
    /// `no_route`: the controller cannot solve this by waiting, it has to move
    /// the robot or drop the constraint.
    blocked_start,

    /// The goal itself is excluded by the constraints.
    blocked_goal,

    /// Start and goal are both fine and no path connects them.
    ///
    /// This is §13's NO_ROUTE. The controller answers it with WAIT, RETRY,
    /// ALTERNATIVE_GOAL or TASK_FAILED — the planner does not choose.
    no_route,

    /// The search hit its expansion cap before reaching the goal.
    ///
    /// Distinct from `no_route`: a path may well exist. What is known is that
    /// the planner was not allowed to look far enough, so the answer is "I do
    /// not know" and not "there is none". Reporting it as NO_ROUTE would have
    /// the controller fail a task that was perfectly achievable.
    search_limit_reached,
};

[[nodiscard]] std::string_view to_string(RouteFailure failure) noexcept;

/// True when the failure may resolve on its own — the blockage clears, the
/// congestion drains — so retrying the same request is worth doing.
///
/// A malformed request or an unknown node will fail identically forever.
[[nodiscard]] bool is_retryable(RouteFailure failure) noexcept;

/// A planned route and what it cost. docs/05_GLOBAL_ROUTING.md §12.
struct RouteResponse {
    domain::Route route;

    /// Sum of edge lengths, in metres.
    double total_distance{0.0};

    /// Expected time from start to goal, before any waiting the traffic
    /// controller may impose.
    core::Duration estimated_time{core::Duration::zero()};

    /// The value the search minimised. Comparable only against another cost
    /// computed with the same weights.
    double cost{0.0};

    /// Which planner produced it. docs/05_GLOBAL_ROUTING.md §23 logs this, and
    /// it is how a benchmark tells A* from its successors without the caller
    /// knowing they exist (CLAUDE.md, Algorithm Isolation).
    std::string planner;

    /// How many nodes the search expanded. Not part of §12; kept because it is
    /// the one number that explains a latency outlier after the fact.
    std::size_t expanded_nodes{0};

    /// Time spent planning, measured on the injected clock.
    core::Duration planning_time{core::Duration::zero()};

    /// Set by `replan_route`: whether the alternative was adopted, and why.
    ///
    /// Empty for a first-time plan, where there was nothing to keep. When it
    /// says the route was kept, `route` is the one the robot already had.
    std::optional<ReplanDecision> replan_decision;
};

}  // namespace traffic::planning
