#include "traffic/state/robot_state_update.h"

namespace traffic::state {
namespace {

using UpdateStatus = core::Status<UpdateRejection>;

constexpr double kMinBattery = 0.0;
constexpr double kMaxBattery = 100.0;

}  // namespace

bool is_robot_reportable(domain::RobotState state) noexcept {
    switch (state) {
        // Things traffic control did to the robot. Only traffic control sets
        // them, so a robot claiming one is either confused or replayed.
        case domain::RobotState::reserving:
        case domain::RobotState::waiting:
        case domain::RobotState::replanning:
            return false;

        // Things a robot can actually observe about itself. `unknown` is
        // included because a robot that has lost its own bearings saying so is
        // more useful than a robot that stays silent.
        case domain::RobotState::idle:
        case domain::RobotState::moving:
        case domain::RobotState::temporarily_stopped:
        case domain::RobotState::blocked:
        case domain::RobotState::failed:
        case domain::RobotState::unknown:
            return true;
    }
    return false;
}

std::string_view to_string(UpdateRejection rejection) noexcept {
    switch (rejection) {
        case UpdateRejection::empty_robot_id:
            return "EMPTY_ROBOT_ID";
        case UpdateRejection::unknown_robot:
            return "UNKNOWN_ROBOT";
        case UpdateRejection::stale_timestamp:
            return "STALE_TIMESTAMP";
        case UpdateRejection::not_robot_reportable:
            return "NOT_ROBOT_REPORTABLE";
        case UpdateRejection::invalid_transition:
            return "INVALID_TRANSITION";
        case UpdateRejection::battery_out_of_range:
            return "BATTERY_OUT_OF_RANGE";
        case UpdateRejection::at_a_node_and_on_an_edge:
            return "AT_A_NODE_AND_ON_AN_EDGE";
    }
    return "UNKNOWN";
}

core::Status<UpdateRejection> validate(const RobotStateUpdate& update) {
    if (update.robot_id.empty()) {
        return UpdateStatus::failure(UpdateRejection::empty_robot_id);
    }
    if (update.battery < kMinBattery || update.battery > kMaxBattery) {
        return UpdateStatus::failure(UpdateRejection::battery_out_of_range);
    }
    if (update.current_node.has_value() && update.current_edge.has_value()) {
        return UpdateStatus::failure(UpdateRejection::at_a_node_and_on_an_edge);
    }
    if (update.reported_state.has_value() && !is_robot_reportable(*update.reported_state)) {
        return UpdateStatus::failure(UpdateRejection::not_robot_reportable);
    }
    return UpdateStatus::success();
}

}  // namespace traffic::state
