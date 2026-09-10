#pragma once

/// \file
/// A* heuristics. docs/05_GLOBAL_ROUTING.md §5.
///
/// §5 offers Manhattan or Euclidean distance "according to the map", and in the
/// same breath requires the heuristic to be admissible. On this project's maps
/// those two sentences pull in different directions, and admissibility wins.
///
/// A heuristic is admissible when it never overestimates the remaining cost. On
/// a grid where robots move only along the axes, Manhattan distance is exactly
/// the distance travelled and is both admissible and sharper than Euclidean. On
/// a general graph with edges laid out at arbitrary angles — which is what
/// docs/03_MAP_GRAPH.md describes and what `reference_map.json` is — Manhattan
/// exceeds the straight-line distance on every diagonal, and A* stops returning
/// the cheapest route. It does not fail; it quietly returns something else.
///
/// So `EuclideanHeuristic` is the default, `ManhattanHeuristic` stays available
/// for maps that really are grids, and the decision is recorded as D-008 in
/// docs/00_INDEX.md.
///
/// Distance is not cost. A heuristic has to be in the same units as `g`, so
/// each one is scaled by the least a metre can cost under the active weights
/// (`min_cost_per_metre`, cost_model.h). Get that scale wrong upwards and
/// admissibility goes with it — `make_admissible_heuristic` computes it from
/// the map so callers do not have to.

#include <string_view>

#include "traffic/domain/values.h"
#include "traffic/map/map.h"
#include "traffic/planning/cost_model.h"

namespace traffic::planning {

/// Straight-line distance, in metres.
[[nodiscard]] double euclidean_distance(const domain::Position& from,
                                        const domain::Position& to) noexcept;

/// Axis-aligned distance, in metres. Never less than the Euclidean distance.
[[nodiscard]] double manhattan_distance(const domain::Position& from,
                                        const domain::Position& to) noexcept;

/// An estimate of the cost still to be paid.
class IHeuristic {
public:
    IHeuristic() = default;
    virtual ~IHeuristic() = default;

    IHeuristic(const IHeuristic&) = delete;
    IHeuristic& operator=(const IHeuristic&) = delete;
    IHeuristic(IHeuristic&&) = delete;
    IHeuristic& operator=(IHeuristic&&) = delete;

    /// Estimated cost from \p from to \p goal. Must never exceed the true
    /// remaining cost, and must be zero when the two coincide.
    [[nodiscard]] virtual double estimate(const domain::Position& from,
                                          const domain::Position& goal) const = 0;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

/// Estimates nothing, which turns A* into Dijkstra.
///
/// docs/05_GLOBAL_ROUTING.md §3 names Dijkstra as the fallback, and this is it:
/// there is no separate algorithm, because A* with a zero heuristic *is*
/// Dijkstra. One implementation to keep correct instead of two, and the
/// equivalence is what the A* tests check their answers against.
class ZeroHeuristic final : public IHeuristic {
public:
    [[nodiscard]] double estimate(const domain::Position& from,
                                  const domain::Position& goal) const override;
    [[nodiscard]] std::string_view name() const noexcept override;
};

/// Straight-line distance, scaled into cost. The default.
class EuclideanHeuristic final : public IHeuristic {
public:
    /// \p cost_per_metre must be a lower bound on what a metre can cost — see
    /// `min_cost_per_metre`. Passing anything larger makes A* return routes
    /// that are not the cheapest.
    explicit EuclideanHeuristic(double cost_per_metre) noexcept;

    [[nodiscard]] double estimate(const domain::Position& from,
                                  const domain::Position& goal) const override;
    [[nodiscard]] std::string_view name() const noexcept override;

private:
    double cost_per_metre_;
};

/// Axis-aligned distance, scaled into cost.
///
/// Admissible **only** on a map whose every edge runs along an axis. On any
/// other map it overestimates and A* loses optimality. Use it when the site
/// really is a grid and the sharper estimate is worth having.
class ManhattanHeuristic final : public IHeuristic {
public:
    explicit ManhattanHeuristic(double cost_per_metre) noexcept;

    [[nodiscard]] double estimate(const domain::Position& from,
                                  const domain::Position& goal) const override;
    [[nodiscard]] std::string_view name() const noexcept override;

private:
    double cost_per_metre_;
};

/// The fastest edge in \p map, in metres per second. 0 for a map with no
/// usable edges.
[[nodiscard]] double max_speed_limit(const map::Map& map) noexcept;

/// A Euclidean heuristic scaled correctly for \p map and \p weights.
///
/// The one call that cannot get the scale wrong. Prefer it to constructing the
/// heuristic directly.
[[nodiscard]] EuclideanHeuristic make_admissible_heuristic(const map::Map& map,
                                                           const CostWeights& weights);

/// True when every edge in \p map is at least as long as the straight line
/// between its endpoints.
///
/// This is the assumption a distance-based heuristic rests on and nothing in
/// the map format enforces it: an edge may name a length shorter than the gap
/// between the coordinates of its ends, and then the straight-line estimate
/// exceeds the real cost. Geometrically impossible, trivially typed into a
/// JSON file.
///
/// Not a map validation error — a site may deliberately use nominal
/// coordinates and carry the real distances in `length`. It is a precondition
/// for the *heuristic*, so it is checked here, and a map that fails it should
/// be planned with `ZeroHeuristic`.
[[nodiscard]] bool geometry_supports_distance_heuristic(const map::Map& map);

}  // namespace traffic::planning
