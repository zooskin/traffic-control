#pragma once

/// \file
/// Why a robot is waiting. docs/04_ROBOT_TASK_MODEL.md §7~8.
///
/// A stopped robot looks identical from the outside whatever stopped it. The
/// reason is what decides the response: a robot waiting on a busy corridor is
/// working as designed, one waiting because it lost a priority contest may be
/// starving (docs/09_PRIORITY_MANAGER.md), and one waiting on a human needs a
/// different timeout entirely because people move at their own pace.
///
/// The reason is *our* conclusion, not something the robot reported, which is
/// why it lives here and not in `RobotStateSnapshot`. A snapshot is what was
/// observed; mixing a decision into it would make two identical observations
/// compare unequal because of something we decided about them, and would leave
/// no way to tell what the fleet said from what we inferred. Decision D-009 in
/// docs/00_INDEX.md.

#include <optional>
#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"

namespace traffic::state {

/// docs/04_ROBOT_TASK_MODEL.md §8.
enum class WaitingReason {
    /// The resource it needs is held by someone else.
    resource_occupied,

    /// It lost a priority contest. docs/09_PRIORITY_MANAGER.md decides these;
    /// recording the outcome is what makes starvation visible.
    higher_priority_robot,

    /// A person is in the way. Held apart from the others because people are
    /// not resources: they do not release on a schedule and cannot be
    /// negotiated with, so this reason carries its own patience.
    human_blockage,

    /// A reservation request was refused.
    reservation_denied,

    /// The route ahead is busy enough to be worth pausing for.
    congestion,

    /// Standing aside so a deadlock can be broken.
    /// docs/22_IMPLEMENTATION_WORKFLOW.md Phase 10.
    deadlock_recovery,

    /// The safety system stopped it.
    ///
    /// Recorded, never caused. CLAUDE.md keeps Emergency Stop, Protective
    /// Stop, Safety Zones and Safety Interlocks in a separate system; traffic
    /// control notes that one of them acted so its own timers do not mistake
    /// the pause for a traffic problem, and does nothing else with it.
    safety_stop,
};

[[nodiscard]] std::string_view to_string(WaitingReason reason) noexcept;
[[nodiscard]] std::optional<WaitingReason> waiting_reason_from_string(
    std::string_view name) noexcept;

/// True when the wait is on something traffic control can resolve.
///
/// False for a human blockage and a safety stop: no amount of reservation
/// juggling clears either, and treating them as traffic problems produces
/// replans that cannot help.
[[nodiscard]] bool is_traffic_resolvable(WaitingReason reason) noexcept;

/// What a robot is waiting on, and since when.
/// docs/04_ROBOT_TASK_MODEL.md §7.
struct WaitingContext {
    WaitingReason reason{WaitingReason::resource_occupied};

    /// The resource being waited for. Empty for a wait that is not about one —
    /// a human in the aisle blocks a place, not a reservation.
    std::optional<core::ResourceId> resource;

    /// When the wait began. `waiting_since` in §7.
    ///
    /// The start, not the duration: a duration would have to be recalculated
    /// on every tick and would drift. Ageing for priority
    /// (docs/09_PRIORITY_MANAGER.md) is computed from this.
    core::TimePoint since{core::kTimeOrigin};

    [[nodiscard]] friend bool operator==(const WaitingContext&, const WaitingContext&) = default;
};

/// How long the robot has been waiting at \p now.
///
/// Zero for a wait that has not started yet, which a replayed log can produce.
[[nodiscard]] core::Duration waited_for(const WaitingContext& context,
                                        core::TimePoint now) noexcept;

}  // namespace traffic::state
