#include "traffic/core/clock.h"

#include <cassert>

namespace traffic::core {

// ---------------------------------------------------------------- SystemClock

SystemClock::SystemClock() : origin_(std::chrono::steady_clock::now()) {}

TimePoint SystemClock::now() const {
    const auto elapsed = std::chrono::steady_clock::now() - origin_;
    return kTimeOrigin + std::chrono::duration_cast<Duration>(elapsed);
}

// ------------------------------------------------------------ SimulationClock

SimulationClock::SimulationClock(TimePoint start) noexcept : now_(start) {}

TimePoint SimulationClock::now() const { return now_; }

void SimulationClock::advance(Duration delta) {
    // A negative step is a caller bug, not external input, so this is an
    // assertion rather than a returned error (docs/20_CODING_GUIDELINES.md §19).
    // Time running backwards would silently corrupt reservation windows and
    // deadlock confirmation timers.
    assert(delta >= Duration::zero() && "SimulationClock: time must not run backwards");
    now_ += delta;
}

void SimulationClock::set(TimePoint point) {
    assert(point >= now_ && "SimulationClock: time must not run backwards");
    now_ = point;
}

}  // namespace traffic::core
