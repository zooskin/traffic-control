#pragma once

/// \file
/// Clock abstraction.
///
/// docs/20_CODING_GUIDELINES.md §17 and docs/12_SOFTWARE_ARCHITECTURE.md §18:
/// production code does not call `std::chrono::*_clock::now()` directly. It
/// takes an `IClock&`. The same logic then runs unchanged under simulation,
/// production and replay.
///
/// This is not a testing convenience. docs/01_REQUIREMENTS.md NFR-003 requires
/// identical results from identical input and seed, and every reservation
/// window, deadlock confirmation timer and stale-state check reads the clock.
/// A single stray `now()` makes a run unreproducible.

#include "traffic/core/time.h"

namespace traffic::core {

/// Source of traffic time.
class IClock {
public:
    IClock() = default;
    virtual ~IClock() = default;

    IClock(const IClock&) = delete;
    IClock& operator=(const IClock&) = delete;
    IClock(IClock&&) = delete;
    IClock& operator=(IClock&&) = delete;

    /// Current point on the traffic timeline. Must never move backwards.
    [[nodiscard]] virtual TimePoint now() const = 0;
};

/// Production clock, backed by `std::chrono::steady_clock`.
///
/// steady_clock rather than system_clock: an NTP correction must not make
/// reservation windows or timeouts jump. Wall-clock stamps for external
/// payloads are produced separately at the API boundary.
class SystemClock final : public IClock {
public:
    SystemClock();

    [[nodiscard]] TimePoint now() const override;

private:
    std::chrono::steady_clock::time_point origin_;
};

/// Clock under explicit control, for simulation, replay and tests.
///
/// Time only moves when the owner advances it, so a scenario runs identically
/// on every machine and at any speed.
class SimulationClock final : public IClock {
public:
    SimulationClock() = default;
    explicit SimulationClock(TimePoint start) noexcept;

    [[nodiscard]] TimePoint now() const override;

    /// Move time forward by \p delta.
    /// \pre \p delta must not be negative — time never runs backwards.
    void advance(Duration delta);

    /// Jump to an absolute point.
    /// \pre \p point must not precede the current time.
    void set(TimePoint point);

private:
    TimePoint now_{kTimeOrigin};
};

}  // namespace traffic::core
