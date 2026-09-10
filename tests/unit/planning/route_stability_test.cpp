/// Route stability. docs/05_GLOBAL_ROUTING.md §16~17.
///
/// docs/05_GLOBAL_ROUTING.md §25 names test_route_hysteresis. The oscillation
/// test at the bottom is the one that matters: it is the case the improvement
/// threshold alone cannot catch, because every step of an A -> B -> A cycle is
/// an improvement at the moment it is taken.

#include "traffic/planning/route_stability.h"

#include <chrono>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/route.h"

namespace traffic::planning {
namespace {

core::Duration seconds(int count) {
    return std::chrono::duration_cast<core::Duration>(core::Seconds{count});
}

RouteStabilityPolicy policy_with(double improvement, int hold_seconds) {
    RouteStabilityPolicy policy;
    policy.minimum_improvement = improvement;
    policy.minimum_hold_time = seconds(hold_seconds);
    return policy;
}

// -------------------------------------------------------------- the policy

TEST(StabilityPolicy, stability_policy_defaults_to_the_spec_example) {
    // docs/05_GLOBAL_ROUTING.md §16 gives 0.10 as its example. It is a
    // starting point for the Phase 15 benchmark, not a measured result.
    const RouteStabilityPolicy policy;
    EXPECT_DOUBLE_EQ(policy.minimum_improvement, 0.10);
}

TEST(StabilityPolicy, stability_policy_defaults_are_valid) {
    EXPECT_TRUE(make_stability_policy(RouteStabilityPolicy{}).has_value());
}

TEST(StabilityPolicy, stability_policy_rejects_a_negative_improvement) {
    EXPECT_FALSE(make_stability_policy(policy_with(-0.1, 0)).has_value());
}

TEST(StabilityPolicy, stability_policy_rejects_demanding_a_free_route) {
    // An improvement of 1 means the new route must cost nothing, so no replan
    // would ever be adopted and a robot would keep driving into a closed
    // aisle.
    EXPECT_FALSE(make_stability_policy(policy_with(1.0, 0)).has_value());
    EXPECT_FALSE(make_stability_policy(policy_with(1.5, 0)).has_value());
}

TEST(StabilityPolicy, stability_policy_rejects_a_negative_hold_time) {
    RouteStabilityPolicy policy;
    policy.minimum_hold_time = -seconds(1);

    EXPECT_FALSE(make_stability_policy(policy).has_value());
}

TEST(StabilityPolicy, always_replan_policy_holds_nothing_back) {
    const RouteStabilityPolicy policy = always_replan_policy();

    EXPECT_DOUBLE_EQ(policy.minimum_improvement, 0.0);
    EXPECT_EQ(policy.minimum_hold_time, core::Duration::zero());
    EXPECT_TRUE(make_stability_policy(policy).has_value());
}

// ------------------------------------------------------------ the decision

TEST(ReplanDecision, replan_adopts_a_route_that_beats_the_threshold) {
    const RouteStabilityPolicy policy = policy_with(0.10, 0);

    // 100 -> 85 is a 15% improvement, past the 10% threshold.
    EXPECT_EQ(decide_replan(policy, 100.0, 85.0, true, seconds(60)),
              ReplanDecision::adopted_improved);
}

TEST(ReplanDecision, replan_keeps_a_route_that_is_only_slightly_better) {
    const RouteStabilityPolicy policy = policy_with(0.10, 0);

    // 5% better. Not worth re-reserving every resource on the route for.
    EXPECT_EQ(decide_replan(policy, 100.0, 95.0, true, seconds(60)),
              ReplanDecision::kept_insufficient_improvement);
}

TEST(ReplanDecision, replan_keeps_a_route_at_exactly_the_threshold) {
    // §16 writes the rule as a strict inequality: new_cost < old_cost * (1 - m).
    // Equal is not an improvement.
    const RouteStabilityPolicy policy = policy_with(0.10, 0);

    EXPECT_EQ(decide_replan(policy, 100.0, 90.0, true, seconds(60)),
              ReplanDecision::kept_insufficient_improvement);
}

TEST(ReplanDecision, replan_keeps_a_route_that_would_get_worse) {
    const RouteStabilityPolicy policy = policy_with(0.10, 0);

    EXPECT_EQ(decide_replan(policy, 100.0, 200.0, true, seconds(60)),
              ReplanDecision::kept_insufficient_improvement);
}

TEST(ReplanDecision, replan_keeps_a_route_that_is_still_within_its_hold_time) {
    const RouteStabilityPolicy policy = policy_with(0.10, 30);

    // Half the price, and only 5 seconds old. §17 holds it anyway.
    EXPECT_EQ(decide_replan(policy, 100.0, 50.0, true, seconds(5)),
              ReplanDecision::kept_within_hold_time);
}

TEST(ReplanDecision, replan_adopts_once_the_hold_time_has_passed) {
    const RouteStabilityPolicy policy = policy_with(0.10, 30);

    EXPECT_EQ(decide_replan(policy, 100.0, 50.0, true, seconds(30)),
              ReplanDecision::adopted_improved);
}

TEST(ReplanDecision, replan_always_replaces_a_route_that_can_no_longer_be_driven) {
    // The ordering that matters. A corridor closed under the robot is not a
    // case for hysteresis: asking whether the alternative is 10% cheaper than
    // a route through a closed aisle is asking the wrong question.
    const RouteStabilityPolicy policy = policy_with(0.50, 3600);

    EXPECT_EQ(decide_replan(policy, 100.0, 500.0, false, core::Duration::zero()),
              ReplanDecision::adopted_current_invalid);
}

TEST(ReplanDecision, replan_decisions_say_whether_the_route_changed) {
    EXPECT_TRUE(is_adopted(ReplanDecision::adopted_improved));
    EXPECT_TRUE(is_adopted(ReplanDecision::adopted_current_invalid));
    EXPECT_FALSE(is_adopted(ReplanDecision::kept_insufficient_improvement));
    EXPECT_FALSE(is_adopted(ReplanDecision::kept_within_hold_time));
}

TEST(ReplanDecision, replan_decision_every_value_has_a_name) {
    for (const ReplanDecision decision : {ReplanDecision::adopted_current_invalid,
                                          ReplanDecision::adopted_improved,
                                          ReplanDecision::kept_insufficient_improvement,
                                          ReplanDecision::kept_within_hold_time}) {
        EXPECT_NE(to_string(decision), "UNKNOWN");
    }
}

// ------------------------------------------------------------- oscillation

TEST(ReplanDecision, replan_hold_time_stops_a_route_oscillating_between_two_options) {
    // The scenario of §17, played out. Route A costs 100. Congestion makes B
    // look better at 80; taking it moves the congestion, and A looks better at
    // 64. Every step is a genuine improvement, so the improvement threshold
    // alone would let this run forever.
    const RouteStabilityPolicy hysteresis = policy_with(0.10, 30);
    const RouteStabilityPolicy improvement_only = policy_with(0.10, 0);

    // First switch: both policies agree, the route is old enough.
    EXPECT_EQ(decide_replan(hysteresis, 100.0, 80.0, true, seconds(60)),
              ReplanDecision::adopted_improved);
    EXPECT_EQ(decide_replan(improvement_only, 100.0, 80.0, true, seconds(60)),
              ReplanDecision::adopted_improved);

    // Two seconds later the pendulum swings back. Without a hold time nothing
    // stops it.
    EXPECT_EQ(decide_replan(improvement_only, 80.0, 64.0, true, seconds(2)),
              ReplanDecision::adopted_improved);
    EXPECT_EQ(decide_replan(hysteresis, 80.0, 64.0, true, seconds(2)),
              ReplanDecision::kept_within_hold_time);
}

// --------------------------------------------------------------- route age

TEST(RouteAge, route_age_is_the_time_since_it_was_created) {
    domain::Route route;
    route.created_at = core::kTimeOrigin + seconds(10);

    EXPECT_EQ(route_age(route, core::kTimeOrigin + seconds(35)), seconds(25));
}

TEST(RouteAge, route_age_of_a_route_created_in_the_future_is_zero) {
    // A replayed log can hand us one. Reporting a negative age would let it
    // slip past the hold-time check.
    domain::Route route;
    route.created_at = core::kTimeOrigin + seconds(100);

    EXPECT_EQ(route_age(route, core::kTimeOrigin + seconds(10)), core::Duration::zero());
}

}  // namespace
}  // namespace traffic::planning
