#pragma once

/// \file
/// The individual rules that decide whether two robots collide.
/// docs/24_DOMAIN_MODEL.md §15~16, docs/06_TRAFFIC_RESERVATION.md §7~11,
/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §7~8 and §11~12.
///
/// Separated from the detector so that each rule can be read and tested on its
/// own. The detector decides *what to compare*; this file decides *whether a
/// comparison is a conflict*. Nothing here touches the map, a clock, or any
/// container the caller owns, which is what makes every rule a pure function of
/// its arguments and therefore reproducible.
///
/// Every rule stops at the fact. None of them says who yields:
/// docs/24_DOMAIN_MODEL.md §15 is explicit that a conflict carries no decision,
/// and resolution belongs to IPriorityManager in Phase 8.

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/conflict.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/reservation.h"
#include "traffic/domain/resources.h"
#include "traffic/domain/values.h"

namespace traffic::reservation {

/// The window an occupancy carries until something fills it in.
///
/// Unbounded rather than empty on purpose: a value that was never populated
/// then conflicts with everything instead of silently conflicting with nothing.
/// A missing claim that hides a collision is the one failure this module cannot
/// have.
inline constexpr domain::TimeWindow kUnboundedWindow{core::kTimeOrigin, core::kNeverExpires};

// ---------------------------------------------------------------- occupancies
//
// What the rules compare. These are observations derived from routes and
// reservations, not entities: they carry no identity and are rebuilt on every
// detection pass.

/// One robot's presence on one traffic resource for one window.
///
/// The shape every capacity question is asked in — docs/06 §7 counts
/// "overlapping reservations" against `resource.capacity` and does not care
/// what kind of resource it is.
struct ResourceOccupancy {
    core::RobotId robot_id;
    core::ResourceId resource_id;
    domain::TimeWindow window{kUnboundedWindow};

    [[nodiscard]] friend bool operator==(const ResourceOccupancy&,
                                         const ResourceOccupancy&) = default;
};

/// One robot standing at one node.
///
/// A node visit is instantaneous in a plan — the robot arrives at the end of
/// one segment and leaves at the start of the next — so the window here is
/// widened by the safety buffer of docs/06_TRAFFIC_RESERVATION.md §13. Without
/// that widening a node visit is a zero-length interval and no two of them ever
/// overlap.
struct NodeVisit {
    core::RobotId robot_id;
    core::NodeId node_id;
    domain::TimeWindow window{kUnboundedWindow};

    [[nodiscard]] friend bool operator==(const NodeVisit&, const NodeVisit&) = default;
};

/// One robot's traversal of one edge, with the end it entered by.
///
/// `from_node` is the whole reason this type exists rather than a plain
/// ResourceOccupancy: it separates two robots following each other from two
/// robots meeting head-on, and those need different answers.
struct EdgeTraversal {
    core::RobotId robot_id;
    core::EdgeId edge_id;

    /// The end the robot enters by.
    core::NodeId from_node;

    /// The end it leaves by.
    core::NodeId to_node;

    /// The resource that actually admits robots: the corridor owning the edge,
    /// or the edge itself. docs/03_MAP_GRAPH.md §20.
    core::ResourceId resource_id;

    domain::TimeWindow window{kUnboundedWindow};

    [[nodiscard]] friend bool operator==(const EdgeTraversal&, const EdgeTraversal&) = default;
};

/// Which way a robot runs a corridor.
///
/// docs/06_TRAFFIC_RESERVATION.md §10 makes direction part of a corridor
/// reservation, and this is why: a capacity count cannot tell a convoy from a
/// head-on meeting.
enum class PassageDirection {
    /// Along the corridor's declared entry_node -> exit_node order.
    entry_to_exit,

    /// Against it.
    exit_to_entry,
};

[[nodiscard]] std::string_view to_string(PassageDirection direction) noexcept;

/// One robot's passage through one corridor, from wherever it joined to
/// wherever it left.
struct CorridorPassage {
    core::RobotId robot_id;
    core::ResourceId corridor_id;

    core::NodeId entry_node;
    core::NodeId exit_node;

    PassageDirection direction{PassageDirection::entry_to_exit};

    domain::TimeWindow window{kUnboundedWindow};

    [[nodiscard]] friend bool operator==(const CorridorPassage&, const CorridorPassage&) = default;
};

/// One robot's way through one intersection.
struct IntersectionCrossing {
    core::RobotId robot_id;
    core::ResourceId intersection_id;

    /// The movement the map defines for the robot's entry and exit edges.
    ///
    /// Empty when the map defines none — a route that ends inside the
    /// intersection, or a turn nobody described. `is_crossing_conflict` says
    /// what that is taken to mean.
    std::optional<core::MovementId> movement_id;

