/// State-change records. docs/04_ROBOT_TASK_MODEL.md §19.

#include "traffic/state/state_change.h"

#include <string>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/domain/robot_state.h"
#include "traffic/state/waiting.h"

namespace traffic::state {
namespace {

using domain::RobotState;

RobotStateChange change(RobotState from, RobotState to) {
    RobotStateChange value;
    value.robot_id = core::RobotId{"R01"};
    value.from = from;
    value.to = to;
    value.version = domain::StateVersion{42};
    return value;
}

TEST(StallOnset, stall_onset_fires_when_a_moving_robot_stops_making_progress) {
    // The transitions worth alerting on: a fleet where these are climbing is
    // jamming, whatever the throughput number says.
    EXPECT_TRUE(is_stall_onset(change(RobotState::moving, RobotState::waiting)));
    EXPECT_TRUE(is_stall_onset(change(RobotState::moving, RobotState::temporarily_stopped)));
    EXPECT_TRUE(is_stall_onset(change(RobotState::moving, RobotState::blocked)));
    EXPECT_TRUE(is_stall_onset(change(RobotState::reserving, RobotState::waiting)));
}

TEST(StallOnset, stall_onset_does_not_fire_for_a_robot_that_was_already_stopped) {
    // A robot moving from waiting to blocked has not just stalled. Counting it
    // would make the alert climb for as long as nothing changed.
    EXPECT_FALSE(is_stall_onset(change(RobotState::waiting, RobotState::blocked)));
    EXPECT_FALSE(is_stall_onset(change(RobotState::blocked, RobotState::replanning)));
}

TEST(StallOnset, stall_onset_does_not_fire_for_an_idle_robot) {
    // An idle robot is not stalled; it has nothing to do.
    EXPECT_FALSE(is_stall_onset(change(RobotState::idle, RobotState::reserving)));
    EXPECT_FALSE(is_stall_onset(change(RobotState::moving, RobotState::idle)));
}

TEST(StallOnset, stall_onset_does_not_fire_for_a_failure) {
    // A failed robot is a different alert with a different response. Folding
    // it into the jam count would hide both.
    EXPECT_FALSE(is_stall_onset(change(RobotState::moving, RobotState::failed)));
}

TEST(Recovery, recovery_fires_when_a_halted_robot_moves_again) {
    EXPECT_TRUE(is_recovery(change(RobotState::waiting, RobotState::moving)));
    EXPECT_TRUE(is_recovery(change(RobotState::temporarily_stopped, RobotState::moving)));
    EXPECT_TRUE(is_recovery(change(RobotState::blocked, RobotState::moving)));
    EXPECT_TRUE(is_recovery(change(RobotState::replanning, RobotState::moving)));
}

TEST(Recovery, recovery_does_not_fire_for_a_robot_that_was_never_stuck) {
    EXPECT_FALSE(is_recovery(change(RobotState::idle, RobotState::moving)));
    EXPECT_FALSE(is_recovery(change(RobotState::reserving, RobotState::moving)));
}

TEST(Recovery, recovery_does_not_fire_for_a_robot_that_gave_up) {
    // Waiting to idle is a task ending, not a jam clearing.
    EXPECT_FALSE(is_recovery(change(RobotState::waiting, RobotState::idle)));
}

TEST(StateChangeLog, state_change_log_line_names_the_states_and_the_version) {
    const std::string line = to_log_line(change(RobotState::moving, RobotState::blocked));

    EXPECT_NE(line.find("robot=R01"), std::string::npos) << line;
    EXPECT_NE(line.find("from=MOVING"), std::string::npos) << line;
    EXPECT_NE(line.find("to=BLOCKED"), std::string::npos) << line;
    EXPECT_NE(line.find("version=42"), std::string::npos) << line;
}

TEST(StateChangeLog, state_change_log_line_names_the_waiting_reason) {
    RobotStateChange held = change(RobotState::moving, RobotState::waiting);
    held.reason = WaitingReason::human_blockage;
    held.resource = core::ResourceId{"CORRIDOR-01"};

    const std::string line = to_log_line(held);

    EXPECT_NE(line.find("reason=HUMAN_BLOCKAGE"), std::string::npos) << line;
    EXPECT_NE(line.find("resource=CORRIDOR-01"), std::string::npos) << line;
}

TEST(StateChangeLog, state_change_log_line_marks_absent_fields) {
    const std::string line = to_log_line(change(RobotState::idle, RobotState::reserving));

    EXPECT_NE(line.find("reason=-"), std::string::npos) << line;
    EXPECT_NE(line.find("resource=-"), std::string::npos) << line;
}

TEST(StateChangeLog, state_change_log_line_keeps_a_fixed_field_order) {
    // Two runs of a scenario are compared line by line to show the run
    // replayed identically.
    const std::string line = to_log_line(change(RobotState::moving, RobotState::blocked));

    EXPECT_LT(line.find("robot="), line.find("from=")) << line;
    EXPECT_LT(line.find("from="), line.find("to=")) << line;
    EXPECT_LT(line.find("to="), line.find("version=")) << line;
}

}  // namespace
}  // namespace traffic::state
