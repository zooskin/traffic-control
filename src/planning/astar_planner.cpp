#include "traffic/planning/astar_planner.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "traffic/domain/graph.h"
#include "traffic/planning/route_validator.h"

namespace traffic::planning {
namespace {

using RouteResult = core::Result<RouteResponse, RouteFailure>;

/// A route segment must span a non-zero interval — `TimeWindow` requires
/// start < end. An edge short enough to round to nothing still takes *some*
/// time, and a zero-length window would read in the logs as a resource that
/// was never held.
constexpr core::Duration kSmallestSegment{1};

/// How a node was reached.
struct Predecessor {
    core::NodeId node;
    core::EdgeId edge;
};

/// An entry in the open set.
struct Candidate {
    double f{0.0};
    double g{0.0};
    core::NodeId node;
};

/// Orders the open set. `std::priority_queue` puts the *greatest* element on
/// top, so this returns true when \p lhs should be visited later than \p rhs.
///
/// The second and third comparisons are what docs/05_GLOBAL_ROUTING.md §22
/// asks for. Without them two runs of the same search can pop equally good
/// nodes in different orders and return different — equally optimal, equally
/// unhelpful — routes. Comparisons are exact: within one run the same path
/// summed the same way gives bit-identical doubles, and an epsilon here would
/// break the strict weak ordering the queue requires.
struct WorseFirst {
    [[nodiscard]] bool operator()(const Candidate& lhs, const Candidate& rhs) const noexcept {
        if (lhs.f != rhs.f) {
            return lhs.f > rhs.f;
        }
        // Equal estimates: prefer the node already further along. It is closer
        // to the goal, so it reaches one sooner.
        if (lhs.g != rhs.g) {
            return lhs.g < rhs.g;
        }
        return lhs.node > rhs.node;
    }
};

/// What one search found.
struct SearchOutcome {
    std::vector<core::NodeId> nodes;
    std::vector<core::EdgeId> edges;
    double cost{0.0};
    std::size_t expanded{0};
    bool found{false};
    bool hit_limit{false};
};

/// Walks the predecessor chain back from the goal.
///
/// The chain is finite and acyclic by construction: a node's predecessor is
/// always one that was reached at a strictly lower cost, or — on a tie — one
/// already settled.
void reconstruct(const std::unordered_map<core::NodeId, Predecessor>& came_from,
                 const core::NodeId& start,
                 const core::NodeId& goal,
                 SearchOutcome& outcome) {
    core::NodeId cursor = goal;
    while (cursor != start) {
        const auto found = came_from.find(cursor);
        if (found == came_from.end()) {
            return;
        }
        outcome.nodes.push_back(cursor);
        outcome.edges.push_back(found->second.edge);
        cursor = found->second.node;
    }
    outcome.nodes.push_back(start);

    std::reverse(outcome.nodes.begin(), outcome.nodes.end());
    std::reverse(outcome.edges.begin(), outcome.edges.end());
}

/// Everything one search needs, so the relaxation step can be its own
/// function without a parameter list nobody can read.
struct SearchContext {
    const map::Map& map;
    const IHeuristic& heuristic;
    const ITrafficConditions& conditions;
    const CostWeights& weights;
    const RouteConstraints& constraints;
    core::TimePoint at;
    const domain::Position& goal_position;
};

/// Considers one step out of \p from along \p edge_id.
void relax(const SearchContext& context,
           const core::NodeId& from,
           double from_cost,
           const core::EdgeId& edge_id,
           std::unordered_map<core::NodeId, double>& best_cost,
           std::unordered_map<core::NodeId, Predecessor>& came_from,
           const std::unordered_set<core::NodeId>& settled,
           std::priority_queue<Candidate, std::vector<Candidate>, WorseFirst>& open) {
    const domain::Edge* edge = context.map.find_edge(edge_id);
    if (edge == nullptr) {
        return;
    }

    const core::ResourceId resource = context.map.resource_for_edge(edge_id);
    if (!allows_edge(context.constraints, edge_id, resource)) {
        return;
    }

    const std::optional<core::NodeId> next = domain::opposite_node(*edge, from);
    if (!next.has_value() || settled.contains(*next) || !allows_node(context.constraints, *next)) {
        return;
    }

    const domain::Node* next_node = context.map.find_node(*next);
    if (next_node == nullptr) {
        return;
    }

    const double step = edge_cost(
        *edge, resource, context.weights, context.constraints, context.conditions, context.at);
    const double cost = from_cost + step;

    const auto existing = best_cost.find(*next);
    if (existing != best_cost.end()) {
        const bool cheaper = cost < existing->second;
        // Equal cost: settle it on edge id, which docs/05_GLOBAL_ROUTING.md
        // §22 names first. Without this the answer depends on which of two
        // equally good paths happened to be relaxed first.
        const bool same_cost_lower_edge =
            cost == existing->second && edge_id < came_from.at(*next).edge;
        if (!cheaper && !same_cost_lower_edge) {
            return;
        }
    }

    best_cost[*next] = cost;
    came_from[*next] = Predecessor{from, edge_id};
    open.push(
        Candidate{cost + context.heuristic.estimate(next_node->position, context.goal_position),
                  cost,
                  *next});
}

/// The search itself.
SearchOutcome search(const SearchContext& context,
                     const core::NodeId& start,
                     const core::NodeId& goal,
                     std::size_t max_expansions) {
    SearchOutcome outcome;

    // Hashed, and never iterated — only ever looked up by key. Iteration order
    // is what docs/20_CODING_GUIDELINES.md §23 rules out, and none of these is
    // walked. The open set, which does decide order, is explicitly ordered.
    std::unordered_map<core::NodeId, double> best_cost;
    std::unordered_map<core::NodeId, Predecessor> came_from;
    std::unordered_set<core::NodeId> settled;

    std::priority_queue<Candidate, std::vector<Candidate>, WorseFirst> open;

    const domain::Node* start_node = context.map.find_node(start);
    if (start_node == nullptr) {
        return outcome;
    }

    best_cost[start] = 0.0;
    open.push(Candidate{
        context.heuristic.estimate(start_node->position, context.goal_position), 0.0, start});

    while (!open.empty()) {
        const Candidate current = open.top();
        open.pop();

        // Lazy deletion: a node can sit in the queue more than once, and only
        // the cheapest entry is worth anything.
        if (settled.contains(current.node)) {
            continue;
        }
        settled.insert(current.node);
        ++outcome.expanded;

        if (current.node == goal) {
            outcome.found = true;
            outcome.cost = current.g;
            reconstruct(came_from, start, goal, outcome);
            return outcome;
        }

        if (max_expansions != 0 && outcome.expanded >= max_expansions) {
            outcome.hit_limit = true;
            return outcome;
        }

        for (const core::EdgeId& edge_id : context.map.traversable_edges(current.node)) {
            relax(context, current.node, current.g, edge_id, best_cost, came_from, settled, open);
        }
    }

    return outcome;
}

/// Turns a node-and-edge walk into a Route with timing.
///
/// The expected windows are what make temporal conflict detection possible at
/// all (docs/24_DOMAIN_MODEL.md §12): two routes over the same resource at
/// disjoint times do not conflict, and without the timing they would look as
/// though they did.
core::Result<domain::Route, domain::DomainError> assemble(const map::Map& map,
                                                          const RouteRequest& request,
                                                          const SearchOutcome& outcome,
                                                          core::RouteId route_id,
                                                          core::TimePoint created_at,
                                                          double& total_distance,
                                                          core::Duration& estimated_time) {
    std::vector<domain::RouteSegment> segments;
    segments.reserve(outcome.edges.size());

    core::TimePoint cursor = request.current_time;
    total_distance = 0.0;

    for (std::size_t i = 0; i < outcome.edges.size(); ++i) {
        const domain::Edge* edge = map.find_edge(outcome.edges[i]);
        const core::Duration travel =
            edge == nullptr ? kSmallestSegment
                            : std::max(domain::nominal_travel_time(*edge), kSmallestSegment);

        domain::RouteSegment segment;
        segment.edge_id = outcome.edges[i];
        segment.from_node = outcome.nodes[i];
        segment.to_node = outcome.nodes[i + 1];
        segment.sequence = i;
        segment.expected_window = domain::TimeWindow{cursor, cursor + travel};
        segments.push_back(std::move(segment));

        cursor += travel;
        if (edge != nullptr) {
            total_distance += edge->length;
        }
    }

    estimated_time = cursor - request.current_time;

    return domain::make_route(std::move(route_id),
                              request.robot_id,
                              outcome.nodes,
                              std::move(segments),
                              map.version(),
                              created_at);
}

}  // namespace

AStarPlanner::AStarPlanner(const map::Map& map,
                           const core::IClock& clock,
                           const IHeuristic& heuristic,
                           const ITrafficConditions& conditions,
                           AStarConfig config)
    : map_(map), clock_(clock), heuristic_(heuristic), conditions_(conditions), config_(config) {}

std::string_view AStarPlanner::planner_name() const noexcept {
    // A* with a zero heuristic is Dijkstra, and the log record of §23 should
    // say which one actually ran rather than which class was instantiated.
    return heuristic_.name() == "dijkstra" ? "dijkstra" : "astar";
}

std::string_view AStarPlanner::name() const noexcept {
    return planner_name();
}

core::Result<RouteResponse, RouteFailure> AStarPlanner::plan_route(const RouteRequest& request) {
    const core::TimePoint started = clock_.now();

    if (request.robot_id.empty() || request.start_node.empty() || request.goal_node.empty()) {
        return RouteResult::failure(RouteFailure::invalid_request);
    }

    const domain::Node* start_node = map_.find_node(request.start_node);
    if (start_node == nullptr) {
        return RouteResult::failure(RouteFailure::unknown_start);
    }
    const domain::Node* goal_node = map_.find_node(request.goal_node);
    if (goal_node == nullptr) {
        return RouteResult::failure(RouteFailure::unknown_goal);
    }

    // Reported apart from `no_route` because the answers differ: a blocked
    // goal may be waited out, a robot standing on a blocked node has to be
    // moved first.
    if (!allows_node(request.constraints, request.start_node)) {
        return RouteResult::failure(RouteFailure::blocked_start);
    }
    if (!allows_node(request.constraints, request.goal_node)) {
        return RouteResult::failure(RouteFailure::blocked_goal);
    }

    const SearchContext context{map_,
                                heuristic_,
                                conditions_,
                                config_.weights,
                                request.constraints,
                                request.current_time,
                                goal_node->position};

    SearchOutcome outcome;
    if (request.start_node == request.goal_node) {
        // Already there. A route with one node and no segments is a valid
        // answer, not an edge case: the controller needs something to hold
        // while the robot waits at its destination.
        outcome.found = true;
        outcome.nodes = {request.start_node};
        outcome.expanded = 1;
    } else {
        outcome = search(context, request.start_node, request.goal_node, config_.max_expansions);
    }

    if (!outcome.found) {
        return RouteResult::failure(outcome.hit_limit ? RouteFailure::search_limit_reached
                                                      : RouteFailure::no_route);
    }

    ++route_sequence_;
    core::RouteId route_id{"ROUTE-" + request.robot_id.value() + "-" +
                           std::to_string(route_sequence_)};

    RouteResponse response;
    auto route = assemble(map_,
                          request,
                          outcome,
                          std::move(route_id),
                          started,
                          response.total_distance,
                          response.estimated_time);
    if (!route.has_value()) {
        // Unreachable: the search produces a connected walk by construction,
        // which is exactly what make_route checks. Reported rather than
        // asserted — a planner that aborts takes the fleet with it.
        return RouteResult::failure(RouteFailure::no_route);
    }

    response.route = std::move(route).value();
    response.cost = outcome.cost;
    response.expanded_nodes = outcome.expanded;
    response.planner = planner_name();
    response.planning_time = clock_.now() - started;
    return RouteResult::success(std::move(response));
}

core::Result<RouteResponse, RouteFailure> AStarPlanner::replan_route(
    const RouteRequest& request, const domain::Route& current_route) {
    auto candidate = plan_route(request);
    if (!candidate.has_value()) {
        // No alternative exists. Whether to wait, retry or fail the task is
        // the controller's call (§13), not the planner's.
        return candidate;
    }

    const bool current_valid = is_route_valid(current_route, request.constraints);
    const double current_cost =
        estimate_route_cost(current_route, request.constraints, request.current_time);
    const core::Duration held_for = route_age(current_route, request.current_time);

    const ReplanDecision decision = decide_replan(
        config_.stability, current_cost, candidate.value().cost, current_valid, held_for);

    if (is_adopted(decision)) {
        RouteResponse adopted = std::move(candidate).value();
        adopted.replan_decision = decision;
        return RouteResult::success(std::move(adopted));
    }

    // Kept. The response carries the route the robot already has, so the
    // caller can apply the answer without having to know which way it went.
    RouteResponse kept;
    kept.route = current_route;
    kept.cost = current_cost;
    kept.planner = planner_name();
    kept.replan_decision = decision;
    return RouteResult::success(std::move(kept));
}

double AStarPlanner::estimate_route_cost(const domain::Route& route,
                                         const RouteConstraints& constraints,
                                         core::TimePoint at) const {
    double total = 0.0;
    for (const domain::RouteSegment& segment : route.segments) {
        const domain::Edge* edge = map_.find_edge(segment.edge_id);
        if (edge == nullptr) {
            // The route uses an edge this map does not have. Costing it as
            // finite would let an undrivable route win a replan comparison.
            return std::numeric_limits<double>::infinity();
        }
        total += edge_cost(*edge,
                           map_.resource_for_edge(segment.edge_id),
                           config_.weights,
                           constraints,
                           conditions_,
                           at);
    }
    return total;
}

bool AStarPlanner::is_route_valid(const domain::Route& route,
                                  const RouteConstraints& constraints) const {
    return planning::is_route_valid(map_, route, constraints);
}

}  // namespace traffic::planning
