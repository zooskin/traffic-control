#include "traffic/domain/reservation_state.h"

#include <array>
#include <cstddef>

namespace traffic::domain {
namespace {

constexpr std::array<ReservationState, kReservationStateCount> kAllStates{
    ReservationState::pending,
    ReservationState::active,
    ReservationState::released,
    ReservationState::expired,
    ReservationState::cancelled,
};

[[nodiscard]] constexpr std::size_t index_of(ReservationState state) noexcept {
    return static_cast<std::size_t>(state);
}

/// Rows are the current state, columns the requested one, in declaration
/// order.
///
///   * `pending` may be granted, withdrawn, or time out before it is granted.
///   * `active` may only be released or expire. It cannot be cancelled: the
///     robot is already inside the resource, and pretending the grant never
///     happened would leave nothing recording that it is occupied.
///   * The three terminal rows are false everywhere except their diagonal.
///     In particular `released -> active` is false, which is the invariant of
///     docs/24_DOMAIN_MODEL.md §30.
constexpr std::array<std::array<bool, kReservationStateCount>, kReservationStateCount> kTransitions{
    {
        // to:       pending active released expired cancelled
        /* pending   */ {{true, true, false, true, true}},
        /* active    */ {{false, true, true, true, false}},
        /* released  */ {{false, false, true, false, false}},
        /* expired   */ {{false, false, false, true, false}},
        /* cancelled */ {{false, false, false, false, true}},
    }};

static_assert(kAllStates.size() == kReservationStateCount);
static_assert(index_of(ReservationState::pending) == 0);
static_assert(index_of(ReservationState::active) == 1);
static_assert(index_of(ReservationState::released) == 2);
static_assert(index_of(ReservationState::expired) == 3);
static_assert(index_of(ReservationState::cancelled) == 4);

}  // namespace

std::span<const ReservationState> reservation_states() noexcept {
    return kAllStates;
}

bool is_transition_allowed(ReservationState from, ReservationState to) noexcept {
    return kTransitions[index_of(from)][index_of(to)];
}

bool is_terminal(ReservationState state) noexcept {
    switch (state) {
        case ReservationState::released:
        case ReservationState::expired:
        case ReservationState::cancelled:
            return true;
        case ReservationState::pending:
        case ReservationState::active:
            return false;
    }
    return false;
}

bool holds_resource(ReservationState state) noexcept {
    switch (state) {
        case ReservationState::pending:
        case ReservationState::active:
            return true;
        case ReservationState::released:
        case ReservationState::expired:
        case ReservationState::cancelled:
            return false;
    }
    return false;
}

std::string_view to_string(ReservationState state) noexcept {
    switch (state) {
        case ReservationState::pending:
            return "PENDING";
        case ReservationState::active:
            return "ACTIVE";
        case ReservationState::released:
            return "RELEASED";
        case ReservationState::expired:
            return "EXPIRED";
        case ReservationState::cancelled:
            return "CANCELLED";
    }
    return "PENDING";
}

std::optional<ReservationState> reservation_state_from_string(std::string_view name) noexcept {
    for (const ReservationState state : kAllStates) {
        if (to_string(state) == name) {
            return state;
        }
    }
    return std::nullopt;
}

}  // namespace traffic::domain
