#include "traffic/state/robot_command.h"

#include <array>
#include <utility>

namespace traffic::state {
namespace {

using CommandResult = core::Result<RobotCommand, domain::DomainError>;

constexpr std::array<std::pair<CommandAction, std::string_view>, 6> kNames{{
    {CommandAction::move, "MOVE"},
    {CommandAction::wait, "WAIT"},
    {CommandAction::stop, "STOP"},
    {CommandAction::resume, "RESUME"},
    {CommandAction::reroute, "REROUTE"},
    {CommandAction::go_to_waiting_bay, "GO_TO_WAITING_BAY"},
}};

/// True when the action cannot be carried out without a route.
[[nodiscard]] bool needs_a_route(CommandAction action) noexcept {
    return action == CommandAction::move || action == CommandAction::reroute;
}

}  // namespace

std::string_view to_string(CommandAction action) noexcept {
    for (const auto& [value, name] : kNames) {
        if (value == action) {
            return name;
        }
    }
    return "UNKNOWN";
}

std::optional<CommandAction> command_action_from_string(std::string_view name) noexcept {
    for (const auto& [value, text] : kNames) {
        if (text == name) {
            return value;
        }
    }
    return std::nullopt;
}

bool sets_a_robot_moving(CommandAction action) noexcept {
    switch (action) {
        case CommandAction::move:
        case CommandAction::resume:
        case CommandAction::reroute:
        case CommandAction::go_to_waiting_bay:
            return true;

        case CommandAction::wait:
        case CommandAction::stop:
            return false;
    }
    return false;
}

core::Result<RobotCommand, domain::DomainError> make_robot_command(
    core::CommandId id,
    core::RobotId robot_id,
    CommandAction action,
    std::optional<core::RouteId> route,
    std::optional<core::NodeId> target_node,
    core::TimePoint created_at) {
    if (id.empty() || robot_id.empty()) {
        return CommandResult::failure(domain::DomainError::empty_id);
    }
    if (needs_a_route(action) && !route.has_value()) {
        // A robot told to move with no route has nothing to drive, and no way
        // to report that usefully. Catch it here, where the caller still knows
        // why it was building the command.
        return CommandResult::failure(domain::DomainError::empty_id);
    }
    if (action == CommandAction::go_to_waiting_bay && !target_node.has_value()) {
        return CommandResult::failure(domain::DomainError::empty_id);
    }

    RobotCommand command;
    command.id = std::move(id);
    command.robot_id = std::move(robot_id);
    command.action = action;
    command.route = std::move(route);
    command.target_node = std::move(target_node);
    command.created_at = created_at;
    return CommandResult::success(std::move(command));
}

}  // namespace traffic::state
