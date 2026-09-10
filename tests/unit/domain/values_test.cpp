/// Tests for domain value objects.
///
/// Naming follows docs/20_CODING_GUIDELINES.md §45.

#include "traffic/domain/values.h"

#include <algorithm>
#include <sstream>
#include <type_traits>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/time.h"
#include "traffic/domain/errors.h"

namespace traffic::domain {
namespace {

using core::kTimeOrigin;
using core::Milliseconds;
using core::Seconds;
using core::TimePoint;

// ------------------------------------------------------------------- Position

TEST(Position, position_compares_by_value) {
    EXPECT_EQ((Position{1.5, 2.5}), (Position{1.5, 2.5}));
    EXPECT_NE((Position{1.5, 2.5}), (Position{1.5, 2.6}));
}

TEST(Position, position_defaults_to_origin) {
    const Position p;
    EXPECT_EQ(p.x, 0.0);
    EXPECT_EQ(p.y, 0.0);
}

TEST(Velocity, velocity_compares_by_value) {
    EXPECT_EQ((Velocity{0.5, -0.5}), (Velocity{0.5, -0.5}));
    EXPECT_NE((Velocity{0.5, -0.5}), (Velocity{0.5, 0.5}));
}

// ------------------------------------------------------------------- Priority

TEST(Priority, priority_orders_by_value_with_higher_winning) {
    EXPECT_LT(Priority{10}, Priority{20});
    EXPECT_GT(Priority{100}, Priority{99});
    EXPECT_EQ(Priority{7}, Priority{7});
}

TEST(Priority, priority_defaults_to_zero) {
    EXPECT_EQ(Priority{}.value(), 0);
    EXPECT_EQ(Priority{}, kLowestPriority);
}

TEST(Priority, priority_accepts_negative_values) {
    // Aging can push a priority below its base; nothing forbids it.
    EXPECT_LT(Priority{-5}, kLowestPriority);
}

TEST(Priority, priority_construction_from_raw_int_is_explicit) {
    static_assert(!std::is_convertible_v<Priority::value_type, Priority>);
    SUCCEED();
}

TEST(Priority, priority_streams_its_value) {
    std::ostringstream out;
    out << Priority{42};
    EXPECT_EQ(out.str(), "42");
}

/// Ordering has to be total and stable, or the same conflict resolves
/// differently between runs (docs/24_DOMAIN_MODEL.md §17).
TEST(Priority, priority_sorts_deterministically) {
    std::vector<Priority> values{Priority{3}, Priority{1}, Priority{2}, Priority{1}};
    std::sort(values.begin(), values.end());

    EXPECT_EQ(values.front(), Priority{1});
    EXPECT_EQ(values.back(), Priority{3});
}

// -------------------------------------------------------------------- Version

TEST(Version, version_defaults_to_zero) {
    EXPECT_EQ(StateVersion{}.value(), 0U);
}

TEST(Version, version_next_increments) {
    const StateVersion v{41};
    EXPECT_EQ(v.next().value(), 42U);
    EXPECT_EQ(v.value(), 41U) << "next() must not mutate the original";
}

TEST(Version, version_detects_stale_data) {
    const StateVersion planned_against{100};
    const StateVersion current{105};

    EXPECT_TRUE(planned_against.is_stale_against(current));
    EXPECT_FALSE(current.is_stale_against(planned_against));
}

TEST(Version, version_equal_to_current_is_not_stale) {
    const StateVersion v{100};
    EXPECT_FALSE(v.is_stale_against(StateVersion{100}));
}

/// A map version must not be comparable with a state version — mixing them is
/// how a stale-result check silently stops checking anything.
TEST(Version, version_tags_are_unrelated_types) {
    static_assert(!std::is_same_v<StateVersion, MapVersion>);
    static_assert(!std::is_convertible_v<StateVersion, MapVersion>);
    SUCCEED();
}

// ----------------------------------------------------------------- TimeWindow

TEST(TimeWindow, time_window_reports_its_duration) {
    const TimeWindow window{kTimeOrigin, kTimeOrigin + Seconds{10}};
    EXPECT_EQ(window.duration(), core::Duration{Seconds{10}});
}

TEST(TimeWindow, time_window_contains_start_but_not_end) {
    const TimeWindow window{kTimeOrigin + Seconds{5}, kTimeOrigin + Seconds{10}};

    EXPECT_TRUE(window.contains(kTimeOrigin + Seconds{5}));
    EXPECT_TRUE(window.contains(kTimeOrigin + Seconds{9}));
    EXPECT_FALSE(window.contains(kTimeOrigin + Seconds{10})) << "the interval is half-open";
    EXPECT_FALSE(window.contains(kTimeOrigin + Seconds{4}));
}

TEST(TimeWindow, time_window_detects_overlap) {
    const TimeWindow a{kTimeOrigin, kTimeOrigin + Seconds{10}};
    const TimeWindow b{kTimeOrigin + Seconds{5}, kTimeOrigin + Seconds{15}};

    EXPECT_TRUE(a.overlaps(b));
    EXPECT_TRUE(b.overlaps(a)) << "overlap is symmetric";
}

/// Back-to-back windows must not overlap, otherwise one robot cannot hand a
/// corridor to the next without an invented gap between them.
TEST(TimeWindow, time_window_touching_windows_do_not_overlap) {
    const TimeWindow first{kTimeOrigin, kTimeOrigin + Seconds{10}};
    const TimeWindow second{kTimeOrigin + Seconds{10}, kTimeOrigin + Seconds{20}};

    EXPECT_FALSE(first.overlaps(second));
    EXPECT_FALSE(second.overlaps(first));
}

TEST(TimeWindow, time_window_disjoint_windows_do_not_overlap) {
    const TimeWindow first{kTimeOrigin, kTimeOrigin + Seconds{5}};
    const TimeWindow second{kTimeOrigin + Seconds{10}, kTimeOrigin + Seconds{15}};

    EXPECT_FALSE(first.overlaps(second));
}

TEST(TimeWindow, time_window_contained_window_overlaps) {
    const TimeWindow outer{kTimeOrigin, kTimeOrigin + Seconds{100}};
    const TimeWindow inner{kTimeOrigin + Seconds{40}, kTimeOrigin + Seconds{50}};

    EXPECT_TRUE(outer.overlaps(inner));
    EXPECT_TRUE(inner.overlaps(outer));
}

TEST(TimeWindow, time_window_compares_by_value) {
    const TimeWindow a{kTimeOrigin, kTimeOrigin + Seconds{10}};
    const TimeWindow b{kTimeOrigin, kTimeOrigin + Seconds{10}};
    const TimeWindow c{kTimeOrigin, kTimeOrigin + Seconds{11}};

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
}

// ------------------------------------------------------- make_time_window

TEST(TimeWindow, make_time_window_accepts_a_positive_span) {
    const auto result = make_time_window(kTimeOrigin, kTimeOrigin + Milliseconds{1});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().duration(), core::Duration{Milliseconds{1}});
}

TEST(TimeWindow, make_time_window_rejects_an_empty_span) {
    const auto result = make_time_window(kTimeOrigin, kTimeOrigin);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::invalid_time_window);
}

TEST(TimeWindow, make_time_window_rejects_an_inverted_span) {
    const auto result = make_time_window(kTimeOrigin + Seconds{10}, kTimeOrigin);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), DomainError::invalid_time_window);
}

// --------------------------------------------------------------- DomainError

TEST(DomainError, domain_error_names_are_stable) {
    EXPECT_EQ(to_string(DomainError::empty_id), "EMPTY_ID");
    EXPECT_EQ(to_string(DomainError::invalid_transition), "INVALID_TRANSITION");
    EXPECT_EQ(to_string(DomainError::invalid_time_window), "INVALID_TIME_WINDOW");
    EXPECT_EQ(to_string(DomainError::empty_route), "EMPTY_ROUTE");
    EXPECT_EQ(to_string(DomainError::same_source_and_destination), "SAME_SOURCE_AND_DESTINATION");
}

}  // namespace
}  // namespace traffic::domain
