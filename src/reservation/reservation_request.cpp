#include "traffic/reservation/reservation_request.h"

namespace traffic::reservation {

std::string_view to_string(TravelDirection direction) noexcept {
    switch (direction) {
        case TravelDirection::forward:
            return "FORWARD";
        case TravelDirection::reverse:
            return "REVERSE";
    }
    return "UNKNOWN";
}

std::optional<TravelDirection> direction_from_entry(const domain::Corridor& corridor,
                                                    const core::NodeId& entry) {
    if (entry == corridor.entry_node) {
        return TravelDirection::forward;
    }
    if (entry == corridor.exit_node) {
        return TravelDirection::reverse;
    }
    return std::nullopt;
}

std::string_view to_string(DecisionStatus status) noexcept {
    switch (status) {
        case DecisionStatus::granted:
            return "GRANTED";
        case DecisionStatus::wait:
            return "WAIT";
        case DecisionStatus::rejected:
            return "REJECTED";
    }
    return "UNKNOWN";
}

std::string_view to_string(DenialReason reason) noexcept {
    switch (reason) {
        case DenialReason::none:
            return "NONE";
        case DenialReason::resource_occupied:
            return "RESOURCE_OCCUPIED";
        case DenialReason::opposing_direction:
            return "OPPOSING_DIRECTION";
        case DenialReason::movement_conflict:
            return "MOVEMENT_CONFLICT";
        case DenialReason::empty_id:
            return "EMPTY_ID";
        case DenialReason::invalid_window:
            return "INVALID_WINDOW";
        case DenialReason::beyond_horizon:
            return "BEYOND_HORIZON";
        case DenialReason::already_terminal:
            return "ALREADY_TERMINAL";
        case DenialReason::duplicate_request:
            return "DUPLICATE_REQUEST";
    }
    return "UNKNOWN";
}

bool is_granted(const ReservationDecision& decision) noexcept {
    return decision.status == DecisionStatus::granted;
}

}  // namespace traffic::reservation
