/// Reservation policy and the request vocabulary.
/// docs/06_TRAFFIC_RESERVATION.md §5, §10, §13, §14, §17, §29,
/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §10, §15.
///
/// Small arithmetic, and two pieces of it decide whether the safety-critical
/// parts hold up: the buffer has to widen the window in both directions (a
/// buffer that only pads the end lets the next robot in early), and aging has
/// to saturate rather than wrap (an overflowed priority inverts the order and
/// hands the corridor to whoever waited longest for the shortest time).

#include "traffic/reservation/reservation_policy.h"

#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "reservation_test_map.h"
#include "traffic/core/ids.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/resources.h"
#include "traffic/domain/values.h"
#include "traffic/reservation/reservation_request.h"

namespace traffic::reservation {
namespace {

using domain::DomainError;
using domain::Priority;
using test::at;
using test::seconds;

// -------------------------------------------------------------- validation

TEST(ReservationPolicy, policy_defaults_are_valid) {
    const auto policy = make_reservation_policy(ReservationPolicy{});

    ASSERT_TRUE(policy.has_value());
    EXPECT_EQ(policy.value().entry_buffer, kDefaultEntryBuffer);
    EXPECT_EQ(policy.value().exit_buffer, kDefaultExitBuffer);
    EXPECT_EQ(policy.value().horizon, kDefaultHorizon);

    // No aging until a site configures it: docs/25 §15 leaves the coefficient
    // to benchmarking, and a number invented here would be one later
    // measurement has to argue with.
    EXPECT_DOUBLE_EQ(policy.value().aging_per_second, 0.0);
}

TEST(ReservationPolicy, policy_negative_buffer_is_rejected) {
    ReservationPolicy policy;
    policy.entry_buffer = -seconds(1);

    const auto checked = make_reservation_policy(policy);

    ASSERT_FALSE(checked.has_value());
    EXPECT_EQ(checked.error(), DomainError::negative_value);
}

TEST(ReservationPolicy, policy_negative_aging_is_rejected) {
    ReservationPolicy policy;
    policy.aging_per_second = -1.0;

    // Aging downwards would cost a robot ground for waiting, which is
    // starvation with extra steps.
    const auto checked = make_reservation_policy(policy);

    ASSERT_FALSE(checked.has_value());
    EXPECT_EQ(checked.error(), DomainError::negative_value);
}

TEST(ReservationPolicy, policy_zero_horizon_is_rejected) {
    ReservationPolicy policy;
    policy.horizon = core::Duration::zero();

    const auto checked = make_reservation_policy(policy);

    ASSERT_FALSE(checked.has_value());
    EXPECT_EQ(checked.error(), DomainError::non_positive_value);
}

TEST(ReservationPolicy, policy_zero_grant_timeout_is_rejected) {
    ReservationPolicy policy;
    policy.grant_timeout = core::Duration::zero();

    const auto checked = make_reservation_policy(policy);

    ASSERT_FALSE(checked.has_value());
    EXPECT_EQ(checked.error(), DomainError::non_positive_value);
}

// ------------------------------------------------------------------ window

TEST(ReservationPolicy, window_is_widened_at_both_ends) {
    ReservationPolicy policy;
    policy.entry_buffer = seconds(2);
    policy.exit_buffer = seconds(3);

    const auto window = effective_window(at(10), seconds(5), policy);

    // §13. Both ends, because position error and comms delay make a robot
    // arrive early as readily as leave late.
    ASSERT_TRUE(window.has_value());
    EXPECT_EQ(window.value().start(), at(8));
    EXPECT_EQ(window.value().end(), at(18));
}

TEST(ReservationPolicy, window_without_buffers_is_exactly_what_was_asked_for) {
    const auto window = effective_window(at(10), seconds(5), test::unbuffered_policy());

    ASSERT_TRUE(window.has_value());
    EXPECT_EQ(window.value().start(), at(10));
    EXPECT_EQ(window.value().end(), at(15));
}

TEST(ReservationPolicy, window_of_no_duration_and_no_buffer_is_rejected) {
    const auto window = effective_window(at(10), core::Duration::zero(), test::unbuffered_policy());

    // docs/24_DOMAIN_MODEL.md §34. A zero-length claim would be granted and
    // expire in the same instant, which reads in the log as a resource that
    // was never held.
    ASSERT_FALSE(window.has_value());
    EXPECT_EQ(window.error(), DomainError::invalid_time_window);
}

TEST(ReservationPolicy, window_of_no_duration_still_holds_the_buffers) {
    ReservationPolicy policy;
    policy.entry_buffer = seconds(1);
    policy.exit_buffer = seconds(1);

    const auto window = effective_window(at(10), core::Duration::zero(), policy);

    // A robot passing straight through a node still occupies it for as long as
    // the buffers say.
    ASSERT_TRUE(window.has_value());
    EXPECT_EQ(window.value().duration(), seconds(2));
}

TEST(ReservationPolicy, window_of_negative_duration_is_rejected) {
    const auto window = effective_window(at(10), -seconds(5), test::unbuffered_policy());

    ASSERT_FALSE(window.has_value());
    EXPECT_EQ(window.error(), DomainError::negative_value);
}

// ------------------------------------------------------------------- aging

TEST(ReservationPolicy, aging_leaves_priority_alone_when_it_is_switched_off) {
    const ReservationPolicy policy;

    EXPECT_EQ(effective_priority(Priority{3}, seconds(1000), policy), Priority{3});
}

TEST(ReservationPolicy, aging_raises_priority_with_waiting_time) {
    ReservationPolicy policy;
    policy.aging_per_second = 0.5;

    // §17: base + aging_factor * waiting_time.
    EXPECT_EQ(effective_priority(Priority{2}, seconds(10), policy), Priority{7});
}

TEST(ReservationPolicy, aging_ignores_a_wait_that_has_not_started) {
    ReservationPolicy policy;
    policy.aging_per_second = 1.0;

    // A replayed log can produce a negative span. Rewarding it would let a
    // clock artefact win a corridor.
    EXPECT_EQ(effective_priority(Priority{4}, -seconds(10), policy), Priority{4});
}

TEST(ReservationPolicy, aging_saturates_instead_of_wrapping) {
    ReservationPolicy policy;
    policy.aging_per_second = 1000.0;

    constexpr Priority::value_type kCeiling = std::numeric_limits<Priority::value_type>::max();

    // Wrapping here would invert the whole ordering: the robot that had waited
    // longest would become the one that yields.
    EXPECT_EQ(effective_priority(Priority{kCeiling}, seconds(3600), policy), Priority{kCeiling});
}

// --------------------------------------------------------------- direction

TEST(ReservationRequest, direction_follows_the_end_the_robot_enters_by) {
    const map::Map graph = test::reference_map();
    const domain::Corridor* corridor = graph.find_corridor(core::ResourceId{"CORRIDOR-01"});
    ASSERT_NE(corridor, nullptr);

    const auto forwards = direction_from_entry(*corridor, core::NodeId{"A"});
    const auto backwards = direction_from_entry(*corridor, core::NodeId{"B"});
    const auto sideways = direction_from_entry(*corridor, core::NodeId{"M1"});

    ASSERT_TRUE(forwards.has_value());
    EXPECT_EQ(forwards.value(), TravelDirection::forward);
    ASSERT_TRUE(backwards.has_value());
    EXPECT_EQ(backwards.value(), TravelDirection::reverse);

    // A node in the middle is not an end. Guessing a direction for it would
    // put a robot into a corridor against the traffic already in it.
    EXPECT_FALSE(sideways.has_value());
}

TEST(ReservationRequest, decision_names_read_the_same_everywhere) {
    // §36 puts these in the decision log, where they are read by people.
    EXPECT_EQ(std::string{to_string(DecisionStatus::granted)}, "GRANTED");
    EXPECT_EQ(std::string{to_string(DecisionStatus::wait)}, "WAIT");
    EXPECT_EQ(std::string{to_string(DenialReason::resource_occupied)}, "RESOURCE_OCCUPIED");
    EXPECT_EQ(std::string{to_string(TravelDirection::reverse)}, "REVERSE");
}

}  // namespace
}  // namespace traffic::reservation
