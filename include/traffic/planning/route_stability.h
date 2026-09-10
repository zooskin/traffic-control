#pragma once

/// \file
/// When a better route is not worth taking.
/// docs/05_GLOBAL_ROUTING.md §16~17.
///
/// A planner that adopts every improvement it finds oscillates. Congestion on
/// the main aisle makes the side aisle cheaper, the robots that switch make the
/// side aisle the congested one, and the routes swap back. The fleet spends its
/// time re-reserving resources instead of moving.
///
/// Two independent brakes, because they stop different things:
///
///   minimum_improvement  a new route must be enough cheaper to be worth the
///                        switch. Stops a route changing over rounding.
///   minimum_hold_time    a route may not be replaced too soon after it was
///                        adopted. Stops A -> B -> A within a few seconds even
///                        when each step really is an improvement.
///
/// The second is the one that matters. Improvement alone cannot detect a cycle:
/// every step of an oscillation is an improvement at the moment it is taken.
///
/// This is policy, not planning, so it is a value the planner is configured
/// with rather than something compiled into the search. docs/15 will tune the
/// numbers against the simulation; the defaults here are §16's own example.

#include <string_view>

#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/route.h"

namespace traffic::planning {

/// docs/05_GLOBAL_ROUTING.md §16~17.
struct RouteStabilityPolicy {
    /// Fraction the new route must beat the old one by.
    ///
    ///     new_cost < old_cost * (1 - minimum_improvement)
    ///
    /// 0.10 is the value §16 gives as its example. It is a starting point for
    /// the benchmark, not a measured result.
    double minimum_improvement{0.10};

    /// How long a route is held before it may be replaced. §17.
    core::Duration minimum_hold_time{core::Duration::zero()};

    [[nodiscard]] friend bool operator==(const RouteStabilityPolicy&,
                                         const RouteStabilityPolicy&) = default;
};

/// A policy that adopts any valid alternative immediately.
///
/// Correct when the current route has become *impossible* rather than merely
/// expensive: a corridor closed under the robot is not a case for hysteresis.
[[nodiscard]] RouteStabilityPolicy always_replan_policy() noexcept;

/// Validates a policy. Improvement must be in [0, 1) and hold time
/// non-negative.
///
/// An improvement of 1 or more would demand a free route and no replan would
/// ever happen — the fleet would keep driving into a blocked aisle.
[[nodiscard]] core::Result<RouteStabilityPolicy, domain::DomainError> make_stability_policy(
    RouteStabilityPolicy policy);

/// Why a replan was or was not adopted. Goes into the log record of §23.
enum class ReplanDecision {
    /// The current route is no longer usable, so the alternative is taken
    /// whatever it costs. Checked first: a blocked route must never be kept
    /// because the alternative was not 10% better.
    adopted_current_invalid,

    /// Cheaper by more than the threshold, and old enough to replace.
    adopted_improved,

    /// Not enough cheaper to be worth the switch. §16.
    kept_insufficient_improvement,

    /// Cheap enough, but the current route is too fresh. §17.
    kept_within_hold_time,
};

[[nodiscard]] std::string_view to_string(ReplanDecision decision) noexcept;

/// True when the decision means the new route is taken.
[[nodiscard]] bool is_adopted(ReplanDecision decision) noexcept;

/// Decides whether \p candidate_cost should replace the route costing
/// \p current_cost.
///
/// \p current_valid is whether the route in hand can still be driven — see
/// route_validator.h. When it cannot, the policy is bypassed entirely.
/// \p held_for is how long the current route has been in effect.
[[nodiscard]] ReplanDecision decide_replan(const RouteStabilityPolicy& policy,
                                           double current_cost,
                                           double candidate_cost,
                                           bool current_valid,
                                           core::Duration held_for) noexcept;

/// How long \p route has been in effect at \p now. Zero if it was created in
/// the future, which a replayed log can produce.
[[nodiscard]] core::Duration route_age(const domain::Route& route, core::TimePoint now) noexcept;

}  // namespace traffic::planning
