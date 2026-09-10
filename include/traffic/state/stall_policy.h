#pragma once

/// \file
/// When a stopped robot stops being merely stopped.
/// docs/04_ROBOT_TASK_MODEL.md §9, docs/25_TRAFFIC_CONTROL_SPECIFICATION.md
/// §17~18.
///
/// This is the policy the whole `temporarily_stopped` state exists for. A
/// person walking past stops a robot several times an hour. Treat that as a
/// failure and the fleet replans over something that clears itself in seconds;
/// never escalate and a genuinely stuck robot holds a corridor forever.
///
/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §18 draws the ladder:
///
///     0  .. T1     TEMPORARILY_STOPPED   still assumed to be nothing
///     T1 .. T2     BLOCKED               needs a new route
///     >  T2        FAILURE / RECOVERY    needs intervention
///
/// and says the real values come from benchmarking the site. T1 gets
/// docs/04_ROBOT_TASK_MODEL.md §9's example of five seconds; **T2 has no
/// default and is empty unless configured**. Declaring a robot failed releases
/// its resources (§11) and sends traffic through where it is standing. Doing
/// that on a number nobody measured is the kind of default that is discovered
/// during an incident.
///
/// "Stopped" is not the same as "reporting zero velocity". A robot inching
/// forward against an obstacle reports motion and gets nowhere, so progress is
/// measured as distance covered since the last place it was seen to move —
/// §9's `minimum_progress`.

#include <chrono>
#include <optional>
#include <string_view>

#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/robot_state.h"
#include "traffic/domain/values.h"

namespace traffic::state {

/// The thresholds of docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §18.
struct StallPolicy {
    /// T1. How long a stop is still treated as nothing.
    ///
    /// docs/04_ROBOT_TASK_MODEL.md §9's `blocked_timeout = 5 sec`, which is the
    /// same threshold named from the other side: past it, the robot is
    /// blocked.
    core::Duration stop_timeout{std::chrono::duration_cast<core::Duration>(core::Seconds{5})};

    /// T2. Past this the robot needs intervention rather than a new route.
    ///
    /// Empty by default, and deliberately: escalating to `failed` releases the
    /// robot's resources and routes traffic through its position. That is not
    /// a step to take on an invented number.
    std::optional<core::Duration> blocked_timeout;

    /// Metres. Movement below this since the last mark is not progress.
    /// docs/04_ROBOT_TASK_MODEL.md §9's `minimum_progress = 0.1 m`.
    double minimum_progress{0.1};

    [[nodiscard]] friend bool operator==(const StallPolicy&, const StallPolicy&) = default;
};

/// Validates a policy.
///
/// Rejects a non-positive T1, a negative progress threshold, and a T2 that is
/// not strictly beyond T1 — with T2 <= T1 the `blocked` band is empty and
/// every ordinary pause escalates straight past a state that exists to absorb
/// it.
[[nodiscard]] core::Result<StallPolicy, domain::DomainError> make_stall_policy(StallPolicy policy);

/// What the policy says about a robot that is not getting anywhere.
enum class StallVerdict {
    /// It has moved far enough to count. Nothing to do.
    progressing,

    /// Stopped, and still within T1.
    temporarily_stopped,

    /// Past T1: it needs a different route.
    blocked,

    /// Past T2: a new route will not help.
    failed,
};

[[nodiscard]] std::string_view to_string(StallVerdict verdict) noexcept;

/// The state a verdict corresponds to, or empty for `progressing`.
///
/// Kept as a function so the mapping lives in one place; a caller writing the
/// switch itself is a caller that will one day map `blocked` to `failed`.
[[nodiscard]] std::optional<domain::RobotState> state_for(StallVerdict verdict) noexcept;

/// The last place a robot was seen to be making progress.
///
/// Not "where it was last seen": a robot that reports a position every
/// hundred milliseconds while pressed against a pallet has plenty of recent
/// observations and has not moved. The mark only advances when it does.
struct ProgressMark {
    domain::Position position;
    core::TimePoint at{core::kTimeOrigin};

    [[nodiscard]] friend bool operator==(const ProgressMark&, const ProgressMark&) = default;
};

/// True when \p to is far enough from \p from to count as movement.
[[nodiscard]] bool has_progressed(const StallPolicy& policy,
                                  const domain::Position& from,
                                  const domain::Position& to) noexcept;

/// How long the robot has been going nowhere at \p now.
[[nodiscard]] core::Duration stalled_for(const ProgressMark& mark, core::TimePoint now) noexcept;

/// The ladder of §18, given how long a robot has been stalled.
///
/// Never returns `progressing`; a duration alone cannot say whether the robot
/// moved. Use `assess_progress` for that.
[[nodiscard]] StallVerdict assess_stall(const StallPolicy& policy, core::Duration stalled) noexcept;

/// The whole question: has this robot got anywhere, and if not, for how long?
[[nodiscard]] StallVerdict assess_progress(const StallPolicy& policy,
                                           const ProgressMark& mark,
                                           const domain::Position& position,
                                           core::TimePoint now) noexcept;

}  // namespace traffic::state
