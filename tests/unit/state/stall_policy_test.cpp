/// The stall ladder. docs/04_ROBOT_TASK_MODEL.md §9,
/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §17~18.
///
/// docs/04_ROBOT_TASK_MODEL.md §25 names test_blocked_state; this is where the
/// threshold that produces it is decided.

#include "traffic/state/stall_policy.h"

#include <chrono>

#include <gtest/gtest.h>

#include "traffic/core/time.h"
#include "traffic/domain/robot_state.h"
#include "traffic/domain/values.h"

namespace traffic::state {
namespace {

using domain::Position;
using domain::RobotState;

core::Duration seconds(int count) {
    return std::chrono::duration_cast<core::Duration>(core::Seconds{count});
}

StallPolicy ladder(int stop_seconds, int blocked_seconds) {
    StallPolicy policy;
    policy.stop_timeout = seconds(stop_seconds);
    policy.blocked_timeout = seconds(blocked_seconds);
    return policy;
}

// ---------------------------------------------------------------- the policy

TEST(StallPolicyTest, stall_policy_defaults_to_the_only_number_the_spec_gives) {
    // docs/04_ROBOT_TASK_MODEL.md §9's blocked_timeout = 5 sec and
    // minimum_progress = 0.1 m.
    const StallPolicy policy;

    EXPECT_EQ(policy.stop_timeout, seconds(5));
    EXPECT_DOUBLE_EQ(policy.minimum_progress, 0.1);
}

TEST(StallPolicyTest, stall_policy_does_not_escalate_to_failed_by_default) {
    // Declaring a robot failed releases its resources and sends traffic
    // through where it is standing. That is not a default; it is a number
    // somebody has to measure.
    const StallPolicy policy;

    EXPECT_FALSE(policy.blocked_timeout.has_value());
    EXPECT_EQ(assess_stall(policy, seconds(10'000)), StallVerdict::blocked);
}

TEST(StallPolicyTest, stall_policy_defaults_are_valid) {
    EXPECT_TRUE(make_stall_policy(StallPolicy{}).has_value());
}

TEST(StallPolicyTest, stall_policy_rejects_a_zero_stop_timeout) {
    // Every pause would instantly be a blockage, which removes the state that
    // exists to absorb someone walking past.
    StallPolicy policy;
    policy.stop_timeout = core::Duration::zero();

    EXPECT_FALSE(make_stall_policy(policy).has_value());
}

TEST(StallPolicyTest, stall_policy_rejects_a_negative_progress_threshold) {
    StallPolicy policy;
    policy.minimum_progress = -0.1;

    EXPECT_FALSE(make_stall_policy(policy).has_value());
}

TEST(StallPolicyTest, stall_policy_rejects_a_blocked_timeout_inside_the_stop_timeout) {
    // T2 <= T1 leaves no band in which a robot is merely blocked, so an
    // ordinary pause escalates straight to needing intervention.
    EXPECT_FALSE(make_stall_policy(ladder(10, 10)).has_value());
    EXPECT_FALSE(make_stall_policy(ladder(10, 5)).has_value());
    EXPECT_TRUE(make_stall_policy(ladder(10, 11)).has_value());
}

// ---------------------------------------------------------------- progress

TEST(Progress, progress_below_the_threshold_does_not_count) {
    // A robot inching forward against a pallet reports motion and gets
    // nowhere. Distance covered is what decides, not reported velocity.
    const StallPolicy policy;

    EXPECT_FALSE(has_progressed(policy, Position{0.0, 0.0, 0.0}, Position{0.05, 0.0, 0.0}));
    EXPECT_FALSE(has_progressed(policy, Position{0.0, 0.0, 0.0}, Position{0.0, 0.0, 0.0}));
}

TEST(Progress, progress_at_or_above_the_threshold_counts) {
    const StallPolicy policy;

    EXPECT_TRUE(has_progressed(policy, Position{0.0, 0.0, 0.0}, Position{0.1, 0.0, 0.0}));
    EXPECT_TRUE(has_progressed(policy, Position{0.0, 0.0, 0.0}, Position{2.0, 0.0, 0.0}));
}

TEST(Progress, progress_is_measured_in_every_direction) {
    const StallPolicy policy;

    EXPECT_TRUE(has_progressed(policy, Position{0.0, 0.0, 0.0}, Position{0.0, -0.5, 0.0}));
    EXPECT_TRUE(has_progressed(policy, Position{0.0, 0.0, 0.0}, Position{0.0, 0.0, 0.5}));
}

TEST(Progress, stalled_for_measures_from_the_last_mark) {
    ProgressMark mark;
    mark.at = core::kTimeOrigin + seconds(10);

    EXPECT_EQ(stalled_for(mark, core::kTimeOrigin + seconds(17)), seconds(7));
}

TEST(Progress, stalled_for_a_mark_in_the_future_is_zero) {
    ProgressMark mark;
    mark.at = core::kTimeOrigin + seconds(100);

    EXPECT_EQ(stalled_for(mark, core::kTimeOrigin), core::Duration::zero());
}

// ------------------------------------------------------------- the ladder

TEST(StallLadder, stall_within_the_stop_timeout_is_only_temporary) {
    // docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §17: a robot that paused is not
    // a failure. Someone walking past does this several times an hour.
    const StallPolicy policy = ladder(5, 30);

    EXPECT_EQ(assess_stall(policy, core::Duration::zero()), StallVerdict::temporarily_stopped);
    EXPECT_EQ(assess_stall(policy, seconds(4)), StallVerdict::temporarily_stopped);
}

TEST(StallLadder, stall_past_the_stop_timeout_is_blocked) {
    const StallPolicy policy = ladder(5, 30);

    EXPECT_EQ(assess_stall(policy, seconds(5)), StallVerdict::blocked);
    EXPECT_EQ(assess_stall(policy, seconds(29)), StallVerdict::blocked);
}

TEST(StallLadder, stall_past_the_blocked_timeout_needs_intervention) {
    const StallPolicy policy = ladder(5, 30);

    EXPECT_EQ(assess_stall(policy, seconds(30)), StallVerdict::failed);
    EXPECT_EQ(assess_stall(policy, seconds(300)), StallVerdict::failed);
}

TEST(StallLadder, stall_past_both_thresholds_does_not_come_back_as_blocked) {
    // The ladder is checked worst-first. Checked the other way round, a robot
    // stuck for an hour reports as blocked forever and never escalates.
    const StallPolicy policy = ladder(5, 30);

    EXPECT_EQ(assess_stall(policy, seconds(3'600)), StallVerdict::failed);
}

TEST(StallLadder, stall_verdicts_map_to_states) {
    EXPECT_FALSE(state_for(StallVerdict::progressing).has_value());
    EXPECT_EQ(state_for(StallVerdict::temporarily_stopped), RobotState::temporarily_stopped);
    EXPECT_EQ(state_for(StallVerdict::blocked), RobotState::blocked);
    EXPECT_EQ(state_for(StallVerdict::failed), RobotState::failed);
}

TEST(StallLadder, stall_verdict_every_value_has_a_name) {
    for (const StallVerdict verdict : {StallVerdict::progressing,
                                       StallVerdict::temporarily_stopped,
                                       StallVerdict::blocked,
                                       StallVerdict::failed}) {
        EXPECT_NE(to_string(verdict), "UNKNOWN");
    }
}

// ------------------------------------------------------- the whole question

TEST(AssessProgress, assess_progress_reports_a_moving_robot_as_progressing) {
    const StallPolicy policy = ladder(5, 30);

    ProgressMark mark;
    mark.position = Position{0.0, 0.0, 0.0};
    mark.at = core::kTimeOrigin;

    // Long past both thresholds, and it has moved — so none of that matters.
    EXPECT_EQ(
        assess_progress(policy, mark, Position{10.0, 0.0, 0.0}, core::kTimeOrigin + seconds(300)),
        StallVerdict::progressing);
}

TEST(AssessProgress, assess_progress_walks_the_ladder_for_a_robot_going_nowhere) {
    const StallPolicy policy = ladder(5, 30);

    ProgressMark mark;
    mark.position = Position{4.0, 2.0, 0.0};
    mark.at = core::kTimeOrigin;

    const Position stuck{4.02, 2.0, 0.0};  // 2 cm, below the 10 cm threshold

    EXPECT_EQ(assess_progress(policy, mark, stuck, core::kTimeOrigin + seconds(1)),
              StallVerdict::temporarily_stopped);
    EXPECT_EQ(assess_progress(policy, mark, stuck, core::kTimeOrigin + seconds(10)),
              StallVerdict::blocked);
    EXPECT_EQ(assess_progress(policy, mark, stuck, core::kTimeOrigin + seconds(60)),
              StallVerdict::failed);
}

}  // namespace
}  // namespace traffic::state
