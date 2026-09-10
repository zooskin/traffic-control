#include "traffic/planning/heuristic.h"

#include <algorithm>
#include <cmath>

namespace traffic::planning {
namespace {

/// Slack for the geometry check. Map coordinates are written by hand and a
/// length that agrees with them to the millimetre is not the point; a length
/// that is *materially* shorter than the straight line is.
constexpr double kGeometryTolerance = 1e-6;

}  // namespace

double euclidean_distance(const domain::Position& from, const domain::Position& to) noexcept {
    // Kept as a name of its own because docs/05_GLOBAL_ROUTING.md §5 pairs it
    // with the Manhattan variant below, but there is one implementation of the
    // maths and it lives with Position.
    return domain::distance(from, to);
}

double manhattan_distance(const domain::Position& from, const domain::Position& to) noexcept {
    return std::abs(to.x - from.x) + std::abs(to.y - from.y) + std::abs(to.z - from.z);
}

double ZeroHeuristic::estimate(const domain::Position& /*from*/,
                               const domain::Position& /*goal*/) const {
    return 0.0;
}

std::string_view ZeroHeuristic::name() const noexcept {
    // Named for what it makes A* behave as, because that is what appears in
    // the log record of docs/05_GLOBAL_ROUTING.md §23.
    return "dijkstra";
}

EuclideanHeuristic::EuclideanHeuristic(double cost_per_metre) noexcept
    : cost_per_metre_(cost_per_metre) {}

double EuclideanHeuristic::estimate(const domain::Position& from,
                                    const domain::Position& goal) const {
    return cost_per_metre_ * euclidean_distance(from, goal);
}

std::string_view EuclideanHeuristic::name() const noexcept {
    return "euclidean";
}

ManhattanHeuristic::ManhattanHeuristic(double cost_per_metre) noexcept
    : cost_per_metre_(cost_per_metre) {}

double ManhattanHeuristic::estimate(const domain::Position& from,
                                    const domain::Position& goal) const {
    return cost_per_metre_ * manhattan_distance(from, goal);
}

std::string_view ManhattanHeuristic::name() const noexcept {
    return "manhattan";
}

double max_speed_limit(const map::Map& map) noexcept {
    double fastest = 0.0;
    for (const domain::Edge& edge : map.edges()) {
        // A disabled edge cannot be used, but it can be re-enabled without a
        // new Map, and a heuristic that stopped being admissible the moment a
        // corridor reopened would be a subtle way to lose optimality.
        fastest = std::max(fastest, edge.speed_limit);
    }
    return fastest;
}

EuclideanHeuristic make_admissible_heuristic(const map::Map& map, const CostWeights& weights) {
    return EuclideanHeuristic{min_cost_per_metre(weights, max_speed_limit(map))};
}

bool geometry_supports_distance_heuristic(const map::Map& map) {
    for (const domain::Edge& edge : map.edges()) {
        const domain::Node* from = map.find_node(edge.from_node);
        const domain::Node* to = map.find_node(edge.to_node);
        if (from == nullptr || to == nullptr) {
            // A dangling endpoint is a map error, caught by map validation.
            // Here it simply means the geometry cannot be checked.
            return false;
        }
        if (edge.length + kGeometryTolerance < euclidean_distance(from->position, to->position)) {
            return false;
        }
    }
    return true;
}

}  // namespace traffic::planning
