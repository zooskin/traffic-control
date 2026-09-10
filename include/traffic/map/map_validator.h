#pragma once

/// \file
/// Map validation. docs/03_MAP_GRAPH.md §17.
///
/// A map is loaded once and then every routing decision for the rest of the
/// run trusts it. A duplicate node id or an edge pointing at a node that does
/// not exist will not announce itself at load time — it surfaces hours later as
/// a robot that cannot be routed, or worse, as two robots granted what the map
/// says are different resources.
///
/// So validation reports *every* problem it finds rather than stopping at the
/// first. Fixing a site map one error per run is not a workable loop.
///
/// Connectivity is reported as a warning, not an error. A map with an
/// unreachable maintenance spur is odd but usable; refusing to load it would
/// block a site over something harmless.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "traffic/map/map.h"

namespace traffic::map {

/// How much a finding matters.
enum class IssueSeverity {
    /// The map cannot be used as it stands.
    error,

    /// Usable, but probably not what was intended.
    warning,
};

[[nodiscard]] std::string_view to_string(IssueSeverity severity) noexcept;

/// What kind of problem was found. One value per check in
/// docs/03_MAP_GRAPH.md §17.
enum class IssueKind {
    duplicate_node_id,
    duplicate_edge_id,
    duplicate_resource_id,

    /// An edge names a node the map does not contain.
    dangling_edge_endpoint,

    /// A resource names an edge or node the map does not contain.
    dangling_resource_member,

    invalid_node_capacity,
    invalid_edge_length,
    invalid_edge_speed_limit,
    invalid_edge_capacity,
    invalid_resource_capacity,

    /// An edge whose ends are the same node.
    self_loop_edge,

    /// A node no edge touches.
    orphan_node,

    /// A node that exists but cannot be reached from the rest of the map.
    unreachable_node,

    /// The graph falls into more than one piece.
    disconnected_graph,

    /// A corridor whose edges do not form a connected run between its ends.
    corridor_not_contiguous,

    /// A conflict group naming a movement the intersection does not define.
    unknown_movement_in_conflict_group,
};

[[nodiscard]] std::string_view to_string(IssueKind kind) noexcept;

/// One problem found in a map.
struct ValidationIssue {
    IssueKind kind;
    IssueSeverity severity;

    /// The node, edge or resource the finding is about, as a readable id.
    std::string subject;

    /// What is wrong, in a sentence.
    std::string detail;
};

/// Everything found in one pass.
class ValidationReport {
public:
    void add(IssueKind kind, IssueSeverity severity, std::string subject, std::string detail);

    [[nodiscard]] const std::vector<ValidationIssue>& issues() const noexcept { return issues_; }

    /// True when nothing was found that prevents the map being used.
    /// Warnings do not make a map invalid.
    [[nodiscard]] bool is_valid() const noexcept;

    [[nodiscard]] std::size_t error_count() const noexcept;
    [[nodiscard]] std::size_t warning_count() const noexcept;

    /// The findings, one per line, for logs and loader failures.
    [[nodiscard]] std::string to_string() const;

private:
    std::vector<ValidationIssue> issues_;
};

/// Checks the parts of a map before a Map is built from them.
///
/// Takes MapData rather than Map because several checks — duplicate ids above
/// all — are about the raw lists. Building the Map first would silently
/// collapse duplicates into the index and hide exactly what is being looked
/// for.
[[nodiscard]] ValidationReport validate(const MapData& data);

/// Validates, then builds. The report comes back on failure so the caller can
/// say what was wrong rather than only that something was.
[[nodiscard]] core::Result<Map, ValidationReport> make_map(MapData data);

}  // namespace traffic::map
