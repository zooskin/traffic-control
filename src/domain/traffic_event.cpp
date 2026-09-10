#include "traffic/domain/traffic_event.h"

#include <array>
#include <utility>

namespace traffic::domain {
namespace {

constexpr std::array<std::pair<TrafficEventType, std::string_view>, 12> kEventTypeNames{{
    {TrafficEventType::robot_state_updated, "ROBOT_STATE_UPDATED"},
    {TrafficEventType::robot_stopped, "ROBOT_STOPPED"},
    {TrafficEventType::robot_blocked, "ROBOT_BLOCKED"},
    {TrafficEventType::robot_recovered, "ROBOT_RECOVERED"},
    {TrafficEventType::robot_failed, "ROBOT_FAILED"},
    {TrafficEventType::task_created, "TASK_CREATED"},
    {TrafficEventType::task_completed, "TASK_COMPLETED"},
    {TrafficEventType::task_cancelled, "TASK_CANCELLED"},
    {TrafficEventType::reservation_expired, "RESERVATION_EXPIRED"},
    {TrafficEventType::human_detected, "HUMAN_DETECTED"},
    {TrafficEventType::human_cleared, "HUMAN_CLEARED"},
    {TrafficEventType::map_updated, "MAP_UPDATED"},
}};

using EventResult = core::Result<TrafficEvent, DomainError>;

}  // namespace

std::string_view to_string(TrafficEventType type) noexcept {
    for (const auto& [value, name] : kEventTypeNames) {
        if (value == type) {
            return name;
        }
    }
    return "ROBOT_STATE_UPDATED";
}

std::optional<TrafficEventType> traffic_event_type_from_string(std::string_view name) noexcept {
    for (const auto& [value, candidate] : kEventTypeNames) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}

bool is_robot_event(TrafficEventType type) noexcept {
    switch (type) {
        case TrafficEventType::robot_state_updated:
        case TrafficEventType::robot_stopped:
        case TrafficEventType::robot_blocked:
        case TrafficEventType::robot_recovered:
        case TrafficEventType::robot_failed:
            return true;
        case TrafficEventType::task_created:
        case TrafficEventType::task_completed:
        case TrafficEventType::task_cancelled:
        case TrafficEventType::reservation_expired:
        case TrafficEventType::human_detected:
        case TrafficEventType::human_cleared:
        case TrafficEventType::map_updated:
            return false;
    }
    return false;
}

TrafficEvent::TrafficEvent(core::EventId id,
                           TrafficEventType type,
                           core::TimePoint timestamp,
                           std::optional<core::RobotId> robot_id,
                           std::optional<core::ResourceId> resource_id,
                           StateVersion state_version,
                           MapVersion map_version,
                           core::CorrelationId correlation_id)
    : id_(std::move(id)),
      type_(type),
      timestamp_(timestamp),
      robot_id_(std::move(robot_id)),
      resource_id_(std::move(resource_id)),
      state_version_(state_version),
      map_version_(map_version),
      correlation_id_(std::move(correlation_id)) {}

bool TrafficEvent::is_stale(StateVersion current) const noexcept {
    return state_version_.is_stale_against(current);
}

core::Result<TrafficEvent, DomainError> make_traffic_event(core::EventId id,
                                                           TrafficEventType type,
                                                           core::TimePoint timestamp,
                                                           std::optional<core::RobotId> robot_id,
                                                           StateVersion state_version,
                                                           MapVersion map_version) {
    if (id.empty()) {
        return EventResult::failure(DomainError::empty_id);
    }
    if (is_robot_event(type) && (!robot_id.has_value() || robot_id->empty())) {
        // "A robot stopped" with no robot named cannot be acted on, and would
        // reach the controller as an event it has to silently drop.
        return EventResult::failure(DomainError::empty_id);
    }

    // The correlation id defaults to the event id: every event starts its own
    // trace unless a caller joins it to an existing one.
    core::CorrelationId correlation{id.value()};

    return EventResult::success(TrafficEvent{std::move(id),
                                             type,
                                             timestamp,
                                             std::move(robot_id),
                                             std::nullopt,
                                             state_version,
                                             map_version,
                                             std::move(correlation)});
}

}  // namespace traffic::domain
