/// Robot telemetry. docs/04_ROBOT_TASK_MODEL.md §20, §22.
///
/// The test that carries the weight is the one refusing a robot that reports
/// `waiting`. A robot claiming a traffic-control state is claiming we granted
/// it something, and believing that is how a reservation appears that was
/// never issued.

#include "traffic/state/robot_state_update.h"

#include <string>
#include <utility>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/domain/robot_state.h"

namespace traffic::state {
namespace {

using core::EdgeId;
using core::NodeId;
using core::RobotId;
using domain::RobotState;

RobotStateUpdate update_for(std::string robot) {
    RobotStateUpdate update;
    update.robot_id = RobotId{std::move(robot)};
    update.battery = 80.0;
    return update;
}

// -------------------------------------------------------- who may say what

TEST(RobotReportable, robot_reportable_accepts_what_a_robot_can_observe) {
    EXPECT_TRUE(is_robot_reportable(RobotState::idle));
    EXPECT_TRUE(is_robot_reportable(RobotState::moving));
    EXPECT_TRUE(is_robot_reportable(RobotState::temporarily_stopped));
    EXPECT_TRUE(is_robot_reportable(RobotState::blocked));
    EXPECT_TRUE(is_robot_reportable(RobotState::failed));
}

TEST(RobotReportable, robot_reportable_refuses_states_traffic_control_assigns) {
    // A robot cannot know it is reserving or replanning — those are things we
    // decided. `waiting` is the dangerous one: it asserts that a resource was
    // granted.
    EXPECT_FALSE(is_robot_reportable(RobotState::reserving));
    EXPECT_FALSE(is_robot_reportable(RobotState::waiting));
    EXPECT_FALSE(is_robot_reportable(RobotState::replanning));
}

TEST(RobotReportable, robot_reportable_allows_a_robot_to_admit_it_is_lost) {
    // Better than staying silent: a robot that says `unknown` gets handled,
    // one that says nothing has to be timed out first.
    EXPECT_TRUE(is_robot_reportable(RobotState::unknown));
}

TEST(RobotReportable, robot_reportable_covers_every_state) {
    // Adding a state without deciding who owns it would default it to
    // unreportable and quietly reject a fleet's telemetry.
    std::size_t reportable = 0;
    for (const RobotState state : domain::robot_states()) {
        if (is_robot_reportable(state)) {
            ++reportable;
        }
    }
    EXPECT_EQ(reportable, domain::kRobotStateCount - 3);
}

// --------------------------------------------------------------- validation

TEST(UpdateValidation, update_validation_accepts_ordinary_telemetry) {
    RobotStateUpdate update = update_for("R01");
    update.current_node = NodeId{"N1"};
    update.reported_state = RobotState::moving;

    EXPECT_TRUE(validate(update).has_value());
}

TEST(UpdateValidation, update_validation_accepts_telemetry_with_no_state) {
    // The common case. Position and battery change constantly; the state
    // rarely does, so most messages carry none.
    const RobotStateUpdate update = update_for("R01");

    EXPECT_TRUE(validate(update).has_value());
    EXPECT_FALSE(update.reported_state.has_value());
}

TEST(UpdateValidation, update_validation_rejects_an_unnamed_robot) {
    RobotStateUpdate update = update_for("R01");
    update.robot_id = RobotId{};

    const auto status = validate(update);

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::empty_robot_id);
}

TEST(UpdateValidation, update_validation_rejects_a_controller_owned_state) {
    RobotStateUpdate update = update_for("R01");
    update.reported_state = RobotState::waiting;

    const auto status = validate(update);

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::not_robot_reportable);
}

TEST(UpdateValidation, update_validation_rejects_an_impossible_battery) {
    RobotStateUpdate over = update_for("R01");
    over.battery = 101.0;

    RobotStateUpdate under = update_for("R01");
    under.battery = -1.0;

    EXPECT_EQ(validate(over).error(), UpdateRejection::battery_out_of_range);
    EXPECT_EQ(validate(under).error(), UpdateRejection::battery_out_of_range);
}

TEST(UpdateValidation, update_validation_accepts_the_battery_endpoints) {
    RobotStateUpdate empty = update_for("R01");
    empty.battery = 0.0;

    RobotStateUpdate full = update_for("R01");
    full.battery = 100.0;

    EXPECT_TRUE(validate(empty).has_value());
    EXPECT_TRUE(validate(full).has_value());
}

TEST(UpdateValidation, update_validation_rejects_being_at_a_node_and_on_an_edge) {
    // Ambiguous occupancy. A robot is standing somewhere or driving between
    // two somewheres, and which resource it holds depends on the answer.
    RobotStateUpdate update = update_for("R01");
    update.current_node = NodeId{"N1"};
    update.current_edge = EdgeId{"E1"};

    const auto status = validate(update);

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::at_a_node_and_on_an_edge);
}

TEST(UpdateValidation, update_validation_accepts_a_robot_that_knows_neither) {
    // A robot that has lost localisation still has useful telemetry. Refusing
    // it would leave us with nothing at all about a robot that is out there.
    const RobotStateUpdate update = update_for("R01");

    EXPECT_TRUE(validate(update).has_value());
}

TEST(UpdateRejectionName, update_rejection_every_value_has_a_name) {
    for (const UpdateRejection rejection : {UpdateRejection::empty_robot_id,
                                            UpdateRejection::unknown_robot,
                                            UpdateRejection::stale_timestamp,
                                            UpdateRejection::not_robot_reportable,
                                            UpdateRejection::invalid_transition,
                                            UpdateRejection::waiting_reason_mismatch,
                                            UpdateRejection::battery_out_of_range,
                                            UpdateRejection::at_a_node_and_on_an_edge}) {
        EXPECT_NE(to_string(rejection), "UNKNOWN");
    }
}

}  // namespace
}  // namespace traffic::state
