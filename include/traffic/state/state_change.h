#pragma once

/// \file
/// A robot's state moved. docs/04_ROBOT_TASK_MODEL.md §19.
///
/// §19 calls this RobotStateChanged and gives the fields: robot_id, old_state,
/// new_state, reason, resource_id. This is that, plus the instant and the fleet
/// version — everything else in the system carries both, and a change that did
/// not would be the one record you could not line up against the rest.
///
/// It is returned rather than published. The state manager hands the change
/// back to whoever caused it; turning it into a `TrafficEvent`, logging it or
/// dropping it is the caller's decision. A manager that published events itself
/// would need a listener registry, and the order those listeners ran in would
/// quietly become part of the system's behaviour.
///
/// The `reason` is only ever set for a move into `waiting`, because that is the
/// only state we assign for a reason we know. A robot that stopped did so for
/// reasons of its own.

#include <optional>
#include <string>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/robot_state.h"
#include "traffic/domain/values.h"
#include "traffic/state/waiting.h"

namespace traffic::state {

/// One state transition that actually happened.
struct RobotStateChange {
    core::RobotId robot_id;

    domain::RobotState from{domain::RobotState::unknown};
    domain::RobotState to{domain::RobotState::unknown};

    /// When the change was observed or decided.
    core::TimePoint at{core::kTimeOrigin};

    /// The fleet version this change produced. docs/23_SYSTEM_ARCHITECTURE.md
    /// §2.4: the number a later decision can be checked against.
    domain::StateVersion version;

    /// Why, when the change was into `waiting`.
    std::optional<WaitingReason> reason;

    /// What it is waiting for, when the reason names a resource.
    std::optional<core::ResourceId> resource;

    [[nodiscard]] friend bool operator==(const RobotStateChange&,
                                         const RobotStateChange&) = default;
};

/// True when the robot stopped making progress as a result of this change.
///
/// The transitions worth alerting on: a fleet where these are climbing is a
/// fleet that is jamming, whatever the throughput number says.
[[nodiscard]] bool is_stall_onset(const RobotStateChange& change) noexcept;

/// True when the robot started making progress again.
[[nodiscard]] bool is_recovery(const RobotStateChange& change) noexcept;

/// The change as one line, fields in a fixed order.
///
/// Fixed order because two runs of a scenario are compared line by line to
/// show the run replayed identically.
[[nodiscard]] std::string to_log_line(const RobotStateChange& change);

}  // namespace traffic::state
