#include "traffic/map/map_validator.h"

#include <algorithm>
#include <array>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace traffic::map {
namespace {

constexpr std::array<std::pair<IssueKind, std::string_view>, 16> kIssueNames{{
    {IssueKind::duplicate_node_id, "DUPLICATE_NODE_ID"},
    {IssueKind::duplicate_edge_id, "DUPLICATE_EDGE_ID"},
    {IssueKind::duplicate_resource_id, "DUPLICATE_RESOURCE_ID"},
    {IssueKind::dangling_edge_endpoint, "DANGLING_EDGE_ENDPOINT"},
    {IssueKind::dangling_resource_member, "DANGLING_RESOURCE_MEMBER"},
    {IssueKind::invalid_node_capacity, "INVALID_NODE_CAPACITY"},
    {IssueKind::invalid_edge_length, "INVALID_EDGE_LENGTH"},
    {IssueKind::invalid_edge_speed_limit, "INVALID_EDGE_SPEED_LIMIT"},
    {IssueKind::invalid_edge_capacity, "INVALID_EDGE_CAPACITY"},
    {IssueKind::invalid_resource_capacity, "INVALID_RESOURCE_CAPACITY"},
    {IssueKind::self_loop_edge, "SELF_LOOP_EDGE"},
    {IssueKind::orphan_node, "ORPHAN_NODE"},
    {IssueKind::unreachable_node, "UNREACHABLE_NODE"},
    {IssueKind::disconnected_graph, "DISCONNECTED_GRAPH"},
    {IssueKind::corridor_not_contiguous, "CORRIDOR_NOT_CONTIGUOUS"},
    {IssueKind::unknown_movement_in_conflict_group, "UNKNOWN_MOVEMENT_IN_CONFLICT_GROUP"},
}};

/// Collects ids and reports the second and later sightings of each.
template<typename IdType, typename Range, typename Project>
void check_duplicates(ValidationReport& report,
                      const Range& items,
                      Project project,
                      IssueKind kind,
                      std::string_view what) {
    std::unordered_set<IdType> seen;
    for (const auto& item : items) {
        const IdType& id = project(item);
        if (!seen.insert(id).second) {
            report.add(kind,
                       IssueSeverity::error,
                       id.value(),
                       std::string{what} + " id appears more than once");
        }
    }
}

}  // namespace

std::string_view to_string(IssueSeverity severity) noexcept {
    return severity == IssueSeverity::error ? "ERROR" : "WARNING";
}

std::string_view to_string(IssueKind kind) noexcept {
    for (const auto& [value, name] : kIssueNames) {
        if (value == kind) {
            return name;
        }
    }
    return "UNKNOWN_MOVEMENT_IN_CONFLICT_GROUP";
}

void ValidationReport::add(IssueKind kind,
                           IssueSeverity severity,
                           std::string subject,
                           std::string detail) {
    issues_.push_back(ValidationIssue{kind, severity, std::move(subject), std::move(detail)});
}

bool ValidationReport::is_valid() const noexcept {
    return error_count() == 0;
}

std::size_t ValidationReport::error_count() const noexcept {
    return static_cast<std::size_t>(
        std::count_if(issues_.begin(), issues_.end(), [](const ValidationIssue& issue) {
            return issue.severity == IssueSeverity::error;
        }));
}

std::size_t ValidationReport::warning_count() const noexcept {
    return issues_.size() - error_count();
}

std::string ValidationReport::to_string() const {
    if (issues_.empty()) {
        return "map is valid";
    }

    std::ostringstream out;
    out << error_count() << " error(s), " << warning_count() << " warning(s)";
    for (const ValidationIssue& issue : issues_) {
        out << '\n'
            << "  [" << ::traffic::map::to_string(issue.severity) << "] "
            << ::traffic::map::to_string(issue.kind) << ' ' << issue.subject << ": "
            << issue.detail;
    }
    return out.str();
}

