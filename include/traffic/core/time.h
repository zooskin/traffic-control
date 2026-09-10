#pragma once

/// \file
/// The project's time vocabulary.
///
/// docs/20_CODING_GUIDELINES.md §16 requires explicit time types and forbids
/// raw integer timestamps in core logic. docs/24_DOMAIN_MODEL.md §27 requires
/// every time value to share one basis.
///
/// `TrafficClock` deliberately has **no** `now()`. Time enters the system only
/// through `IClock` (see clock.h), so `TimePoint::clock::now()` does not
/// compile. That is the point: docs/01_REQUIREMENTS.md NFR-003 (determinism)
/// and the replay requirement in docs/16_TEST_STRATEGY.md §14 both depend on
/// simulation being able to supply its own time.

#include <chrono>
#include <cstdint>

namespace traffic::core {

/// The single time basis for all traffic logic.
///
/// Not a Cpp17Clock: it intentionally omits `now()`.
struct TrafficClock {
    using rep = std::int64_t;
    using period = std::nano;
    using duration = std::chrono::duration<rep, period>;
    using time_point = std::chrono::time_point<TrafficClock, duration>;

    /// Simulation time may be stepped or rewound, so this is never steady.
    /// The name is fixed by the standard clock interface, not by our style.
    // NOLINTNEXTLINE(readability-identifier-naming)
    static constexpr bool is_steady = false;
};

/// A point on the traffic timeline. Monotonic within one run; its epoch is
/// arbitrary and carries no wall-clock meaning.
using TimePoint = TrafficClock::time_point;

/// An elapsed span on the traffic timeline.
using Duration = TrafficClock::duration;

// Convenience aliases so call sites read as intent rather than as arithmetic.
using Nanoseconds = std::chrono::nanoseconds;
using Microseconds = std::chrono::microseconds;
using Milliseconds = std::chrono::milliseconds;
using Seconds = std::chrono::seconds;

/// Wall-clock time, UTC. Used **only** at system boundaries — external API
/// payloads (docs/13_API_SPECIFICATION.md) and persisted history.
/// Never use this to drive a traffic decision; use `TimePoint` via `IClock`.
using WallTime = std::chrono::sys_time<Nanoseconds>;

/// The zero point of the traffic timeline.
inline constexpr TimePoint kTimeOrigin{Duration::zero()};

/// Distinguished value for "no deadline" / "does not expire".
inline constexpr TimePoint kNeverExpires{Duration::max()};

}  // namespace traffic::core