    domain::TimeWindow window{kUnboundedWindow};

    [[nodiscard]] friend bool operator==(const IntersectionCrossing&,
                                         const IntersectionCrossing&) = default;
};

// ----------------------------------------------------------------- projection

[[nodiscard]] ResourceOccupancy as_occupancy(const NodeVisit& visit);
[[nodiscard]] ResourceOccupancy as_occupancy(const EdgeTraversal& traversal);
[[nodiscard]] ResourceOccupancy as_occupancy(const CorridorPassage& passage);
[[nodiscard]] ResourceOccupancy as_occupancy(const IntersectionCrossing& crossing);
[[nodiscard]] ResourceOccupancy as_occupancy(const domain::Reservation& reservation);

// ---------------------------------------------------------------------- rules

/// The span both windows cover. Empty when they do not overlap.
///
/// Half-open like `TimeWindow::overlaps`, so a window ending exactly where
/// another begins yields nothing — the handover docs/24_DOMAIN_MODEL.md §34
/// relies on is not a conflict.
[[nodiscard]] std::optional<domain::TimeWindow> overlap_of(const domain::TimeWindow& lhs,
                                                           const domain::TimeWindow& rhs) noexcept;

/// True when two different robots want one resource at overlapping times.
///
/// The rule of docs/06_TRAFFIC_RESERVATION.md §7, and capacity-blind: it says
/// the two claims meet, not that the resource cannot hold both. Pair it with
/// `is_capacity_conflict` wherever the capacity is known.
[[nodiscard]] bool is_temporal_conflict(const ResourceOccupancy& lhs,
                                        const ResourceOccupancy& rhs) noexcept;

/// True when \p lhs and \p rhs meet *and* the resource cannot hold everyone
/// present while they do.
///
/// docs/06_TRAFFIC_RESERVATION.md §7~8: a conflict is not an overlap, it is
/// "overlapping reservations > resource.capacity". With capacity 1 any two
/// overlapping holders qualify; above that, the pair is in conflict only if at
/// some instant they share, the number of *distinct* robots present exceeds the
/// capacity. Distinctness matters — one robot holding two adjacent windows of
/// the same corridor is one occupant, not two.
///
/// \p holders must list every occupancy of the resource, \p lhs and \p rhs
/// included; occupancies of other resources are ignored. The scan is quadratic
/// in the holders of a single resource, which is acceptable while capacity 1 —
/// the short-circuited case — remains the overwhelming majority. Revisit under
/// docs/22_IMPLEMENTATION_WORKFLOW.md Phase 15 if that stops being true.
[[nodiscard]] bool is_capacity_conflict(const ResourceOccupancy& lhs,
                                        const ResourceOccupancy& rhs,
                                        std::span<const ResourceOccupancy> holders,
                                        std::uint32_t capacity);

/// True when two robots stand at the same node at overlapping times.
[[nodiscard]] bool is_node_conflict(const NodeVisit& lhs, const NodeVisit& rhs) noexcept;

/// True when two robots run the same edge the same way at overlapping times.
[[nodiscard]] bool is_following_conflict(const EdgeTraversal& lhs,
                                         const EdgeTraversal& rhs) noexcept;

/// True when two robots enter the same edge from opposite ends at overlapping
/// times. docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8.
[[nodiscard]] bool is_head_on_conflict(const EdgeTraversal& lhs, const EdgeTraversal& rhs) noexcept;

/// How two traversals of one edge collide, or empty when they do not.
///
/// One call rather than two, because the two edge rules are exhaustive and
/// mutually exclusive: robots on one edge entered either by the same end or by
/// opposite ones.
[[nodiscard]] std::optional<domain::ConflictType> classify_edge_conflict(
    const EdgeTraversal& lhs, const EdgeTraversal& rhs) noexcept;

/// True when two passages run \p corridor in opposite directions at overlapping
/// times.
///
/// Only for corridors that `can_deadlock_head_on`: both directions legal, one
/// robot at a time. A wide bidirectional aisle lets robots pass, so opposing
/// traffic there is a capacity question and not this one. This is the case
/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8 draws, and the reason the robots
/// need not share an edge to be in trouble: they can be one edge apart inside
/// the same single-lane passage and still have nowhere to go.
[[nodiscard]] bool is_head_on_conflict(const domain::Corridor& corridor,
                                       const CorridorPassage& lhs,
                                       const CorridorPassage& rhs) noexcept;

/// True when two robots cannot be inside \p intersection together.
///
/// Membership of a conflict group decides, not occupancy:
/// docs/06_TRAFFIC_RESERVATION.md §8 says so in as many words, and
/// docs/03_MAP_GRAPH.md §11 explains why — movements in different groups are
/// exactly the ones that may run at the same time, which a capacity count
/// cannot express.
///
/// Fail-closed on a movement the map does not define. A way through that nobody
/// described cannot be shown to be compatible with anything, and guessing in
/// the safe direction costs a wait while guessing the other way costs a
/// collision.
[[nodiscard]] bool is_crossing_conflict(const domain::Intersection& intersection,
                                        const IntersectionCrossing& lhs,
                                        const IntersectionCrossing& rhs) noexcept;

// ------------------------------------------------------------ classification

/// Which classification survives when several rules describe the same two
/// robots contending for the same resource. Higher wins.
///
/// Two robots meeting inside a single-lane corridor trip the corridor rule, the
/// edge rule and the generic resource rule, all three naming one corridor. That
/// is one fact, and reporting it three times would have a priority manager
/// resolve the same contention three times over. The order runs from the most
/// specific statement about *why* the resource cannot be shared to the least.
[[nodiscard]] int conflict_type_precedence(domain::ConflictType type) noexcept;

// ---------------------------------------------------------------- severity

/// When a conflict stops being something ordinary priority handling absorbs.
///
/// docs/24_DOMAIN_MODEL.md §15 names `severity` without defining a scale, and
/// domain/conflict.h says the same: the levels are provisional and the numbers
/// belong to simulation rather than to a guess made here. So the thresholds are
/// configuration with defaults that are only a starting point, and the mapping
/// is deliberately one-dimensional — how much lead time is left before the two
/// robots want the same thing. That is the quantity the three levels in
/// domain/conflict.h are written about.
struct SeverityThresholds {
    /// At or beyond this much warning, `low`: there is room for the ordinary
    /// priority decision to play out before anything has to stop.
    core::Duration absorbed_lead{std::chrono::duration_cast<core::Duration>(core::Seconds{10})};