ValidationReport validate(const MapData& data) {
    ValidationReport report;

    // ---------------------------------------------------------- duplicates
    check_duplicates<core::NodeId>(
        report,
        data.nodes,
        [](const domain::Node& n) -> const core::NodeId& { return n.id; },
        IssueKind::duplicate_node_id,
        "node");

    check_duplicates<core::EdgeId>(
        report,
        data.edges,
        [](const domain::Edge& e) -> const core::EdgeId& { return e.id; },
        IssueKind::duplicate_edge_id,
        "edge");

    // Corridors, intersections and bays share one resource id space: a
    // reservation names a resource without saying which kind it is, so a
    // corridor and a bay with the same id would be indistinguishable at the
    // moment it matters.
    {
        std::unordered_set<core::ResourceId> seen_resources;
        const auto check_resource = [&](const core::ResourceId& id, std::string_view what) {
            if (id.empty()) {
                return;
            }
            if (!seen_resources.insert(id).second) {
                report.add(IssueKind::duplicate_resource_id,
                           IssueSeverity::error,
                           id.value(),
                           std::string{what} + " reuses a resource id already taken");
            }
        };
        for (const auto& corridor : data.corridors) {
            check_resource(corridor.id, "corridor");
        }
        for (const auto& intersection : data.intersections) {
            check_resource(intersection.id, "intersection");
        }
        for (const auto& bay : data.waiting_bays) {
            check_resource(bay.id, "waiting bay");
        }
    }

    // Index what exists, so references can be checked against it.
    std::unordered_set<core::NodeId> node_ids;
    node_ids.reserve(data.nodes.size());
    for (const auto& node : data.nodes) {
        node_ids.insert(node.id);
    }

    std::unordered_set<core::EdgeId> edge_ids;
    edge_ids.reserve(data.edges.size());
    for (const auto& edge : data.edges) {
        edge_ids.insert(edge.id);
    }

    // --------------------------------------------------------------- nodes
    for (const auto& node : data.nodes) {
        if (node.capacity == 0) {
            report.add(IssueKind::invalid_node_capacity,
                       IssueSeverity::error,
                       node.id.value(),
                       "capacity is zero, so no robot could ever occupy it");
        }
    }

    // --------------------------------------------------------------- edges
    for (const auto& edge : data.edges) {
        if (!node_ids.contains(edge.from_node)) {
            report.add(IssueKind::dangling_edge_endpoint,
                       IssueSeverity::error,
                       edge.id.value(),
                       "from_node '" + edge.from_node.value() + "' is not in the map");
        }
        if (!node_ids.contains(edge.to_node)) {
            report.add(IssueKind::dangling_edge_endpoint,
                       IssueSeverity::error,
                       edge.id.value(),
                       "to_node '" + edge.to_node.value() + "' is not in the map");
        }
        if (edge.from_node == edge.to_node) {
            report.add(IssueKind::self_loop_edge,
                       IssueSeverity::error,
                       edge.id.value(),
                       "both ends are the same node");
        }
        if (edge.length <= 0.0) {
            report.add(IssueKind::invalid_edge_length,
                       IssueSeverity::error,
                       edge.id.value(),
                       "length must be positive");
        }
        if (edge.speed_limit <= 0.0) {
            report.add(IssueKind::invalid_edge_speed_limit,
                       IssueSeverity::error,
                       edge.id.value(),
                       "speed limit must be positive");
        }
        if (edge.capacity == 0) {
            report.add(IssueKind::invalid_edge_capacity,
                       IssueSeverity::error,
                       edge.id.value(),
                       "capacity is zero, so no robot could ever traverse it");
        }
    }

    // ----------------------------------------------------------- resources
    for (const auto& corridor : data.corridors) {
        if (corridor.capacity == 0) {
            report.add(IssueKind::invalid_resource_capacity,
                       IssueSeverity::error,
                       corridor.id.value(),
                       "corridor capacity is zero");
        }
        for (const auto& node_id : {corridor.entry_node, corridor.exit_node}) {
            if (!node_ids.contains(node_id)) {
                report.add(IssueKind::dangling_resource_member,
                           IssueSeverity::error,
                           corridor.id.value(),
                           "end node '" + node_id.value() + "' is not in the map");
            }
        }
        bool edges_resolve = true;
        for (const auto& edge_id : corridor.edges) {
            if (!edge_ids.contains(edge_id)) {
                report.add(IssueKind::dangling_resource_member,
                           IssueSeverity::error,
                           corridor.id.value(),
                           "edge '" + edge_id.value() + "' is not in the map");
                edges_resolve = false;
            }
        }

        // The edges are documented as running in order from entry to exit.
        // If they do not actually join up, the corridor claims to be one
        // passage while covering two disconnected stretches — and reserving it
        // would hand a robot a resource that does not lead where it thinks.
        if (edges_resolve && !corridor.edges.empty()) {
            const auto edge_by_id = [&data](const core::EdgeId& id) -> const domain::Edge* {
                const auto it = std::find_if(data.edges.begin(),
                                             data.edges.end(),
                                             [&id](const domain::Edge& e) { return e.id == id; });
                return it == data.edges.end() ? nullptr : &*it;
            };

            core::NodeId cursor = corridor.entry_node;
            bool contiguous = true;
            for (const auto& edge_id : corridor.edges) {
                const domain::Edge* edge = edge_by_id(edge_id);
                const auto next = domain::opposite_node(*edge, cursor);
                if (!next.has_value()) {
                    report.add(IssueKind::corridor_not_contiguous,
                               IssueSeverity::error,
                               corridor.id.value(),
                               "edge '" + edge_id.value() + "' does not continue from '" +
                                   cursor.value() + "'");
                    contiguous = false;
                    break;
                }
                cursor = *next;
            }
            if (contiguous && cursor != corridor.exit_node) {
                report.add(IssueKind::corridor_not_contiguous,
                           IssueSeverity::error,
                           corridor.id.value(),
                           "following its edges from '" + corridor.entry_node.value() +
                               "' ends at '" + cursor.value() + "', not at the declared exit '" +
                               corridor.exit_node.value() + "'");
            }
        }
    }

    for (const auto& intersection : data.intersections) {
        if (intersection.capacity == 0) {
            report.add(IssueKind::invalid_resource_capacity,
                       IssueSeverity::error,
                       intersection.id.value(),
                       "intersection capacity is zero");
        }
        for (const auto& node_id : intersection.nodes) {
            if (!node_ids.contains(node_id)) {
                report.add(IssueKind::dangling_resource_member,
                           IssueSeverity::error,
                           intersection.id.value(),
                           "node '" + node_id.value() + "' is not in the map");
            }
        }

        // A conflict group that names a movement the intersection does not
        // define silently protects nothing: the lookup misses and the two
        // movements are treated as compatible.
        std::unordered_set<core::MovementId> movement_ids;
        for (const auto& movement : intersection.movements) {
            movement_ids.insert(movement.id);
        }
        for (const auto& group : intersection.conflict_groups) {
            for (const auto& movement_id : group.movements) {
                if (!movement_ids.contains(movement_id)) {
                    report.add(IssueKind::unknown_movement_in_conflict_group,
                               IssueSeverity::error,
                               group.id.value(),
                               "movement '" + movement_id.value() +
                                   "' is not defined by intersection '" + intersection.id.value() +
                                   "'");
                }
            }
        }
    }

    for (const auto& bay : data.waiting_bays) {
        if (bay.capacity == 0) {
            report.add(IssueKind::invalid_resource_capacity,
                       IssueSeverity::error,
                       bay.id.value(),
                       "waiting bay capacity is zero");
        }
        if (!node_ids.contains(bay.node_id)) {
            report.add(IssueKind::dangling_resource_member,
                       IssueSeverity::error,
                       bay.id.value(),
                       "node '" + bay.node_id.value() + "' is not in the map");
        }
    }

    // ---------------------------------------------------- graph structure
    //
    // Only worth checking once the references above hold; running it on a map
    // with dangling endpoints reports connectivity failures that are really
    // just the earlier errors showing through.
    if (report.is_valid() && !data.nodes.empty()) {
        std::unordered_set<core::NodeId> touched;
        for (const auto& edge : data.edges) {
            touched.insert(edge.from_node);
            touched.insert(edge.to_node);
        }
        for (const auto& node : data.nodes) {
            if (!touched.contains(node.id)) {
                report.add(IssueKind::orphan_node,
                           IssueSeverity::warning,
                           node.id.value(),
                           "no edge touches this node");
            }
        }

        const Map map{data};
        const auto reached = map.reachable_from(data.nodes.front().id);
        if (reached.size() < data.nodes.size()) {
            const std::unordered_set<core::NodeId> reachable(reached.begin(), reached.end());
            for (const auto& node : data.nodes) {
                if (!reachable.contains(node.id)) {
                    report.add(IssueKind::unreachable_node,
                               IssueSeverity::warning,
                               node.id.value(),
                               "cannot be reached from '" + data.nodes.front().id.value() + "'");
                }
            }
            report.add(IssueKind::disconnected_graph,
                       IssueSeverity::warning,
                       data.map_id,
                       "only " + std::to_string(reached.size()) + " of " +
                           std::to_string(data.nodes.size()) +
                           " nodes are reachable from the first node");
        }
    }

    return report;
}

core::Result<Map, ValidationReport> make_map(MapData data) {
    ValidationReport report = validate(data);
    if (!report.is_valid()) {
        return core::Result<Map, ValidationReport>::failure(std::move(report));
    }
    return core::Result<Map, ValidationReport>::success(Map{std::move(data)});
}

}  // namespace traffic::map
