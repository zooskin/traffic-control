#include "traffic/reservation/reservation_policy.h"

#include <limits>

namespace traffic::reservation {
namespace {

using PolicyResult = core::Result<ReservationPolicy, domain::DomainError>;
using WindowResult = core::Result<domain::TimeWindow, domain::DomainError>;

using PriorityValue = domain::Priority::value_type;

[[nodiscard]] double to_seconds(core::Duration duration) noexcept {
    return std::chrono::duration<double>(duration).count();
}

}  // namespace

core::Result<ReservationPolicy, domain::DomainError> make_reservation_policy(
    ReservationPolicy policy) {
    if (policy.entry_buffer < core::Duration::zero() ||
        policy.exit_buffer < core::Duration::zero()) {
        return PolicyResult::failure(domain::DomainError::negative_value);
    }
    if (policy.aging_per_second < 0.0) {
        // Ageing downwards would make a robot lose ground by waiting, which is
        // the opposite of what §17 asks the term to do.
        return PolicyResult::failure(domain::DomainError::negative_value);
    }
    if (policy.horizon <= core::Duration::zero() ||
        policy.grant_timeout <= core::Duration::zero()) {
        return PolicyResult::failure(domain::DomainError::non_positive_value);
    }
    return PolicyResult::success(policy);
}

core::Result<domain::TimeWindow, domain::DomainError> effective_window(
    core::TimePoint start, core::Duration duration, const ReservationPolicy& policy) {
    if (duration < core::Duration::zero()) {
        return WindowResult::failure(domain::DomainError::negative_value);
    }
    return domain::make_time_window(start - policy.entry_buffer,
                                    start + duration + policy.exit_buffer);
}

domain::Priority effective_priority(domain::Priority base,
                                    core::Duration waited,
                                    const ReservationPolicy& policy) noexcept {
    if (policy.aging_per_second <= 0.0 || waited <= core::Duration::zero()) {
        return base;
    }

    const double raised =
        static_cast<double>(base.value()) + policy.aging_per_second * to_seconds(waited);

    constexpr double kCeiling = static_cast<double>(std::numeric_limits<PriorityValue>::max());
    if (raised >= kCeiling) {
        return domain::Priority{std::numeric_limits<PriorityValue>::max()};
    }
    return domain::Priority{static_cast<PriorityValue>(raised)};
}

}  // namespace traffic::reservation
