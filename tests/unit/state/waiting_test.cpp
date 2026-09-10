/// Waiting reasons. docs/04_ROBOT_TASK_MODEL.md §7~8.

#include "traffic/state/waiting.h"

#include <chrono>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"

namespace traffic::state {
namespace {

core::Duration seconds(int count) {
    return std::chrono::duration_cast<core::Duration>(core::Seconds{count});
}

TEST(WaitingReasonName, waiting_reason_names_round_trip) {
    for (const WaitingReason reason : {WaitingReason::resource_occupied,
                                       WaitingReason::higher_priority_robot,
                                       WaitingReason::human_blockage,
                                       WaitingReason::reservation_denied,
                                       WaitingReason::congestion,
                                       WaitingReason::deadlock_recovery,
                                       WaitingReason::safety_stop}) {
        const std::string_view name = to_string(reason);
        EXPECT_NE(name, "UNKNOWN");
        EXPECT_EQ(waiting_reason_from_string(name), reason);
    }
}

TEST(WaitingReasonName, waiting_reason_unknown_name_parses_to_nothing) {
    EXPECT_FALSE(waiting_reason_from_string("SOMETHING_ELSE").has_value());
    EXPECT_FALSE(waiting_reason_from_string("").has_value());
}

TEST(WaitingReasonName, waiting_reason_names_match_the_spec) {
    // These appear in logs and in the events of §19, so they are part of the
    // interface rather than an implementation detail.
    EXPECT_EQ(to_string(WaitingReason::resource_occupied), "RESOURCE_OCCUPIED");
    EXPECT_EQ(to_string(WaitingReason::human_blockage), "HUMAN_BLOCKAGE");
    EXPECT_EQ(to_string(WaitingReason::deadlock_recovery), "DEADLOCK_RECOVERY");
}

TEST(WaitingResolvable, waiting_traffic_problems_are_traffic_resolvable) {
    EXPECT_TRUE(is_traffic_resolvable(WaitingReason::resource_occupied));
    EXPECT_TRUE(is_traffic_resolvable(WaitingReason::higher_priority_robot));
    EXPECT_TRUE(is_traffic_resolvable(WaitingReason::reservation_denied));
    EXPECT_TRUE(is_traffic_resolvable(WaitingReason::congestion));
    EXPECT_TRUE(is_traffic_resolvable(WaitingReason::deadlock_recovery));
}

TEST(WaitingResolvable, waiting_on_a_person_is_not_a_traffic_problem) {
    // Rearranging reservations does not move someone out of an aisle, and
    // replanning for it produces routes that cannot help.
    EXPECT_FALSE(is_traffic_resolvable(WaitingReason::human_blockage));
}

TEST(WaitingResolvable, waiting_on_the_safety_system_is_not_a_traffic_problem) {
    // CLAUDE.md keeps safety in a separate system. We record that it acted so
    // our own timers do not mistake the pause for congestion, and we do not
    // try to resolve it.
    EXPECT_FALSE(is_traffic_resolvable(WaitingReason::safety_stop));
}

TEST(WaitingContextTest, waiting_context_measures_from_when_the_wait_began) {
    // The start, not a running duration: a duration recomputed every tick
    // drifts, and priority ageing is derived from this.
    WaitingContext context;
    context.reason = WaitingReason::resource_occupied;
    context.resource = core::ResourceId{"CORRIDOR-01"};
    context.since = core::kTimeOrigin + seconds(10);

    EXPECT_EQ(waited_for(context, core::kTimeOrigin + seconds(40)), seconds(30));
}

TEST(WaitingContextTest, waiting_context_of_a_wait_that_has_not_started_is_zero) {
    // A replayed log can hand us one. A negative wait would make a robot look
    // like it had been patient for a very long time.
    WaitingContext context;
    context.since = core::kTimeOrigin + seconds(100);

    EXPECT_EQ(waited_for(context, core::kTimeOrigin + seconds(10)), core::Duration::zero());
}

TEST(WaitingContextTest, waiting_context_need_not_name_a_resource) {
    // A person blocks a place, not a reservation.
    WaitingContext context;
    context.reason = WaitingReason::human_blockage;

    EXPECT_FALSE(context.resource.has_value());
}

}  // namespace
}  // namespace traffic::state
