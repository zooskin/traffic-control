#pragma once

/// \file
/// Conflict. docs/24_DOMAIN_MODEL.md §15~16.
///
/// A conflict is a detected fact: two robots want something that cannot be
/// shared. It carries no decision — who yields is the priority manager's
/// answer, in Phase 8, and keeping detection separate from resolution is what
/// lets either be replaced without touching the other.

#include <optional>
#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"

namespace traffic::domain {

/// How two robots collide. docs/24_DOMAIN_MODEL.md §16.
enum class ConflictType {
    /// Both want the same node at overlapping times.
    node,

    /// Both want the same edge, travelling the same way.
    edge,

    /// Both want the same edge from opposite ends. The characteristic failure
    /// of a single-lane corridor and, untreated, an immediate deadlock —
    /// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8.
    head_on,

    /// Paths cross at an intersection without sharing an edge.
    crossing,

    /// Both want a corridor whose capacity is already taken.
    corridor,

    /// Both want a resource that is neither plainly a node nor an edge — a
    /// charging or loading area.
    resource,

    /// The same resource at times that overlap only once expected entry and
    /// exit are considered. Invisible without the timing on route segments.
    temporal,
};

[[nodiscard]] std::string_view to_string(ConflictType type) noexcept;
[[nodiscard]] std::optional<ConflictType> conflict_type_from_string(std::string_view name) noexcept;

/// How urgently a conflict needs resolving.
///
/// Provisional. docs/24_DOMAIN_MODEL.md §15 names the field but not its scale,
/// and the thresholds should come out of simulation rather than be guessed
/// here. Treat the ordering as meaningful and the exact levels as open.
enum class ConflictSeverity {
    /// Far enough ahead that ordinary priority handling will absorb it.
    low,

    /// Needs a decision this cycle.
    medium,

    /// Robots are close enough that only stopping one resolves it.
    high,
};

[[nodiscard]] std::string_view to_string(ConflictSeverity severity) noexcept;

/// Two robots contending for one resource.
struct Conflict {
    core::ConflictId id;

    /// The two parties. Order carries no meaning — see `involves`.
    core::RobotId robot_a;
    core::RobotId robot_b;

    /// What they are contending for.
    core::ResourceId resource_id;

    ConflictType type{ConflictType::resource};
    ConflictSeverity severity{ConflictSeverity::medium};

    core::TimePoint detected_at{core::kTimeOrigin};

    [[nodiscard]] friend bool operator==(const Conflict& lhs, const Conflict& rhs) noexcept {
        return lhs.id == rhs.id;
    }
};

/// True when \p robot is one of the two parties.
[[nodiscard]] bool involves(const Conflict& conflict, const core::RobotId& robot) noexcept;

/// The other party to \p robot. Empty when \p robot is not involved.
[[nodiscard]] std::optional<core::RobotId> counterpart(const Conflict& conflict,
                                                       const core::RobotId& robot);

/// Builds a Conflict, rejecting empty ids and a robot in conflict with itself.
[[nodiscard]] core::Result<Conflict, DomainError> make_conflict(core::ConflictId id,
                                                                core::RobotId robot_a,
                                                                core::RobotId robot_b,
                                                                core::ResourceId resource_id,
                                                                ConflictType type,
                                                                ConflictSeverity severity,
                                                                core::TimePoint detected_at);

}  // namespace traffic::domain
