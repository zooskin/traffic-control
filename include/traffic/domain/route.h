#pragma once

/// \file
/// Route and its segments. docs/24_DOMAIN_MODEL.md §11~12.
///
/// A route answers "where should this robot go". It does *not* answer "when may
/// it go" — that is the traffic controller's, and the separation is the central
/// architectural principle of docs/00_MASTER_PLAN.md §4.1.
///
/// Each segment carries an expected entry and exit time. Those estimates are
/// what make temporal conflict detection possible: two routes that use the same
/// resource at disjoint times do not conflict, and without the timing they
/// would appear to.
///
/// A route records the map version it was planned against. Committing a route
/// computed on an older map is exactly the stale-result failure
/// docs/23_SYSTEM_ARCHITECTURE.md §16 requires to be caught before commit.

#include <cstddef>
#include <optional>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/errors.h"
#include "traffic/domain/values.h"

namespace traffic::domain {

/// One edge traversal within a route.
struct RouteSegment {
    core::EdgeId edge_id;

    /// The node this segment starts at.
    core::NodeId from_node;

    /// The node it ends at.
    core::NodeId to_node;

    /// Position in the route, starting at 0.
    std::size_t sequence{0};

    /// When the robot is expected to enter and leave. Estimates, not promises;
    /// reservations are derived from them but are a separate commitment.
    std::optional<TimeWindow> expected_window;

    [[nodiscard]] friend bool operator==(const RouteSegment&, const RouteSegment&) = default;
};

/// An ordered path for one robot.
struct Route {
    core::RouteId id;
    core::RobotId robot_id;

    /// Nodes visited, in order. Always one more than there are segments.
    std::vector<core::NodeId> nodes;

    /// Edge traversals, in order.
    std::vector<RouteSegment> segments;

    /// The map this route was planned against.
    MapVersion map_version;

    core::TimePoint created_at{core::kTimeOrigin};

    /// When the route stops being valid. Empty means it does not expire.
    std::optional<core::TimePoint> expires_at;

    [[nodiscard]] friend bool operator==(const Route& lhs, const Route& rhs) noexcept {
        // docs/24_DOMAIN_MODEL.md §33: entities compare by identity.
        return lhs.id == rhs.id;
    }
};

/// Where the route starts. Empty for an empty route.
[[nodiscard]] std::optional<core::NodeId> start_node(const Route& route);

/// Where the route ends. Empty for an empty route.
[[nodiscard]] std::optional<core::NodeId> goal_node(const Route& route);

/// True when the route has no segments — the robot is already at its goal.
[[nodiscard]] bool is_trivial(const Route& route) noexcept;

/// True when \p route was planned against a map older than \p current.
///
/// docs/23_SYSTEM_ARCHITECTURE.md §16: check this before committing, not after.
[[nodiscard]] bool is_stale_against_map(const Route& route, MapVersion current) noexcept;

/// True when the route has expired at \p now.
[[nodiscard]] bool is_expired(const Route& route, core::TimePoint now) noexcept;

/// The resources this route will occupy, in order, one per segment.
///
/// Note these are edge ids, not resource ids: mapping an edge to the corridor
/// that owns it needs the map, which the domain does not have. Phase 2 supplies
/// that lookup.
[[nodiscard]] std::vector<core::EdgeId> traversed_edges(const Route& route);

/// Builds a Route, checking the invariants of docs/24_DOMAIN_MODEL.md §34.
///
/// Rejects an empty id, an empty node list, and — the check that matters —
/// nodes and segments that do not describe one connected walk. A route whose
/// segments do not join up would be reserved resource by resource and then
/// fail on the first move the robot could not make.
[[nodiscard]] core::Result<Route, DomainError> make_route(core::RouteId id,
                                                          core::RobotId robot_id,
                                                          std::vector<core::NodeId> nodes,
                                                          std::vector<RouteSegment> segments,
                                                          MapVersion map_version,
                                                          core::TimePoint created_at);

}  // namespace traffic::domain
