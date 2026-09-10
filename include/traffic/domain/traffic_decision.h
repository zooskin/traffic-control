#pragma once

/// \file
/// Traffic decision. docs/24_DOMAIN_MODEL.md §19.
///
/// The controller's output, and the answer to the only question it owns: may
/// this robot proceed right now (docs/00_MASTER_PLAN.md §4.1)?
///
/// Every decision carries the state version it was made against and a reason.
/// The reason is not decoration — docs/01_REQUIREMENTS.md NFR-004 makes traffic
/// decisions traceable by requirement, and "R07 waited 40 seconds" is only
/// answerable if the decision recorded why.

#include <optional>
#include <string>
#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/values.h"

namespace traffic::domain {

/// What the robot is being told to do. docs/24_DOMAIN_MODEL.md §19.
enum class TrafficAction {
    /// Proceed along the committed route.
    go,

    /// Hold position until told otherwise. Its resources are kept — this is a
    /// pause in traffic, not a release.
    wait,

    /// Stop. Stronger than `wait`, used when continuing is unsafe.
    ///
    /// Note this is a *traffic* stop. Emergency and protective stops belong to
    /// the safety controller and never originate here
    /// (docs/23_SYSTEM_ARCHITECTURE.md §2.3).
    stop,

    /// The current route is no longer usable; a new one is being computed.
    replan,

    /// Move aside to a holding area so others can pass. A deadlock recovery
    /// strategy — docs/22_IMPLEMENTATION_WORKFLOW.md Phase 10.
    hold,

    /// Take the recovery path for a failed or lost robot.
    recover,
};

[[nodiscard]] std::string_view to_string(TrafficAction action) noexcept;
[[nodiscard]] std::optional<TrafficAction> traffic_action_from_string(
    std::string_view name) noexcept;

/// True when the action leaves the robot stationary.
[[nodiscard]] bool is_halting(TrafficAction action) noexcept;

/// One instruction for one robot.
struct TrafficDecision {
    core::TrafficDecisionId id;
    core::RobotId robot_id;

    TrafficAction action{TrafficAction::wait};

    /// The route this decision applies to, when it applies to one.
    std::optional<core::RouteId> route_id;

    /// The reservation that justifies a `go`, when one does.
    std::optional<core::ReservationId> reservation_id;

    /// Why. Short and stable enough to aggregate over — "NO_CONFLICT",
    /// "CORRIDOR_OCCUPIED", "LOWER_PRIORITY", "HUMAN_BLOCKAGE".
    std::string reason;

    core::TimePoint created_at{core::kTimeOrigin};

    /// The state this decision was computed against. Checked before the
    /// decision is issued — docs/23_SYSTEM_ARCHITECTURE.md §16.
    StateVersion state_version;

    /// Ties the decision back to the event that caused it.
    core::CorrelationId correlation_id;

    [[nodiscard]] friend bool operator==(const TrafficDecision& lhs,
                                         const TrafficDecision& rhs) noexcept {
        return lhs.id == rhs.id;
    }
};

/// True when the decision was computed against an older world than \p current.
[[nodiscard]] bool is_stale(const TrafficDecision& decision, StateVersion current) noexcept;

/// Builds a TrafficDecision.
///
/// Rejects empty ids and an empty reason: a decision nobody can explain
/// afterwards defeats NFR-004, and "the reason field was blank" is not an
/// answer to why a robot waited.
///
/// It does *not* check that a `go` carries a reservation. That pairing needs
/// the reservation table to verify, which the domain has no access to; the
/// controller enforces it in Phase 11.
[[nodiscard]] core::Result<TrafficDecision, DomainError> make_traffic_decision(
    core::TrafficDecisionId id,
    core::RobotId robot_id,
    TrafficAction action,
    std::string reason,
    core::TimePoint created_at,
    StateVersion state_version);

}  // namespace traffic::domain
