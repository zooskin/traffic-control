#pragma once

/// \file
/// Traffic events. docs/24_DOMAIN_MODEL.md §18.
///
/// Events are the controller's only input. docs/00_MASTER_PLAN.md §4.4 makes
/// the system event-driven precisely so that a single robot stopping does not
/// cost a fleet-wide replan.
///
/// docs/20_CODING_GUIDELINES.md §39 requires events to be immutable, so every
/// field here is const. An event records what happened; nothing downstream may
/// edit that record.
///
/// Each event carries the state and map versions current when it was raised.
/// docs/23_SYSTEM_ARCHITECTURE.md §14 asks for them, and they are what lets a
/// late-arriving event be recognised as stale rather than acted on
/// (docs/10_TRAFFIC_CONTROLLER.md §22 out-of-order events).

#include <optional>
#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/values.h"

namespace traffic::domain {

/// What happened. docs/24_DOMAIN_MODEL.md §18.
enum class TrafficEventType {
    robot_state_updated,

    /// Stopped by something outside traffic control, and expected to resume.
    robot_stopped,

    /// Cannot continue on its current route.
    robot_blocked,

    /// Moving again after a stop or a blockage.
    robot_recovered,

    /// Out of service. Its resources have to be released.
    robot_failed,

    task_created,
    task_completed,
    task_cancelled,

    /// A held resource passed its window without being released.
    reservation_expired,

    /// A person or their activity is occupying a resource.
    human_detected,

    /// That occupation has ended.
    human_cleared,

    /// Topology or edge attributes changed; routes planned on the old map are
    /// now suspect.
    map_updated,
};

[[nodiscard]] std::string_view to_string(TrafficEventType type) noexcept;
[[nodiscard]] std::optional<TrafficEventType> traffic_event_type_from_string(
    std::string_view name) noexcept;

/// True when the event concerns one robot rather than the fleet or the map.
[[nodiscard]] bool is_robot_event(TrafficEventType type) noexcept;

/// An immutable record of something that happened.
///
/// Built through `make_traffic_event`; the const members mean an event cannot
/// be assigned over or edited after the fact.
class TrafficEvent {
public:
    TrafficEvent(core::EventId id,
                 TrafficEventType type,
                 core::TimePoint timestamp,
                 std::optional<core::RobotId> robot_id,
                 std::optional<core::ResourceId> resource_id,
                 StateVersion state_version,
                 MapVersion map_version,
                 core::CorrelationId correlation_id);

    [[nodiscard]] const core::EventId& id() const noexcept { return id_; }
    [[nodiscard]] TrafficEventType type() const noexcept { return type_; }
    [[nodiscard]] core::TimePoint timestamp() const noexcept { return timestamp_; }

    [[nodiscard]] const std::optional<core::RobotId>& robot_id() const noexcept {
        return robot_id_;
    }
    [[nodiscard]] const std::optional<core::ResourceId>& resource_id() const noexcept {
        return resource_id_;
    }

    [[nodiscard]] StateVersion state_version() const noexcept { return state_version_; }
    [[nodiscard]] MapVersion map_version() const noexcept { return map_version_; }

    /// Ties this event to the log lines, metrics and decisions it produced —
    /// docs/23_SYSTEM_ARCHITECTURE.md §24.
    [[nodiscard]] const core::CorrelationId& correlation_id() const noexcept {
        return correlation_id_;
    }

    /// True when this event was raised against an older world than \p current.
    [[nodiscard]] bool is_stale(StateVersion current) const noexcept;

private:
    // Immutability here is enforced by the absence of mutators: every accessor
    // is const and nothing sets a field after construction
    // (docs/20_CODING_GUIDELINES.md §39).
    //
    // The members are deliberately *not* const. Const members would make the
    // type non-assignable, and events have to be sortable — they arrive out of
    // order and the controller reorders them by timestamp
    // (docs/10_TRAFFIC_CONTROLLER.md §22). An unsortable event is worse than a
    // technically-assignable one that nothing assigns to.
    core::EventId id_;
    TrafficEventType type_;
    core::TimePoint timestamp_;
    std::optional<core::RobotId> robot_id_;
    std::optional<core::ResourceId> resource_id_;
    StateVersion state_version_;
    MapVersion map_version_;
    core::CorrelationId correlation_id_;
};

/// Builds a TrafficEvent.
///
/// Rejects an empty event id, and a robot-scoped event with no robot: an
/// event saying a robot stopped without saying which one cannot be acted on.
[[nodiscard]] core::Result<TrafficEvent, DomainError> make_traffic_event(
    core::EventId id,
    TrafficEventType type,
    core::TimePoint timestamp,
    std::optional<core::RobotId> robot_id,
    StateVersion state_version,
    MapVersion map_version);

}  // namespace traffic::domain