    /// Below this much warning, `high`: too close for a different route to
    /// help, so only stopping a robot resolves it.
    ///
    /// Two seconds is about one control cycle plus one command round trip at
    /// the latencies CLAUDE.md targets. It is a placeholder for a measurement.
    core::Duration critical_lead{std::chrono::duration_cast<core::Duration>(core::Seconds{2})};

    [[nodiscard]] friend bool operator==(const SeverityThresholds&,
                                         const SeverityThresholds&) = default;
};

/// Validates thresholds.
///
/// Rejects a negative lead and a critical lead that is not strictly below the
/// absorbed one. With the two crossed the `medium` band is empty, and every
/// conflict then reads as either ignorable or unavoidable — the one shape the
/// scale must not have, because nothing is ever merely due this cycle.
[[nodiscard]] core::Result<SeverityThresholds, domain::DomainError> make_severity_thresholds(
    SeverityThresholds thresholds);

/// How urgent a conflict beginning at \p overlap is, seen from \p now.
///
/// Head-on never reports `low`, however far off it is. The other types describe
/// a resource two robots may take in turn, and time is exactly what resolves
/// them; a head-on inside a single-lane corridor is not resolved by waiting,
/// because once both robots have entered no ordering of them exists
/// (docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8). The decision has to be taken
/// before the second robot enters, so it is never merely something to watch.
[[nodiscard]] domain::ConflictSeverity severity_for(domain::ConflictType type,
                                                    const domain::TimeWindow& overlap,
                                                    core::TimePoint now,
                                                    const SeverityThresholds& thresholds) noexcept;

// ------------------------------------------------------------------ identity

/// A conflict id that depends only on what the conflict *is*.
///
/// No counter and no clock: docs/01_REQUIREMENTS.md NFR-003 wants identical
/// input to produce identical output, and an id handed out in discovery order
/// would change whenever the routes were presented in a different order. The
/// robot pair is sorted inside, so one contention has one id however the two
/// robots are passed in — which is also what stops A-versus-B and B-versus-A
/// being counted as two conflicts.
///
/// Readable rather than hashed, as docs/24_DOMAIN_MODEL.md §26 asks:
/// `CONF-HEAD_ON-CORRIDOR-01-R01-R02-15000000000`.
[[nodiscard]] core::ConflictId make_conflict_id(std::string_view prefix,
                                                domain::ConflictType type,
                                                const core::ResourceId& resource,
                                                const core::RobotId& first,
                                                const core::RobotId& second,
                                                core::TimePoint overlap_start);

}  // namespace traffic::reservation
