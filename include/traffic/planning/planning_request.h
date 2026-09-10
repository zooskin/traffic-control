#pragma once

/// \file
/// The batch planning contract. docs/24_DOMAIN_MODEL.md §20~21.
///
/// A `PlanningRequest` covers several robots at once; a `RouteRequest`
/// (route_request.h) covers one. The batch form exists now, ahead of any
/// planner that reasons about robots jointly, because it is the boundary the
/// traffic controller calls across — and because a multi-robot planner
/// (WHCA*/PIBT/ECBS, deferred past Phase 15 by decision D-003) must be able to
/// arrive behind this interface without the callers changing.
///
/// Phase 3's planner answers a batch by planning each robot independently.
/// That is a real answer, not a placeholder: docs/05_GLOBAL_ROUTING.md §19
/// separates routing from reservation in the initial version, and it is the
/// baseline every later algorithm has to beat in the benchmark of
/// docs/15_ALGORITHM_BENCHMARK.md.

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/values.h"
#include "traffic/planning/route_request.h"

namespace traffic::planning {

/// Input to a planning run. docs/24_DOMAIN_MODEL.md §20.
///
/// §20 also lists `reservations`. They are absent here on purpose:
/// docs/05_GLOBAL_ROUTING.md §19 keeps A* and reservation apart in the initial
/// version, and Phase 6 introduces reservations. Reservation-aware planning
/// supplies them through `ITrafficConditions` (cost_model.h) instead, so that
/// the planner never learns the reservation table's shape.
struct PlanningRequest {
    core::PlanningRequestId request_id;

    /// One entry per robot. Carries §20's robot_ids, start_states and goals
    /// together, which keeps them from drifting out of alignment — three
    /// parallel vectors can disagree about their lengths, one vector cannot.
    std::vector<RouteRequest> routes;

    /// The map the request was raised against.
    domain::MapVersion map_version;

    /// The fleet state version it was raised against.
    ///
    /// Both versions are checked before the result is committed
    /// (docs/23_SYSTEM_ARCHITECTURE.md §2.4): a plan computed on state 100 must
    /// not be applied to a world that has moved to 105.
    domain::StateVersion traffic_state_version;

    /// Point in traffic time after which the result is worthless.
    std::optional<core::TimePoint> deadline;

    /// How long the planner may spend before giving up.
    std::optional<core::Duration> timeout;
};

/// docs/24_DOMAIN_MODEL.md §21.
enum class PlanningStatus {
    /// Every requested route was produced.
    success,

    /// At least one robot has no path. The routes that did succeed are still
    /// returned — discarding them would make one unreachable goal stall the
    /// whole fleet.
    no_path,

    /// The deadline or timeout was reached first.
    timeout,

    /// Abandoned by the caller.
    cancelled,

    /// Something else went wrong: a malformed request, an unknown node.
    failed,
};

[[nodiscard]] std::string_view to_string(PlanningStatus status) noexcept;

/// One robot's outcome inside a batch.
struct RouteOutcome {
    core::RobotId robot_id;

    /// Set when planning succeeded.
    std::optional<RouteResponse> response;

    /// Set when it did not.
    std::optional<RouteFailure> failure;

    [[nodiscard]] bool succeeded() const noexcept { return response.has_value(); }
};

/// Output of a planning run. docs/24_DOMAIN_MODEL.md §21.
struct PlanningResult {
    core::PlanningRequestId request_id;
    PlanningStatus status{PlanningStatus::failed};

    /// One per requested robot, in request order. Deterministic: the same
    /// batch produces the same sequence.
    std::vector<RouteOutcome> outcomes;

    domain::MapVersion map_version;
    domain::StateVersion traffic_state_version;

    /// Total time spent, measured on the injected clock.
    core::Duration planning_time{core::Duration::zero()};
};

/// How many robots got a route.
[[nodiscard]] std::size_t succeeded_count(const PlanningResult& result) noexcept;

/// The routes that were produced, in request order.
[[nodiscard]] std::vector<const RouteResponse*> successful_routes(const PlanningResult& result);

/// The status a batch with these outcomes should carry.
///
/// Kept as a function rather than set at each return site so that "some
/// succeeded, some had no path" is decided in exactly one place.
[[nodiscard]] PlanningStatus status_for(const std::vector<RouteOutcome>& outcomes) noexcept;

}  // namespace traffic::planning
