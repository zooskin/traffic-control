#pragma once

/// \file
/// Reservation lifecycle.
///
/// States are those of docs/24_DOMAIN_MODEL.md §13.
///
/// docs/24_DOMAIN_MODEL.md §30 states the invariant: a RELEASED reservation is
/// never made ACTIVE again. Once a robot has given a corridor back, another
/// robot may already be inside it — reviving the old grant would hand the same
/// resource to two owners, which is the collision this whole subsystem exists
/// to prevent.
///
/// Reservation *semantics* are on the do-not-change list in CLAUDE.md. This
/// file is the lifecycle only; who may hold what, and the conflict rules, are
/// docs/06_TRAFFIC_RESERVATION.md and arrive in Phase 6.

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace traffic::domain {

enum class ReservationState {
    /// Requested, not yet granted. The robot must not enter the resource.
    pending,

    /// Granted and held. The robot owns the resource for its window.
    active,

    /// Given back by the holder. Terminal.
    released,

    /// Its window elapsed before it was released. Terminal.
    ///
    /// Distinct from `released` on purpose: an expiry means a robot held a
    /// resource longer than it promised, which is a signal worth counting
    /// rather than an ordinary handover.
    expired,

    /// Withdrawn before it was ever held. Terminal.
    cancelled,
};

inline constexpr std::size_t kReservationStateCount = 5;

[[nodiscard]] std::span<const ReservationState> reservation_states() noexcept;

/// True when \p from -> \p to is a legal move.
[[nodiscard]] bool is_transition_allowed(ReservationState from, ReservationState to) noexcept;

/// True when the reservation will never change state again.
[[nodiscard]] bool is_terminal(ReservationState state) noexcept;

/// True when the reservation currently withholds its resource from others.
///
/// `pending` counts: a request that has been accepted for consideration must
/// still block conflicting grants, or two robots can both be told to proceed.
[[nodiscard]] bool holds_resource(ReservationState state) noexcept;

[[nodiscard]] std::string_view to_string(ReservationState state) noexcept;
[[nodiscard]] std::optional<ReservationState> reservation_state_from_string(
    std::string_view name) noexcept;

}  // namespace traffic::domain
