#include "traffic/state/state_change.h"

#include <sstream>

namespace traffic::state {

bool is_stall_onset(const RobotStateChange& change) noexcept {
    // The robot was getting somewhere and now is not. A self-transition does
    // not count: a robot that was already blocked and is reported blocked
    // again has not just stalled, and counting it would make the alert climb
    // for as long as nothing changed.
    return !domain::is_halted(change.from) && domain::is_halted(change.to);
}

bool is_recovery(const RobotStateChange& change) noexcept {
    return domain::is_halted(change.from) && change.to == domain::RobotState::moving;
}

std::string to_log_line(const RobotStateChange& change) {
    std::ostringstream line;
    line << "robot=" << change.robot_id.value() << " from=" << domain::to_string(change.from)
         << " to=" << domain::to_string(change.to) << " version=" << change.version
         << " reason=" << (change.reason.has_value() ? to_string(*change.reason) : "-")
         << " resource=" << (change.resource.has_value() ? change.resource->value() : "-");
    return line.str();
}

}  // namespace traffic::state
