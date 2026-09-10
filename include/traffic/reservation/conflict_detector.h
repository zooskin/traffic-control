#pragma once

/// \file
/// Conflict detection. docs/22_IMPLEMENTATION_WORKFLOW.md Phase 6 ("Conflict
/// Detection", Phase 7 of the confirmed order in docs/00_INDEX.md D-003),
/// docs/24_DOMAIN_MODEL.md §15~16, docs/06_TRAFFIC_RESERVATION.md §7~11,
/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §8 and §12.
///
/// A pure detector. It is handed routes, reservations and the map, and it
/// answers with the conflicts it can see. It stores nothing between calls and
/// it decides nothing: which robot yields is IPriorityManager's answer in Phase
/// 8, and docs/24_DOMAIN_MODEL.md §15 keeps that out of the Conflict entity on
/// purpose. Keeping detection stateless is also what lets the traffic
/// controller, the replanner and a simulation run all ask the same question
/// without agreeing on an order to ask it in.
///
/// The flow is the one docs/22 Phase 6 draws — route plus reservation in,
/// conflict list out. Robot state is deliberately absent: a robot's reported
/// position tells us where it is, not what it intends, and everything this file
/// compares is intent.
///
/// Detection runs in passes, one per rule in conflict_rules.h, and the
/// candidates they produce are then reduced to one conflict per pair of robots
/// per contended resource. Several passes describing the same corridor are
/// describing one fact; see `conflict_type_precedence`.

#include <chrono>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "traffic/core/clock.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/conflict.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/reservation.h"
#include "traffic/domain/route.h"
#include "traffic/map/map.h"
#include "traffic/reservation/conflict_rules.h"

namespace traffic::reservation {

/// The margin added around a node visit before it is compared with another.
/// docs/06_TRAFFIC_RESERVATION.md §13.
///
/// A plan has a robot leaving a node at the same instant it arrives, so without
/// a margin the node it passes through is a zero-length interval that can never
/// overlap anything. §13 anticipates exactly this and makes the buffer
/// configuration; the defaults here are the worked example it gives.
///
/// Deliberately not applied to edge traversals. An edge traversal already lasts
/// as long as the robot is on the edge, so the segment window covers the fact;
/// widening it as well would report every convoy that ever shares an aisle.
struct SafetyBuffer {
    /// How early a robot is treated as having arrived.
    core::Duration entry{std::chrono::duration_cast<core::Duration>(core::Milliseconds{500})};

    /// How late it is treated as having left.
    core::Duration exit{std::chrono::duration_cast<core::Duration>(core::Milliseconds{1000})};

    [[nodiscard]] friend bool operator==(const SafetyBuffer&, const SafetyBuffer&) = default;
};

/// Validates a buffer. Rejects a negative margin, which would narrow a node
/// visit instead of widening it and hide the conflicts this exists to expose.
[[nodiscard]] core::Result<SafetyBuffer, domain::DomainError> make_safety_buffer(
    SafetyBuffer buffer);

/// How a detector is set up.
struct ConflictDetectorConfig {
    SeverityThresholds severity{};
    SafetyBuffer node_buffer{};

    /// Leads every generated conflict id, so conflicts from one run are
    /// recognisable in a log that carries several.
    std::string id_prefix{"CONF"};

    [[nodiscard]] friend bool operator==(const ConflictDetectorConfig&,
                                         const ConflictDetectorConfig&) = default;
};

/// Validates a configuration, including the parts it is made of.
[[nodiscard]] core::Result<ConflictDetectorConfig, domain::DomainError>
make_conflict_detector_config(ConflictDetectorConfig config);

/// What to examine.
///
/// Views, not copies: a detection pass is read-only and runs against state the
/// caller already holds. Both spans must outlive the call.
struct ConflictQuery {
    /// The routes robots intend to drive.
    std::span<const domain::Route> routes{};

