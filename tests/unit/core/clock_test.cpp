/// Tests for the clock abstraction.
///
/// docs/01_REQUIREMENTS.md NFR-003 (determinism) and docs/16_TEST_STRATEGY.md
/// §14 (deterministic replay) both rest on SimulationClock being fully under
/// the caller's control.

#include "traffic/core/clock.h"
#include "traffic/core/time.h"

#include <gtest/gtest.h>

#include <type_traits>
#include <vector>

namespace traffic::core {
namespace {

// ------------------------------------------------------------ SimulationClock

TEST(SimulationClock, simulation_clock_default_starts_at_origin) {
    const SimulationClock clock;
    EXPECT_EQ(clock.now(), kTimeOrigin);
}

TEST(SimulationClock, simulation_clock_starts_at_requested_point) {
    const SimulationClock clock{kTimeOrigin + Seconds{42}};
    EXPECT_EQ(clock.now(), kTimeOrigin + Seconds{42});
}

TEST(SimulationClock, simulation_clock_does_not_move_on_its_own) {
    const SimulationClock clock;
    const auto first = clock.now();
    for (int i = 0; i < 1000; ++i) {
        ASSERT_EQ(clock.now(), first);
    }
}

TEST(SimulationClock, simulation_clock_advance_moves_time_forward) {
    SimulationClock clock;
    clock.advance(Milliseconds{250});
    EXPECT_EQ(clock.now(), kTimeOrigin + Milliseconds{250});

    clock.advance(Milliseconds{750});
    EXPECT_EQ(clock.now(), kTimeOrigin + Seconds{1});
}

TEST(SimulationClock, simulation_clock_zero_advance_keeps_time) {
    SimulationClock clock{kTimeOrigin + Seconds{5}};
    clock.advance(Duration::zero());
    EXPECT_EQ(clock.now(), kTimeOrigin + Seconds{5});
}

TEST(SimulationClock, simulation_clock_set_jumps_to_absolute_point) {
    SimulationClock clock;
    clock.set(kTimeOrigin + Seconds{30});
    EXPECT_EQ(clock.now(), kTimeOrigin + Seconds{30});
}

/// Identical inputs must produce an identical timeline. This is the property
/// every replay and every seeded scenario depends on.
TEST(SimulationClock, simulation_clock_same_steps_produce_same_timeline) {
    const std::vector<Duration> steps{
        Milliseconds{10}, Milliseconds{5}, Seconds{1}, Milliseconds{125},
    };

    const auto run = [&steps] {
        SimulationClock clock;
        std::vector<TimePoint> observed;
        observed.reserve(steps.size());
        for (const auto& step : steps) {
            clock.advance(step);
            observed.push_back(clock.now());
        }
        return observed;
    };

    EXPECT_EQ(run(), run());
}

// ---------------------------------------------------------------- SystemClock

TEST(SystemClock, system_clock_starts_at_origin) {
    const SystemClock clock;
    // Constructed just now, so elapsed time is small but not necessarily zero.
    EXPECT_GE(clock.now(), kTimeOrigin);
    EXPECT_LT(clock.now(), kTimeOrigin + Seconds{1});
}

TEST(SystemClock, system_clock_never_moves_backwards) {
    const SystemClock clock;
    auto previous = clock.now();
    for (int i = 0; i < 10000; ++i) {
        const auto current = clock.now();
        ASSERT_GE(current, previous);
        previous = current;
    }
}

// --------------------------------------------------------------------- IClock

TEST(IClock, clock_implementations_are_usable_through_the_interface) {
    SimulationClock simulation;
    SystemClock system;

    const auto read = [](const IClock& clock) { return clock.now(); };

    simulation.advance(Seconds{3});
    EXPECT_EQ(read(simulation), kTimeOrigin + Seconds{3});
    EXPECT_GE(read(system), kTimeOrigin);
}

/// docs/20_CODING_GUIDELINES.md §17: production code takes an IClock&.
/// Copying a clock would let two owners drift apart.
TEST(IClock, clock_is_not_copyable_or_movable) {
    static_assert(!std::is_copy_constructible_v<IClock>);
    static_assert(!std::is_move_constructible_v<IClock>);
    static_assert(std::has_virtual_destructor_v<IClock>);
    SUCCEED();
}

// ----------------------------------------------------------------- TimePoint

/// TrafficClock has no now(), so time cannot enter the system behind IClock's
/// back. Guarding the property here means a future "convenience" now() gets
/// caught by a failing build rather than by a non-reproducible simulation.
TEST(TrafficClock, traffic_clock_exposes_no_now_function) {
    static_assert(!requires { TrafficClock::now(); },
                  "TrafficClock must not provide now(); time enters only through IClock");
    static_assert(!TrafficClock::is_steady);
    SUCCEED();
}

TEST(TrafficClock, traffic_clock_duration_has_nanosecond_resolution) {
    static_assert(std::is_same_v<Duration::period, std::nano>);
    EXPECT_EQ(Duration{Seconds{1}}.count(), 1'000'000'000);
}

}  // namespace
}  // namespace traffic::core
