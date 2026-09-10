#include "traffic/domain/traffic_decision.h"

#include <array>
#include <utility>

namespace traffic::domain {
namespace {

constexpr std::array<std::pair<TrafficAction, std::string_view>, 6> kActionNames{{
    {TrafficAction::go, "GO"},
    {TrafficAction::wait, "WAIT"},
    {TrafficAction::stop, "STOP"},
    {TrafficAction::replan, "REPLAN"},
    {TrafficAction::hold, "HOLD"},
    {TrafficAction::recover, "RECOVER"},
}};

using DecisionResult = core::Result<TrafficDecision, DomainError>;

}  // namespace

std::string_view to_string(TrafficAction action) noexcept {
    for (const auto& [value, name] : kActionNames) {
        if (value == action) {
            return name;
        }
    }
    return "WAIT";
}

std::optional<TrafficAction> traffic_action_from_string(std::string_view name) noexcept {
    for (const auto& [value, candidate] : kActionNames) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}

bool is_halting(TrafficAction action) noexcept {
    switch (action) {
        case TrafficAction::wait:
        case TrafficAction::stop:
        case TrafficAction::replan:
            return true;
        case TrafficAction::go:
        case TrafficAction::hold:
        case TrafficAction::recover:
            // `hold` and `recover` both move the robot — to a holding area and
            // along a recovery path respectively.
            return false;
    }
    return false;
}

bool is_stale(const TrafficDecision& decision, StateVersion current) noexcept {
    return decision.state_version.is_stale_against(current);
}

core::Result<TrafficDecision, DomainError> make_traffic_decision(core::TrafficDecisionId id,
                                                                 core::RobotId robot_id,
                                                                 TrafficAction action,
                                                                 std::string reason,
                                                                 core::TimePoint created_at,
                                                                 StateVersion state_version) {
    if (id.empty() || robot_id.empty()) {
        return DecisionResult::failure(DomainError::empty_id);
    }
    if (reason.empty()) {
        return DecisionResult::failure(DomainError::empty_id);
    }

    TrafficDecision decision;
    decision.id = std::move(id);
    decision.robot_id = std::move(robot_id);
    decision.action = action;
    decision.reason = std::move(reason);
    decision.created_at = created_at;
    decision.state_version = state_version;
    return DecisionResult::success(std::move(decision));
}

}  // namespace traffic::domain
