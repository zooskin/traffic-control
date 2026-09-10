#pragma once

/// \file
/// What a robot asks for, and what it is told.
/// docs/06_TRAFFIC_RESERVATION.md §5, §10, §11, §14, §15, §36.
///
/// Two things here are not in §5's field list and are needed anyway.
///
/// **The reservation id is supplied by the caller.** §35 requires that a
/// repeated request not create a second reservation, and names a request id as
/// the mechanism. Making that id *be* the reservation id means idempotency is
/// enforced by the primary key rather than by a second table that can disagree
/// with it, and it keeps an id generator — a thing that would have to be seeded
/// and would then be one more source of run-to-run variation — out of the
/// manager entirely.
///
/// **`ResourceUsage` says how the resource will be used.** A time window and a
/// resource id are enough for capacity, and not enough for §10 and §11: two
/// robots entering one corridor from opposite ends contend even where capacity
/// would admit both, and two robots crossing an intersection contend only when
/// their movements share a conflict group. Neither fact is visible in a window.

#include <optional>
#include <string_view>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/resources.h"
#include "traffic/domain/values.h"

namespace traffic::reservation {

/// Which way a robot runs through a corridor, relative to `entry_node` ->
/// `exit_node`. docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §10.
enum class TravelDirection {
    /// entry_node -> exit_node.
    forward,

    /// exit_node -> entry_node.
    reverse,
};

[[nodiscard]] std::string_view to_string(TravelDirection direction) noexcept;

/// The direction implied by entering \p corridor at \p entry.
/// Empty when \p entry is not one of the corridor's ends.
[[nodiscard]] std::optional<TravelDirection> direction_from_entry(const domain::Corridor& corridor,
                                                                  const core::NodeId& entry);

/// How the robot intends to use the resource.
///
/// Both fields are optional because both are unknowable for some resources: a
/// charging area has no direction and a corridor has no movements. An absent
/// field means "not stated", and an unstated fact cannot refuse anything —
/// capacity still applies, and for the resources where this matters most
/// (single-lane corridors, intersections) capacity is one.
struct ResourceUsage {
    /// Set for a corridor. Two holders travelling opposite ways are refused
    /// whatever the capacity — docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §7~8
    /// makes a bidirectional corridor hold one direction at a time.
    std::optional<TravelDirection> direction;

    /// Set for an intersection. docs/06_TRAFFIC_RESERVATION.md §11.
    std::optional<core::MovementId> movement;

    [[nodiscard]] friend bool operator==(const ResourceUsage&, const ResourceUsage&) = default;
};

/// docs/06_TRAFFIC_RESERVATION.md §5.
struct ReservationRequest {
    /// Names the reservation this request is for, and is the idempotency key
    /// of §35. Re-sending a request with the same id does not create a second
    /// reservation.
    core::ReservationId reservation_id;

    core::RobotId robot_id;
    core::ResourceId resource_id;

    /// When the robot expects to need the resource. May be in the past for a
    /// robot that is already there.
    core::TimePoint requested_start{core::kTimeOrigin};

    /// How long it expects to hold it. Buffers are added on top — §13.
    core::Duration estimated_duration{core::Duration::zero()};

    /// The robot's base priority. Waiting time is added by the manager, which
    /// is the only party that knows how long the robot has been asking.
    domain::Priority priority;

    ResourceUsage usage;

    /// The route this claim belongs to. §5, and §27: a route and its
    /// reservations are separate structures, so this is a reference and not a
    /// copy of anything.
    std::optional<core::RouteId> route_id;

    /// Used only to break ties deterministically —
    /// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §16.
    std::optional<core::TaskId> task_id;
};

/// docs/06_TRAFFIC_RESERVATION.md §14.
enum class DecisionStatus {
    /// The robot owns the resource for the window in the decision.
    granted,

    /// Someone else has it. The robot must not enter; it may ask again.
    wait,

    /// The request cannot succeed as stated. Asking again unchanged will fail
    /// again — held apart from `wait` for exactly that reason.
    rejected,
};

[[nodiscard]] std::string_view to_string(DecisionStatus status) noexcept;

/// Why a request was not granted. The `reason` field of §36's decision log.
enum class DenialReason {
    /// Granted. Present so the field always has a defined value.
    none,

    /// Capacity is already taken for the requested window. §7~9.
    resource_occupied,

    /// A robot is already holding the corridor the other way. §10,
    /// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §7~8.
    opposing_direction,

    /// Another robot holds a movement in the same conflict group. §11.
    movement_conflict,

    /// An identifier was empty.
    empty_id,

    /// The requested span is empty or inverted once buffers are applied.
    invalid_window,

    /// The start is further ahead than the rolling horizon allows. §28~29.
    beyond_horizon,

    /// The reservation id names a reservation that has already ended.
    /// docs/24_DOMAIN_MODEL.md §30 — a released claim is never revived.
    already_terminal,

    /// The reservation id is in use for a different robot or resource.
    duplicate_request,
};

[[nodiscard]] std::string_view to_string(DenialReason reason) noexcept;

/// docs/06_TRAFFIC_RESERVATION.md §14~15, and the record §36 requires.
///
/// Carries `start_time` and `end_time` as two points rather than a TimeWindow
/// so that a rejected decision — which has no valid window at all — still has
/// a defined, printable value in every field. §14's ReservationResult lists the
/// two times separately for the same reason.
struct ReservationDecision {
    DecisionStatus status{DecisionStatus::rejected};

    core::ReservationId reservation_id;
    core::RobotId robot_id;
    core::ResourceId resource_id;

    /// The granted window when granted; the window that was asked for
    /// otherwise. Both include the buffers of §13.
    core::TimePoint window_start{core::kTimeOrigin};
    core::TimePoint window_end{core::kTimeOrigin};

    DenialReason reason{DenialReason::none};

    /// When the resource is expected to free up. §15. A hint for scheduling a
    /// retry, not a promise: a holder may extend (§18) or release early.
    std::optional<core::TimePoint> next_available;

    /// Who is in the way, in id order. The `owner` field of §36.
    std::vector<core::RobotId> blocking_robots;

    /// The priority the decision was actually taken with, waiting time
    /// included. §17.
    domain::Priority effective_priority;

    /// True when this decision restates one already taken for the same
    /// reservation id. §35.
    bool replayed{false};

    core::TimePoint decided_at{core::kTimeOrigin};
};

/// True when the robot may proceed on this decision.
[[nodiscard]] bool is_granted(const ReservationDecision& decision) noexcept;

}  // namespace traffic::reservation