    /// Reservations already requested or granted. Those that no longer withhold
    /// their resource (`holds_resource`) are ignored.
    std::span<const domain::Reservation> reservations{};
};

// ------------------------------------------------------------- route reading
//
// Free functions rather than members: turning a route into occupancies needs
// the map and nothing else, and the reservation manager and the replanner will
// both want it without holding a detector.

/// The edges \p route traverses, with the timing the conflict rules need.
///
/// Segments with no `expected_window` are skipped rather than guessed at. An
/// invented window would be compared against real ones and produce conflicts
/// that exist only in the estimate.
[[nodiscard]] std::vector<EdgeTraversal> edge_traversals(const map::Map& map,
                                                         const domain::Route& route);

/// The nodes \p route stands at, widened by \p buffer.
///
/// Nodes belonging to an intersection are left out: inside an intersection it
/// is movements that decide, not occupancy (docs/06_TRAFFIC_RESERVATION.md §8),
/// and two robots crossing on compatible movements share the crossing node by
/// design. Reporting that as a node conflict would make every workable
/// intersection look blocked. `intersection_crossings` covers those nodes.
[[nodiscard]] std::vector<NodeVisit> node_visits(const map::Map& map,
                                                 const domain::Route& route,
                                                 const SafetyBuffer& buffer);

/// The corridors \p route passes through, one entry per uninterrupted run of
/// segments inside the same corridor.
///
/// A run, not a set: a route that leaves a corridor and comes back later is two
/// passages, and merging them would claim the corridor for the gap in between.
[[nodiscard]] std::vector<CorridorPassage> corridor_passages(const map::Map& map,
                                                             const domain::Route& route);

/// The intersections \p route crosses, each with the movement the map defines
/// for the edges it enters and leaves by.
[[nodiscard]] std::vector<IntersectionCrossing> intersection_crossings(const map::Map& map,
                                                                       const domain::Route& route,
                                                                       const SafetyBuffer& buffer);

// -------------------------------------------------------------------- detector

/// Finds the conflicts in a set of routes and reservations.
///
/// An interface because the traffic controller of Phase 11 should not know
/// which rules are in force, in the same way it does not know which planner
/// computed a route (CLAUDE.md, Algorithm Isolation). A detector that consults
/// a reservation table, or one that only checks the next few seconds, replaces
/// this one without the controller changing.
class IConflictDetector {
public:
    IConflictDetector() = default;
    virtual ~IConflictDetector() = default;

    IConflictDetector(const IConflictDetector&) = delete;
    IConflictDetector& operator=(const IConflictDetector&) = delete;
    IConflictDetector(IConflictDetector&&) = delete;
    IConflictDetector& operator=(IConflictDetector&&) = delete;

    [[nodiscard]] virtual std::string_view name() const noexcept = 0;

    /// Every conflict in \p query, in a deterministic order.
    [[nodiscard]] virtual std::vector<domain::Conflict> detect(
        const ConflictQuery& query) const = 0;

    /// Every conflict between two routes.
    ///
    /// Not merely a convenience: the replanner asks about one candidate route
    /// against one other, and going through the whole-fleet entry point would
    /// have it build a container to hold two elements.
    [[nodiscard]] virtual std::vector<domain::Conflict> detect_between(
        const domain::Route& lhs, const domain::Route& rhs) const = 0;
};

/// The Phase 7 detector: every rule in conflict_rules.h, applied to everything.
///
/// Exhaustive rather than incremental. It compares every pair that shares a
/// resource, which is quadratic in the robots contending for one place. That is
/// the right shape for a detector whose answers are checked against a
/// simulation; docs/22_IMPLEMENTATION_WORKFLOW.md Phase 15 is where a narrower
/// one earns its place, and the interface above is what lets it be swapped in.
class ConflictDetector final : public IConflictDetector {
public:
    ConflictDetector(const map::Map& map, const core::IClock& clock);

    /// \p config is used as given. Validate it with
    /// `make_conflict_detector_config` first when it comes from a file or an
    /// operator rather than from code.
    ConflictDetector(const map::Map& map, const core::IClock& clock, ConflictDetectorConfig config);

    [[nodiscard]] std::string_view name() const noexcept override;

    [[nodiscard]] const ConflictDetectorConfig& config() const noexcept { return config_; }

    /// Compares every route with every other and every reservation with both.
    ///
    /// Output order is fixed by the conflict's own content — the robot pair,
    /// then the resource — and never by the order the routes arrived in.
    /// docs/01_REQUIREMENTS.md NFR-003: the same fleet state must produce the
    /// same conflict list, or no scenario in docs/26_TEST_SCENARIOS.md is
    /// reproducible.
    [[nodiscard]] std::vector<domain::Conflict> detect(const ConflictQuery& query) const override;

    [[nodiscard]] std::vector<domain::Conflict> detect_between(
        const domain::Route& lhs, const domain::Route& rhs) const override;

private:
    const map::Map& map_;
    const core::IClock& clock_;
    ConflictDetectorConfig config_;
};

}  // namespace traffic::reservation
