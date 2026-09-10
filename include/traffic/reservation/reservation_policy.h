#pragma once

/// \file
/// The numbers a reservation decision depends on.
/// docs/06_TRAFFIC_RESERVATION.md §13, §17, §22, §29.
///
/// Every value here trades safety margin against throughput, and the right
/// trade-off is a property of a site rather than of this code — §13 and §29
/// both say so explicitly ("값은 configuration으로 관리한다"). Nothing below is
/// a constant.
///
/// The buffers default to the numbers §13 gives. The aging factor does not:
/// §17 names the term but docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §15 says the
/// coefficient comes out of benchmarking, which is Phase 15. A number invented
/// here would be one that later measurement has to argue with, so the default
/// is zero — no aging until a site configures it — while the mechanism itself
/// is present and tested.

#include <chrono>
#include <cstddef>

#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/values.h"

namespace traffic::reservation {

/// docs/06_TRAFFIC_RESERVATION.md §13.
inline constexpr core::Duration kDefaultEntryBuffer =
    std::chrono::duration_cast<core::Duration>(core::Milliseconds{500});

/// docs/06_TRAFFIC_RESERVATION.md §13.
inline constexpr core::Duration kDefaultExitBuffer =
    std::chrono::duration_cast<core::Duration>(core::Seconds{1});

/// docs/06_TRAFFIC_RESERVATION.md §29.
inline constexpr core::Duration kDefaultHorizon =
    std::chrono::duration_cast<core::Duration>(core::Seconds{10});

/// How long a grant may go unused before it is taken back.
/// docs/06_TRAFFIC_RESERVATION.md §22.
inline constexpr core::Duration kDefaultGrantTimeout =
    std::chrono::duration_cast<core::Duration>(core::Seconds{30});

struct ReservationPolicy {
    /// Subtracted from the requested start. Covers position error and the
    /// delay between deciding and the robot acting on it — §13.
    core::Duration entry_buffer{kDefaultEntryBuffer};

    /// Added to the expected exit. Larger than the entry buffer by default,
    /// because a robot that leaves late is the dangerous direction: the next
    /// robot is already moving towards the resource.
    core::Duration exit_buffer{kDefaultExitBuffer};

    /// How far ahead of now a reservation may start. §28~29: reserving a whole
    /// route locks resources nobody is near, which shows up as traffic that
    /// stops for robots that are not there yet.
    core::Duration horizon{kDefaultHorizon};

    /// How long a granted-but-unentered reservation survives. §22.
    core::Duration grant_timeout{kDefaultGrantTimeout};

    /// Priority points added per second of waiting. §17, and
    /// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §15.
    double aging_per_second{0.0};

    /// How many decisions the manager keeps for §36. Zero disables the log.
    std::size_t decision_log_capacity{1024};

    [[nodiscard]] friend bool operator==(const ReservationPolicy&,
                                         const ReservationPolicy&) = default;
};

/// Validates a policy.
///
/// Rejects negative buffers, a non-positive horizon or grant timeout, and a
/// negative aging factor. A negative aging factor would make a robot lose
/// priority the longer it waited, which is starvation with extra steps.
[[nodiscard]] core::Result<ReservationPolicy, domain::DomainError> make_reservation_policy(
    ReservationPolicy policy);

/// The window a reservation actually has to hold, buffers included. §13.
///
///     [start - entry_buffer, start + duration + exit_buffer)
///
/// This, not the requested span, is what conflict checks run against. The
/// buffer only protects anything if it is part of what other robots are kept
/// out of.
[[nodiscard]] core::Result<domain::TimeWindow, domain::DomainError> effective_window(
    core::TimePoint start, core::Duration duration, const ReservationPolicy& policy);

/// Base priority raised by how long the robot has been waiting. §17.
///
///     effective = base + aging_per_second * waited
///
/// Saturates rather than wrapping: an overflowed priority would invert the
/// ordering and hand the resource to the robot that had waited longest for the
/// shortest possible time.
///
/// Phase 8 owns priority policy (docs/09_PRIORITY_MANAGER.md, IPriorityManager).
/// This is the minimum §17 and §39's `test_priority_aging` require of Phase 6,
/// kept as one free function so that replacing it with the priority manager is
/// a call-site change and not a rewrite.
[[nodiscard]] domain::Priority effective_priority(domain::Priority base,
                                                  core::Duration waited,
                                                  const ReservationPolicy& policy) noexcept;

}  // namespace traffic::reservation
