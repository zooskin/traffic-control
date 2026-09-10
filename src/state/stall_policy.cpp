#include "traffic/state/stall_policy.h"

namespace traffic::state {
namespace {

using PolicyResult = core::Result<StallPolicy, domain::DomainError>;

}  // namespace

core::Result<StallPolicy, domain::DomainError> make_stall_policy(StallPolicy policy) {
    if (policy.stop_timeout <= core::Duration::zero()) {
        // A zero T1 means every stop is instantly a blockage, which removes
        // the state that exists to absorb someone walking past.
        return PolicyResult::failure(domain::DomainError::non_positive_value);
    }
    if (policy.minimum_progress < 0.0) {
        return PolicyResult::failure(domain::DomainError::negative_value);
    }
    if (policy.blocked_timeout.has_value() && *policy.blocked_timeout <= policy.stop_timeout) {
        // T2 <= T1 leaves no band in which a robot is merely blocked, so an
        // ordinary pause escalates straight to needing intervention.
        return PolicyResult::failure(domain::DomainError::non_positive_value);
    }
    return PolicyResult::success(policy);
}

std::string_view to_string(StallVerdict verdict) noexcept {
    switch (verdict) {
        case StallVerdict::progressing:
            return "PROGRESSING";
        case StallVerdict::temporarily_stopped:
            return "TEMPORARILY_STOPPED";
        case StallVerdict::blocked:
            return "BLOCKED";
        case StallVerdict::failed:
            return "FAILED";
    }
    return "UNKNOWN";
}

std::optional<domain::RobotState> state_for(StallVerdict verdict) noexcept {
    switch (verdict) {
        case StallVerdict::progressing:
            return std::nullopt;
        case StallVerdict::temporarily_stopped:
            return domain::RobotState::temporarily_stopped;
        case StallVerdict::blocked:
            return domain::RobotState::blocked;
        case StallVerdict::failed:
            return domain::RobotState::failed;
    }
    return std::nullopt;
}

bool has_progressed(const StallPolicy& policy,
                    const domain::Position& from,
                    const domain::Position& to) noexcept {
    return domain::distance(from, to) >= policy.minimum_progress;
}

core::Duration stalled_for(const ProgressMark& mark, core::TimePoint now) noexcept {
    if (now <= mark.at) {
        return core::Duration::zero();
    }
    return now - mark.at;
}

StallVerdict assess_stall(const StallPolicy& policy, core::Duration stalled) noexcept {
    // Ordered from the worst outcome down, so a duration past both thresholds
    // cannot come back as merely blocked.
    if (policy.blocked_timeout.has_value() && stalled >= *policy.blocked_timeout) {
        return StallVerdict::failed;
    }
    if (stalled >= policy.stop_timeout) {
        return StallVerdict::blocked;
    }
    return StallVerdict::temporarily_stopped;
}

StallVerdict assess_progress(const StallPolicy& policy,
                             const ProgressMark& mark,
                             const domain::Position& position,
                             core::TimePoint now) noexcept {
    if (has_progressed(policy, mark.position, position)) {
        return StallVerdict::progressing;
    }
    return assess_stall(policy, stalled_for(mark, now));
}

}  // namespace traffic::state
