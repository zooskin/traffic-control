#pragma once

/// \file
/// Domain value objects.
///
/// docs/24_DOMAIN_MODEL.md §25 lists these alongside the identifier types:
/// Position, Velocity, Priority, and the version counters of §28.
///
/// docs/24_DOMAIN_MODEL.md §33: value objects compare by value, entities by
/// identity. Everything here compares by value.

#include <compare>
#include <cstdint>
#include <ostream>

#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"

namespace traffic::domain {

/// A point in space, in metres.
///
/// Traffic decisions are made on the graph, not on coordinates — this exists
/// for map geometry, logging and visualisation. Do not route on it.
///
/// `z` is present because docs/03_MAP_GRAPH.md §4 lists it, and a multi-floor
/// site needs it. It stays 0 in a single-floor warehouse.
struct Position {
    double x{0.0};
    double y{0.0};
    double z{0.0};

    [[nodiscard]] friend bool operator==(const Position&, const Position&) = default;
};

/// Straight-line distance between two points, in metres.
///
/// Lives here rather than in whichever module first wanted it, because more
/// than one does: the planner's heuristic measures how far the goal is, and
/// the state manager measures whether a robot has moved at all. Two copies of
/// this would be two chances to disagree about whether `z` counts.
[[nodiscard]] double distance(const Position& from, const Position& to) noexcept;

/// Planar velocity, in metres per second.
struct Velocity {
    double vx{0.0};
    double vy{0.0};

    [[nodiscard]] friend bool operator==(const Velocity&, const Velocity&) = default;
};

/// Traffic priority. Higher wins.
///
/// docs/24_DOMAIN_MODEL.md §17 distinguishes the inputs (base priority,
/// waiting time, deadline, blocking impact, task priority) from the single
/// effective priority they produce. This type is that scalar. The policy that
/// computes it is Phase 8 — see docs/09_PRIORITY_MANAGER.md — and lives behind
/// IPriorityManager, not here.
///
/// Ordering is total and deterministic, which docs/24_DOMAIN_MODEL.md §17
/// requires: the same inputs must always resolve the same way.
class Priority {
public:
    using value_type = std::int32_t;

    constexpr Priority() = default;
    constexpr explicit Priority(value_type value) noexcept : value_(value) {}

    [[nodiscard]] constexpr value_type value() const noexcept { return value_; }

    [[nodiscard]] friend constexpr bool operator==(Priority, Priority) = default;
    [[nodiscard]] friend constexpr std::strong_ordering operator<=>(Priority, Priority) = default;

    friend std::ostream& operator<<(std::ostream& os, Priority priority) {
        return os << priority.value_;
    }

private:
    value_type value_{0};
};

/// Lowest priority a robot can hold. Used as the starting point for aging.
inline constexpr Priority kLowestPriority{0};

/// A monotonically increasing counter used to detect stale data.
///
/// docs/23_SYSTEM_ARCHITECTURE.md §2.4: a planning result computed against
/// state version 100 must not be committed once the world has moved to 105.
/// docs/24_DOMAIN_MODEL.md §28 lists the entities that carry one.
///
/// \p Tag separates the counters so a map version cannot be compared against a
/// state version.
template<typename Tag>
class Version {
public:
    using value_type = std::uint64_t;

    constexpr Version() = default;
    constexpr explicit Version(value_type value) noexcept : value_(value) {}

    [[nodiscard]] constexpr value_type value() const noexcept { return value_; }

    /// The version that follows this one.
    [[nodiscard]] constexpr Version next() const noexcept { return Version{value_ + 1}; }

    /// True when \p other describes a world at least as new as this one.
    [[nodiscard]] constexpr bool is_stale_against(Version other) const noexcept {
        return value_ < other.value_;
    }

    [[nodiscard]] friend constexpr bool operator==(Version, Version) = default;
    [[nodiscard]] friend constexpr std::strong_ordering operator<=>(Version, Version) = default;

    friend std::ostream& operator<<(std::ostream& os, Version version) {
        return os << version.value_;
    }

private:
    value_type value_{0};
};

namespace tags {
struct StateVersion {};
struct MapVersion {};
}  // namespace tags

/// Version of the fleet's observed state. Bumped on every state update.
using StateVersion = Version<tags::StateVersion>;

/// Version of the traffic map. Bumped when topology or attributes change.
using MapVersion = Version<tags::MapVersion>;

/// A closed-open interval on the traffic timeline: `[start, end)`.
///
/// Reservations, route segments and blockages all describe a span of time, and
/// all of them need the same overlap test. docs/24_DOMAIN_MODEL.md §34 requires
/// start < end, which is checked by the factory rather than the constructor.
class TimeWindow {
public:
    /// \pre start < end. Use `make_time_window` to validate untrusted input.
    constexpr TimeWindow(core::TimePoint start, core::TimePoint end) noexcept
        : start_(start), end_(end) {}

    [[nodiscard]] constexpr core::TimePoint start() const noexcept { return start_; }
    [[nodiscard]] constexpr core::TimePoint end() const noexcept { return end_; }

    [[nodiscard]] constexpr core::Duration duration() const noexcept { return end_ - start_; }

    [[nodiscard]] constexpr bool contains(core::TimePoint point) const noexcept {
        return point >= start_ && point < end_;
    }

    /// True when the two windows share any instant.
    ///
    /// Half-open, so a window ending exactly when another starts does *not*
    /// overlap. That is what lets one robot hand a corridor to the next without
    /// a fabricated gap.
    [[nodiscard]] constexpr bool overlaps(const TimeWindow& other) const noexcept {
        return start_ < other.end_ && other.start_ < end_;
    }

    [[nodiscard]] friend constexpr bool operator==(const TimeWindow&, const TimeWindow&) = default;

private:
    core::TimePoint start_;
    core::TimePoint end_;
};

/// Builds a TimeWindow, rejecting an empty or inverted span.
///
/// docs/24_DOMAIN_MODEL.md §34 names `start_time < end_time` as a domain
/// invariant. A zero-length reservation would be granted and then instantly
/// expire, which reads in the logs as a resource that was never held.
[[nodiscard]] core::Result<TimeWindow, DomainError> make_time_window(core::TimePoint start,
                                                                     core::TimePoint end);

}  // namespace traffic::domain
