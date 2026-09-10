#pragma once

/// \file
/// Reservation and the resources reservations are taken on.
/// docs/24_DOMAIN_MODEL.md §13~14, docs/06_TRAFFIC_RESERVATION.md.
///
/// A reservation is a claim by one robot on one resource for one window of
/// time. Whether two claims may coexist is `conflicts_with` below, and it is
/// the single rule the whole safety argument rests on: if it says two
/// reservations do not conflict, both robots are told to proceed.
///
/// Reservation semantics are on the do-not-change list in CLAUDE.md. Change the
/// specification first.

#include <cstdint>
#include <optional>
#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/reservation_state.h"
#include "traffic/domain/values.h"

namespace traffic::domain {

/// What kind of thing is being reserved. docs/24_DOMAIN_MODEL.md §14.
enum class ResourceType {
    node,
    edge,

    /// A narrow run of edges treated as one indivisible resource. The reason
    /// the project models resources at all — docs/00_MASTER_PLAN.md §4.2.
    corridor,

    intersection,
    charging_area,
    loading_area,
};

[[nodiscard]] std::string_view to_string(ResourceType type) noexcept;
[[nodiscard]] std::optional<ResourceType> resource_type_from_string(std::string_view name) noexcept;

/// Something robots compete for.
struct Resource {
    core::ResourceId id;
    ResourceType type{ResourceType::edge};

    /// How many robots may hold it at once. One for a single-lane corridor.
    std::uint32_t capacity{1};

    [[nodiscard]] friend bool operator==(const Resource& lhs, const Resource& rhs) noexcept {
        return lhs.id == rhs.id;
    }
};

/// One robot's claim on one resource for one window.
struct Reservation {
    core::ReservationId id;
    core::RobotId robot_id;
    core::ResourceId resource_id;

    TimeWindow window;

    ReservationState state{ReservationState::pending};

    /// Used to decide who wins when two robots want the same resource.
    Priority priority;

    core::TimePoint requested_at{core::kTimeOrigin};

    [[nodiscard]] friend bool operator==(const Reservation& lhs, const Reservation& rhs) noexcept {
        return lhs.id == rhs.id;
    }
};

/// True when two reservations cannot both be honoured.
///
/// They conflict when all three hold:
///   * they name the same resource;
///   * both still withhold it (`holds_resource`);
///   * their windows overlap.
///
/// Two claims by the *same* robot never conflict — a robot does not block
/// itself, and treating it as if it did produces a deadlock with one
/// participant.
///
/// This is a pairwise test. It does not know a resource's capacity, so a
/// resource that admits more than one robot needs the caller to count
/// overlapping holders rather than reject the first pair. That counting is the
/// ReservationTable's job in Phase 6.
[[nodiscard]] bool conflicts_with(const Reservation& lhs, const Reservation& rhs) noexcept;

/// True when the reservation's window has ended at \p now.
///
/// Being past its window is not the same as being in the `expired` state:
/// something has to notice and record the expiry. This is the test that
/// noticing uses.
[[nodiscard]] bool is_past_window(const Reservation& reservation, core::TimePoint now) noexcept;

/// Moves a reservation to \p next, or reports `invalid_transition`.
/// The original is left untouched.
[[nodiscard]] core::Result<Reservation, DomainError> with_state(const Reservation& reservation,
                                                                ReservationState next);

/// Builds a Reservation, checking the invariants of docs/24_DOMAIN_MODEL.md §34.
///
/// The window must be non-empty; `make_time_window` enforces that.
[[nodiscard]] core::Result<Reservation, DomainError> make_reservation(core::ReservationId id,
                                                                      core::RobotId robot_id,
                                                                      core::ResourceId resource_id,
                                                                      TimeWindow window,
                                                                      Priority priority,
                                                                      core::TimePoint requested_at);

}  // namespace traffic::domain
