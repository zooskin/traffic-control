/// Robot commands. docs/04_ROBOT_TASK_MODEL.md §23.

#include "traffic/state/robot_command.h"

#include <optional>
#include <utility>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"

namespace traffic::state {
namespace {

using core::CommandId;
using core::NodeId;
using core::RobotId;
using core::RouteId;

core::Result<RobotCommand, domain::DomainError> command(CommandAction action,
                                                        std::optional<RouteId> route,
                                                        std::optional<NodeId> target) {
    return make_robot_command(CommandId{"CMD-1"},
                              RobotId{"R01"},
                              action,
                              std::move(route),
                              std::move(target),
                              core::kTimeOrigin);
}

// ------------------------------------------------------------------- naming

TEST(CommandActionName, command_action_names_round_trip) {
    for (const CommandAction action : {CommandAction::move,
                                       CommandAction::wait,
                                       CommandAction::stop,
                                       CommandAction::resume,
                                       CommandAction::reroute,
                                       CommandAction::go_to_waiting_bay}) {
        const std::string_view name = to_string(action);
        EXPECT_NE(name, "UNKNOWN");
        EXPECT_EQ(command_action_from_string(name), action);
    }
}

TEST(CommandActionName, command_action_names_match_the_spec) {
    EXPECT_EQ(to_string(CommandAction::move), "MOVE");
    EXPECT_EQ(to_string(CommandAction::reroute), "REROUTE");
    EXPECT_EQ(to_string(CommandAction::go_to_waiting_bay), "GO_TO_WAITING_BAY");
}

TEST(CommandActionName, command_action_has_no_emergency_stop) {
    // docs/04_ROBOT_TASK_MODEL.md §23 separates Emergency Stop from ordinary
    // traffic commands, and CLAUDE.md puts it in another system entirely. A
    // traffic controller that could issue one is a traffic controller people
    // would start relying on for safety, and it is not built to that standard.
    EXPECT_FALSE(command_action_from_string("EMERGENCY_STOP").has_value());
    EXPECT_FALSE(command_action_from_string("E_STOP").has_value());
}

// ------------------------------------------------------------------- motion

TEST(CommandMotion, command_motion_identifies_the_actions_that_start_a_robot) {
    // What the reservation manager needs to know: these must not be sent
    // before the resources ahead are held.
    EXPECT_TRUE(sets_a_robot_moving(CommandAction::move));
    EXPECT_TRUE(sets_a_robot_moving(CommandAction::resume));
    EXPECT_TRUE(sets_a_robot_moving(CommandAction::reroute));
    EXPECT_TRUE(sets_a_robot_moving(CommandAction::go_to_waiting_bay));
}

TEST(CommandMotion, command_motion_identifies_the_actions_that_stop_one) {
    EXPECT_FALSE(sets_a_robot_moving(CommandAction::wait));
    EXPECT_FALSE(sets_a_robot_moving(CommandAction::stop));
}

// -------------------------------------------------------------- construction

TEST(CommandFactory, command_factory_builds_a_move_with_a_route) {
    const auto result = command(CommandAction::move, RouteId{"ROUTE-1"}, std::nullopt);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().action, CommandAction::move);
    EXPECT_EQ(result.value().route, RouteId{"ROUTE-1"});
    EXPECT_EQ(result.value().robot_id, RobotId{"R01"});
}

TEST(CommandFactory, command_factory_refuses_a_move_with_nothing_to_drive) {
    // A robot told to move with no route has nothing to do and no useful way
    // to say so. The failure belongs where the caller still knows why it was
    // building the command.
    EXPECT_FALSE(command(CommandAction::move, std::nullopt, std::nullopt).has_value());
}

TEST(CommandFactory, command_factory_refuses_a_reroute_with_nothing_to_drive) {
    EXPECT_FALSE(command(CommandAction::reroute, std::nullopt, std::nullopt).has_value());
}

TEST(CommandFactory, command_factory_refuses_a_bay_move_with_nowhere_to_go) {
    EXPECT_FALSE(command(CommandAction::go_to_waiting_bay, std::nullopt, std::nullopt).has_value());
}

TEST(CommandFactory, command_factory_builds_a_bay_move_with_a_target) {
    const auto result = command(CommandAction::go_to_waiting_bay, std::nullopt, NodeId{"BAY-01"});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().target_node, NodeId{"BAY-01"});
}

TEST(CommandFactory, command_factory_builds_a_stop_with_neither) {
    // Stopping needs no destination.
    EXPECT_TRUE(command(CommandAction::stop, std::nullopt, std::nullopt).has_value());
    EXPECT_TRUE(command(CommandAction::wait, std::nullopt, std::nullopt).has_value());
    EXPECT_TRUE(command(CommandAction::resume, std::nullopt, std::nullopt).has_value());
}

TEST(CommandFactory, command_factory_refuses_empty_identifiers) {
    EXPECT_FALSE(make_robot_command(CommandId{},
                                    RobotId{"R01"},
                                    CommandAction::stop,
                                    std::nullopt,
                                    std::nullopt,
                                    core::kTimeOrigin)
                     .has_value());
    EXPECT_FALSE(make_robot_command(CommandId{"CMD-1"},
                                    RobotId{},
                                    CommandAction::stop,
                                    std::nullopt,
                                    std::nullopt,
                                    core::kTimeOrigin)
                     .has_value());
}

TEST(CommandFactory, command_compares_by_identity) {
    // docs/24_DOMAIN_MODEL.md §33.
    const auto first = command(CommandAction::stop, std::nullopt, std::nullopt);
    const auto second = command(CommandAction::move, RouteId{"ROUTE-1"}, std::nullopt);

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first.value(), second.value());
}

}  // namespace
}  // namespace traffic::state
