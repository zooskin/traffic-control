#include "traffic/domain/reservation.h"

#include <array>
#include <utility>

namespace traffic::domain {
namespace {

constexpr std::array<std::pair<ResourceType, std::string_view>, 6> kResourceTypeNames{{
    {ResourceType::node, "NODE"},
    {ResourceType::edge, "EDGE"},
    {ResourceType::corridor, "CORRIDOR"},
    {ResourceType::intersection, "INTERSECTION"},
    {ResourceType::charging_area, "CHARGING_AREA"},
    {ResourceType::loading_area, "LOADING_AREA"},
}};

using ReservationResult = core::Result<Reservation, DomainError>;

}  // namespace

std::string_view to_string(ResourceType type) noexcept {
    for (const auto& [value, name] : kResourceTypeNames) {
        if (value == type) {
            return name;
        }
    }
    return "EDGE";
}

std::optional<ResourceType> resource_type_from_string(std::string_view name) noexcept {
    for (const auto& [value, candidate] : kResourceTypeNames) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}

bool conflicts_with(const Reservation& lhs, const Reservation& rhs) noexcept {
    if (lhs.resource_id != rhs.resource_id) {
        return false;
    }
    if (lhs.robot_id == rhs.robot_id) {
        // A robot does not block itself. Treating consecutive claims by one
        // robot as a conflict produces a deadlock cycle of length one.
        return false;
    }
    if (!holds_resource(lhs.state) || !holds_resource(rhs.state)) {
        return false;
    }
    return lhs.window.overlaps(rhs.window);
}

bool is_past_window(const Reservation& reservation, core::TimePoint now) noexcept {
    return now >= reservation.window.end();
}

core::Result<Reservation, DomainError> with_state(const Reservation& reservation,
                                                  ReservationState next) {
    if (!is_transition_allowed(reservation.state, next)) {
        return ReservationResult::failure(DomainError::invalid_transition);
    }

    Reservation updated = reservation;
    updated.state = next;
    return ReservationResult::success(std::move(updated));
}

core::Result<Reservation, DomainError> make_reservation(core::ReservationId id,
                                                        core::RobotId robot_id,
                                                        core::ResourceId resource_id,
                                                        TimeWindow window,
                                                        Priority priority,
                                                        core::TimePoint requested_at) {
    if (id.empty() || robot_id.empty() || resource_id.empty()) {
        return ReservationResult::failure(DomainError::empty_id);
    }
    if (window.end() <= window.start()) {
        // Reachable only if a caller built the window directly rather than
        // through make_time_window, which is why it is re-checked here.
        return ReservationResult::failure(DomainError::invalid_time_window);
    }

    // Aggregate initialisation: TimeWindow has no default constructor, by
    // design — there is no sensible empty window.
    Reservation reservation{
        .id = std::move(id),
        .robot_id = std::move(robot_id),
        .resource_id = std::move(resource_id),
        .window = window,
        .state = ReservationState::pending,
        .priority = priority,
        .requested_at = requested_at,
    };
    return ReservationResult::success(std::move(reservation));
}

}  // namespace traffic::domain
