#include "traffic/reservation/conflict_detector.h"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/domain/graph.h"
#include "traffic/domain/reservation_state.h"
#include "traffic/domain/resources.h"
#include "traffic/domain/values.h"

namespace traffic::reservation {
namespace {

using BufferResult = core::Result<SafetyBuffer, domain::DomainError>;
using ConfigResult = core::Result<ConflictDetectorConfig, domain::DomainError>;

/// Everything one detection pass compares, gathered from the routes it was
/// given. Rebuilt per call: the detector holds no state between passes, which
/// is what lets two callers ask at the same time about different fleets.
struct Scene {
    std::vector<NodeVisit> node_visits;
    std::vector<EdgeTraversal> edge_traversals;
    std::vector<CorridorPassage> corridor_passages;
    std::vector<IntersectionCrossing> intersection_crossings;
};

/// A finding, before the passes that describe the same contention are reduced
/// to one. `robot_a` is always the lexicographically smaller id, so a pair is
/// one candidate however the passes happened to encounter it.
struct Candidate {
    core::RobotId robot_a;
    core::RobotId robot_b;
    core::ResourceId resource_id;
    domain::ConflictType type{domain::ConflictType::temporal};
    domain::TimeWindow overlap{kUnboundedWindow};
};

/// Two robots and the resource they are contending for — the identity of a
/// contention, independent of which rule noticed it.
struct ContentionKey {
    core::RobotId robot_a;
    core::RobotId robot_b;
    core::ResourceId resource_id;

    [[nodiscard]] friend bool operator==(const ContentionKey&, const ContentionKey&) = default;
    [[nodiscard]] friend std::strong_ordering operator<=>(const ContentionKey&,
                                                          const ContentionKey&) = default;
};

/// When a robot is at the node at position \p index of \p route, widened by
/// \p buffer.
///
/// A plan has the robot arrive at the end of one segment and leave at the start
/// of the next, which is the same instant; the buffer of
/// docs/06_TRAFFIC_RESERVATION.md §13 is what turns that instant into something
/// two robots can be found to share. The first and last nodes have only one
/// incident segment, so departure and arrival stand in for each other there.
[[nodiscard]] std::optional<domain::TimeWindow> node_window(const domain::Route& route,
                                                            std::size_t index,
                                                            const SafetyBuffer& buffer) {
    std::optional<core::TimePoint> arrival;
    std::optional<core::TimePoint> departure;

    if (index > 0 && index - 1 < route.segments.size()) {
        const std::optional<domain::TimeWindow>& incoming =
            route.segments[index - 1].expected_window;
        if (incoming.has_value()) {
            arrival = incoming.value().end();
        }
    }
    if (index < route.segments.size()) {
        const std::optional<domain::TimeWindow>& outgoing = route.segments[index].expected_window;
        if (outgoing.has_value()) {
            departure = outgoing.value().start();
        }
    }

    if (!arrival.has_value()) {
        arrival = departure;
    }
    if (!departure.has_value()) {
        departure = arrival;
    }
    if (!arrival.has_value() || !departure.has_value()) {
        // Neither incident segment is timed. Nothing here can be compared with
        // anything, and a guessed window would be compared with real ones.
        return std::nullopt;
    }

    auto window =
        domain::make_time_window(arrival.value() - buffer.entry, departure.value() + buffer.exit);
    if (!window.has_value()) {
        return std::nullopt;
    }
    return std::move(window).value();
}

/// The intersection \p node_id belongs to, or nullptr.
[[nodiscard]] const domain::Intersection* intersection_at(const map::Map& map,
                                                          const core::NodeId& node_id) {
    const std::optional<core::ResourceId> resource = map.resource_for_node(node_id);
    if (!resource.has_value()) {
        return nullptr;
    }
    return map.find_intersection(resource.value());
}

/// The corridor's nodes in order, walked from its entry end.
///
/// docs/03_MAP_GRAPH.md §9 orders a corridor's edges but not its nodes, and an
/// individual edge may be stored either way round, so the order has to be
/// walked rather than read.
[[nodiscard]] std::vector<core::NodeId> corridor_node_order(const map::Map& map,
                                                            const domain::Corridor& corridor) {
    std::vector<core::NodeId> order;
    order.reserve(corridor.edges.size() + 1);

    core::NodeId current = corridor.entry_node;
    order.push_back(current);
    for (const core::EdgeId& edge_id : corridor.edges) {
        const domain::Edge* edge = map.find_edge(edge_id);
        if (edge == nullptr) {
            break;
        }
        const std::optional<core::NodeId> next = domain::opposite_node(*edge, current);
        if (!next.has_value()) {
            break;
        }
        current = next.value();
        order.push_back(current);
    }
    return order;
}

/// Which way a passage between \p entry and \p exit runs through \p corridor.
[[nodiscard]] PassageDirection passage_direction(const map::Map& map,
                                                 const domain::Corridor& corridor,
                                                 const core::NodeId& entry,
                                                 const core::NodeId& exit) {
    const std::vector<core::NodeId> order = corridor_node_order(map, corridor);

    std::optional<std::size_t> entry_index;
    std::optional<std::size_t> exit_index;
    for (std::size_t i = 0; i < order.size(); ++i) {
        if (!entry_index.has_value() && order[i] == entry) {
            entry_index = i;
        }
        if (!exit_index.has_value() && order[i] == exit) {
            exit_index = i;
        }
    }

    if (!entry_index.has_value() || !exit_index.has_value()) {
        // A map that loaded always resolves both: a corridor whose edges do not
        // form a run between its ends is a load error, not a runtime surprise
        // (docs/03_MAP_GRAPH.md §17). Falling back to the corridor's own
        // orientation is the harmless answer — claiming the opposite direction
        // would report a head-on between two robots travelling together.
        return PassageDirection::entry_to_exit;
    }
    return entry_index.value() < exit_index.value() ? PassageDirection::entry_to_exit
                                                    : PassageDirection::exit_to_entry;
}

/// How many robots \p resource admits at once.
///
/// The corridor, intersection or bay that owns it when one does; otherwise the
/// edge or node that *is* it (docs/03_MAP_GRAPH.md §20). One when the map knows
/// nothing about it, because that is the assumption that cannot cause a
/// collision.
[[nodiscard]] std::uint32_t resource_capacity(const map::Map& map,
                                              const core::ResourceId& resource) {
    if (const domain::Corridor* corridor = map.find_corridor(resource); corridor != nullptr) {
        return corridor->capacity;
    }
    if (const domain::Intersection* intersection = map.find_intersection(resource);
        intersection != nullptr) {
        return intersection->capacity;
    }
    if (const domain::WaitingBay* bay = map.find_waiting_bay(resource); bay != nullptr) {
        return bay->capacity;
    }
    if (const domain::Edge* edge = map.find_edge(core::EdgeId{resource.value()}); edge != nullptr) {
        return edge->capacity;
    }
    if (const domain::Node* node = map.find_node(core::NodeId{resource.value()}); node != nullptr) {
        return node->capacity;
    }
    return 1U;
}

void add_candidate(std::vector<Candidate>& out,
                   const core::RobotId& first,
                   const core::RobotId& second,
                   core::ResourceId resource,
                   domain::ConflictType type,
                   const std::optional<domain::TimeWindow>& overlap) {
    if (!overlap.has_value() || resource.empty() || first == second) {
        return;
    }

    const bool ordered = first < second;
    Candidate candidate;
    candidate.robot_a = ordered ? first : second;
    candidate.robot_b = ordered ? second : first;
    candidate.resource_id = std::move(resource);
    candidate.type = type;
    candidate.overlap = overlap.value();
    out.push_back(std::move(candidate));
}

/// True when \p candidate is the better description of a contention already
/// described by \p incumbent.
[[nodiscard]] bool is_better_description(const Candidate& candidate,
                                         const Candidate& incumbent) noexcept {
    const int candidate_rank = conflict_type_precedence(candidate.type);
    const int incumbent_rank = conflict_type_precedence(incumbent.type);
    if (candidate_rank != incumbent_rank) {
        return candidate_rank > incumbent_rank;
    }
    // Two passes reaching the same classification: keep the earlier meeting, so
    // the severity reported is the one for the first moment the robots collide
    // rather than for a later consequence of the same approach.
    if (candidate.overlap.start() != incumbent.overlap.start()) {
        return candidate.overlap.start() < incumbent.overlap.start();
    }
    return candidate.overlap.end() < incumbent.overlap.end();
}

void collect(Scene& scene,
             const map::Map& map,
             const domain::Route& route,
             const SafetyBuffer& buffer) {
    const std::vector<NodeVisit> visits = node_visits(map, route, buffer);
    scene.node_visits.insert(scene.node_visits.end(), visits.begin(), visits.end());

    const std::vector<EdgeTraversal> traversals = edge_traversals(map, route);
    scene.edge_traversals.insert(scene.edge_traversals.end(), traversals.begin(), traversals.end());

    const std::vector<CorridorPassage> passages = corridor_passages(map, route);
    scene.corridor_passages.insert(scene.corridor_passages.end(), passages.begin(), passages.end());

    const std::vector<IntersectionCrossing> crossings = intersection_crossings(map, route, buffer);
    scene.intersection_crossings.insert(
        scene.intersection_crossings.end(), crossings.begin(), crossings.end());
}

/// Occupancies of one group of claims, in the group's own order.
template<typename Claim>
[[nodiscard]] std::vector<ResourceOccupancy> occupancies_of(const std::vector<Claim>& claims) {
    std::vector<ResourceOccupancy> holders;
    holders.reserve(claims.size());
    for (const Claim& claim : claims) {
        holders.push_back(as_occupancy(claim));
    }
    return holders;
}

void detect_node_conflicts(const map::Map& map, const Scene& scene, std::vector<Candidate>& out) {
    std::map<core::NodeId, std::vector<NodeVisit>> by_node;
    for (const NodeVisit& visit : scene.node_visits) {
        by_node[visit.node_id].push_back(visit);
    }

    for (const auto& entry : by_node) {
        const std::vector<NodeVisit>& visits = entry.second;
        const domain::Node* node = map.find_node(entry.first);
        const std::uint32_t capacity = node == nullptr ? 1U : node->capacity;
        const std::vector<ResourceOccupancy> holders = occupancies_of(visits);

        for (std::size_t i = 0; i < visits.size(); ++i) {
            for (std::size_t j = i + 1; j < visits.size(); ++j) {
                if (!is_node_conflict(visits[i], visits[j])) {
                    continue;
                }
                if (!is_capacity_conflict(holders[i], holders[j], holders, capacity)) {
                    continue;
                }
                add_candidate(out,
                              visits[i].robot_id,
                              visits[j].robot_id,
                              core::ResourceId{entry.first.value()},
                              domain::ConflictType::node,
                              overlap_of(visits[i].window, visits[j].window));
            }
        }
    }
}

void detect_edge_conflicts(const map::Map& map, const Scene& scene, std::vector<Candidate>& out) {
    std::map<core::EdgeId, std::vector<EdgeTraversal>> by_edge;
    for (const EdgeTraversal& traversal : scene.edge_traversals) {
        by_edge[traversal.edge_id].push_back(traversal);
    }

    for (const auto& entry : by_edge) {
        const std::vector<EdgeTraversal>& traversals = entry.second;
        if (traversals.empty()) {
            continue;
        }
        // Every traversal of one edge names the same resource, and it is the
        // resource — the corridor when one owns the edge — whose capacity
        // decides. The holders counted here are only this edge's, so a corridor
        // wider than one lane is judged properly by the corridor pass instead;
        // this pass then contributes the direction, which that pass takes from
        // the passage as a whole.
        const core::ResourceId resource = traversals.front().resource_id;
        const std::uint32_t capacity = resource_capacity(map, resource);
        const std::vector<ResourceOccupancy> holders = occupancies_of(traversals);

        for (std::size_t i = 0; i < traversals.size(); ++i) {
            for (std::size_t j = i + 1; j < traversals.size(); ++j) {
                const std::optional<domain::ConflictType> type =
                    classify_edge_conflict(traversals[i], traversals[j]);
                if (!type.has_value()) {
                    continue;
                }
                if (!is_capacity_conflict(holders[i], holders[j], holders, capacity)) {
                    continue;
                }
                add_candidate(out,
                              traversals[i].robot_id,
                              traversals[j].robot_id,
                              resource,
                              type.value(),
                              overlap_of(traversals[i].window, traversals[j].window));
            }
        }
    }
}

void detect_corridor_conflicts(const map::Map& map,
                               const Scene& scene,
                               std::vector<Candidate>& out) {
    std::map<core::ResourceId, std::vector<CorridorPassage>> by_corridor;
    for (const CorridorPassage& passage : scene.corridor_passages) {
        by_corridor[passage.corridor_id].push_back(passage);
    }

    for (const auto& entry : by_corridor) {
        const domain::Corridor* corridor = map.find_corridor(entry.first);
        if (corridor == nullptr) {
            continue;
        }
        const std::vector<CorridorPassage>& passages = entry.second;
        const std::vector<ResourceOccupancy> holders = occupancies_of(passages);

        for (std::size_t i = 0; i < passages.size(); ++i) {
            for (std::size_t j = i + 1; j < passages.size(); ++j) {
                domain::ConflictType type = domain::ConflictType::corridor;
                if (is_head_on_conflict(*corridor, passages[i], passages[j])) {
                    type = domain::ConflictType::head_on;
                } else if (!is_capacity_conflict(
                               holders[i], holders[j], holders, corridor->capacity)) {
                    continue;
                }
                add_candidate(out,
                              passages[i].robot_id,
                              passages[j].robot_id,
                              entry.first,
                              type,
                              overlap_of(passages[i].window, passages[j].window));
            }
        }
    }
}

void detect_intersection_conflicts(const map::Map& map,
                                   const Scene& scene,
                                   std::vector<Candidate>& out) {
    std::map<core::ResourceId, std::vector<IntersectionCrossing>> by_intersection;
    for (const IntersectionCrossing& crossing : scene.intersection_crossings) {
        by_intersection[crossing.intersection_id].push_back(crossing);
    }

    for (const auto& entry : by_intersection) {
        const domain::Intersection* intersection = map.find_intersection(entry.first);
        if (intersection == nullptr) {
            continue;
        }
        const std::vector<IntersectionCrossing>& crossings = entry.second;

        // No capacity test here, deliberately. docs/06_TRAFFIC_RESERVATION.md
        // §8 settles it: at an intersection, conflict groups decide. Counting
        // occupants as well would report every workable crossing as full.
        for (std::size_t i = 0; i < crossings.size(); ++i) {
            for (std::size_t j = i + 1; j < crossings.size(); ++j) {
                if (!is_crossing_conflict(*intersection, crossings[i], crossings[j])) {
                    continue;
                }
                add_candidate(out,
                              crossings[i].robot_id,
                              crossings[j].robot_id,
                              entry.first,
                              domain::ConflictType::crossing,
                              overlap_of(crossings[i].window, crossings[j].window));
            }
        }
    }
}

void detect_reservation_conflicts(const map::Map& map,
                                  std::span<const domain::Reservation> reservations,
                                  std::vector<Candidate>& out) {
    std::map<core::ResourceId, std::vector<domain::Reservation>> by_resource;
    for (const domain::Reservation& claim : reservations) {
        if (domain::holds_resource(claim.state)) {
            by_resource[claim.resource_id].push_back(claim);
        }
    }

    for (const auto& entry : by_resource) {
        const std::vector<domain::Reservation>& held = entry.second;
        const std::uint32_t capacity = resource_capacity(map, entry.first);
        const std::vector<ResourceOccupancy> holders = occupancies_of(held);

        for (std::size_t i = 0; i < held.size(); ++i) {
            for (std::size_t j = i + 1; j < held.size(); ++j) {
                // The domain's own pairwise rule first, so there is one
                // definition of "these two claims meet" and this is not a
                // second copy of it that can drift.
                if (!domain::conflicts_with(held[i], held[j])) {
                    continue;
                }
                if (!is_capacity_conflict(holders[i], holders[j], holders, capacity)) {
                    continue;
                }
                add_candidate(out,
                              held[i].robot_id,
                              held[j].robot_id,
                              entry.first,
                              domain::ConflictType::resource,
                              overlap_of(held[i].window, held[j].window));
            }
        }
    }
}

/// Everything the routes in \p scene want, keyed by resource.
[[nodiscard]] std::map<core::ResourceId, std::vector<ResourceOccupancy>> route_claims(
    const Scene& scene) {
    std::map<core::ResourceId, std::vector<ResourceOccupancy>> claims;

    for (const NodeVisit& visit : scene.node_visits) {
        const ResourceOccupancy occupancy = as_occupancy(visit);
        claims[occupancy.resource_id].push_back(occupancy);
    }
    for (const EdgeTraversal& traversal : scene.edge_traversals) {
        const ResourceOccupancy occupancy = as_occupancy(traversal);
        claims[occupancy.resource_id].push_back(occupancy);
    }
    for (const CorridorPassage& passage : scene.corridor_passages) {
        const ResourceOccupancy occupancy = as_occupancy(passage);
        claims[occupancy.resource_id].push_back(occupancy);
    }
    for (const IntersectionCrossing& crossing : scene.intersection_crossings) {
        const ResourceOccupancy occupancy = as_occupancy(crossing);
        claims[occupancy.resource_id].push_back(occupancy);
    }
    return claims;
}

/// A route that wants what a reservation already withholds.
///
/// Classified `temporal`: the clash is visible only because the route carries
/// expected entry and exit times (docs/24_DOMAIN_MODEL.md §12), and without
/// them a robot's intent and another robot's grant could not be compared at
/// all. Where a more specific rule also describes the same contention — a
/// corridor, a crossing — that classification wins on precedence.
void detect_reservation_route_conflicts(const map::Map& map,
                                        const Scene& scene,
                                        std::span<const domain::Reservation> reservations,
                                        std::vector<Candidate>& out) {
    if (reservations.empty()) {
        return;
    }

    std::map<core::ResourceId, std::vector<ResourceOccupancy>> held;
    for (const domain::Reservation& granted : reservations) {
        if (domain::holds_resource(granted.state)) {
            held[granted.resource_id].push_back(as_occupancy(granted));
        }
    }

    const std::map<core::ResourceId, std::vector<ResourceOccupancy>> claims = route_claims(scene);

    for (const auto& entry : held) {
        const auto found = claims.find(entry.first);
        if (found == claims.end()) {
            continue;
        }
        const std::uint32_t capacity = resource_capacity(map, entry.first);

        std::vector<ResourceOccupancy> holders = entry.second;
        holders.insert(holders.end(), found->second.begin(), found->second.end());

        for (const ResourceOccupancy& grant : entry.second) {
            for (const ResourceOccupancy& claim : found->second) {
                if (!is_capacity_conflict(grant, claim, holders, capacity)) {
                    continue;
                }
                add_candidate(out,
                              grant.robot_id,
                              claim.robot_id,
                              entry.first,
                              domain::ConflictType::temporal,
                              overlap_of(grant.window, claim.window));
            }
        }
    }
}

/// One conflict per pair of robots per contended resource, in an order fixed by
/// the conflicts themselves.
[[nodiscard]] std::vector<domain::Conflict> reduce(std::vector<Candidate> candidates,
                                                   core::TimePoint now,
                                                   const ConflictDetectorConfig& config) {
    std::map<ContentionKey, Candidate> best;
    for (Candidate& candidate : candidates) {
        ContentionKey key{candidate.robot_a, candidate.robot_b, candidate.resource_id};
        const auto found = best.find(key);
        if (found == best.end()) {
            best.emplace(std::move(key), std::move(candidate));
        } else if (is_better_description(candidate, found->second)) {
            found->second = std::move(candidate);
        }
    }

    std::vector<domain::Conflict> conflicts;
    conflicts.reserve(best.size());
    for (const auto& entry : best) {
        const Candidate& candidate = entry.second;
        core::ConflictId id = make_conflict_id(config.id_prefix,
                                               candidate.type,
                                               candidate.resource_id,
                                               candidate.robot_a,
                                               candidate.robot_b,
                                               candidate.overlap.start());

        auto conflict = domain::make_conflict(
            std::move(id),
            candidate.robot_a,
            candidate.robot_b,
            candidate.resource_id,
            candidate.type,
            severity_for(candidate.type, candidate.overlap, now, config.severity),
            now);
        if (conflict.has_value()) {
            conflicts.push_back(std::move(conflict).value());
        }
    }
    return conflicts;
}

[[nodiscard]] std::vector<domain::Conflict> detect_in(
    const map::Map& map,
    const Scene& scene,
    std::span<const domain::Reservation> reservations,
    core::TimePoint now,
    const ConflictDetectorConfig& config) {
    std::vector<Candidate> candidates;

    detect_node_conflicts(map, scene, candidates);
    detect_edge_conflicts(map, scene, candidates);
    detect_corridor_conflicts(map, scene, candidates);
    detect_intersection_conflicts(map, scene, candidates);
    detect_reservation_conflicts(map, reservations, candidates);
    detect_reservation_route_conflicts(map, scene, reservations, candidates);

    return reduce(std::move(candidates), now, config);
}

}  // namespace

core::Result<SafetyBuffer, domain::DomainError> make_safety_buffer(SafetyBuffer buffer) {
    if (buffer.entry < core::Duration::zero() || buffer.exit < core::Duration::zero()) {
        return BufferResult::failure(domain::DomainError::negative_value);
    }
    return BufferResult::success(buffer);
}

core::Result<ConflictDetectorConfig, domain::DomainError> make_conflict_detector_config(
    ConflictDetectorConfig config) {
    if (config.id_prefix.empty()) {
        return ConfigResult::failure(domain::DomainError::empty_id);
    }

    auto severity = make_severity_thresholds(config.severity);
    if (!severity.has_value()) {
        return ConfigResult::failure(std::move(severity).error());
    }
    auto buffer = make_safety_buffer(config.node_buffer);
    if (!buffer.has_value()) {
        return ConfigResult::failure(std::move(buffer).error());
    }
    return ConfigResult::success(std::move(config));
}

// ------------------------------------------------------------- route reading

std::vector<EdgeTraversal> edge_traversals(const map::Map& map, const domain::Route& route) {
    std::vector<EdgeTraversal> traversals;
    traversals.reserve(route.segments.size());

    for (const domain::RouteSegment& segment : route.segments) {
        const std::optional<domain::TimeWindow>& window = segment.expected_window;
        if (!window.has_value()) {
            continue;
        }

        EdgeTraversal traversal;
        traversal.robot_id = route.robot_id;
        traversal.edge_id = segment.edge_id;
        traversal.from_node = segment.from_node;
        traversal.to_node = segment.to_node;
        traversal.resource_id = map.resource_for_edge(segment.edge_id);
        traversal.window = window.value();

        if (traversal.resource_id.empty()) {
            // The map does not contain this edge — a route planned against an
            // older map (docs/23_SYSTEM_ARCHITECTURE.md §16). Its own id is
            // still a resource name two robots can be found to share, and
            // dropping the traversal would make a robot invisible.
            traversal.resource_id = core::ResourceId{segment.edge_id.value()};
        }
        traversals.push_back(std::move(traversal));
    }
    return traversals;
}

std::vector<NodeVisit> node_visits(const map::Map& map,
                                   const domain::Route& route,
                                   const SafetyBuffer& buffer) {
    std::vector<NodeVisit> visits;
    visits.reserve(route.nodes.size());

    for (std::size_t index = 0; index < route.nodes.size(); ++index) {
        const core::NodeId& node_id = route.nodes[index];
        if (intersection_at(map, node_id) != nullptr) {
            continue;
        }
        const std::optional<domain::TimeWindow> window = node_window(route, index, buffer);
        if (!window.has_value()) {
            continue;
        }

        NodeVisit visit;
        visit.robot_id = route.robot_id;
        visit.node_id = node_id;
        visit.window = window.value();
        visits.push_back(std::move(visit));
    }
    return visits;
}

std::vector<CorridorPassage> corridor_passages(const map::Map& map, const domain::Route& route) {
    std::vector<CorridorPassage> passages;

    std::size_t index = 0;
    while (index < route.segments.size()) {
        const std::optional<domain::TimeWindow>& first = route.segments[index].expected_window;
        if (!first.has_value()) {
            ++index;
            continue;
        }

        const core::ResourceId resource = map.resource_for_edge(route.segments[index].edge_id);
        const domain::Corridor* corridor = map.find_corridor(resource);
        if (corridor == nullptr) {
            ++index;
            continue;
        }

        std::size_t last = index;
        while (last + 1 < route.segments.size()) {
            const domain::RouteSegment& next = route.segments[last + 1];
            if (!next.expected_window.has_value()) {
                break;
            }
            if (map.resource_for_edge(next.edge_id) != resource) {
                break;
            }
            ++last;
        }

        const std::optional<domain::TimeWindow>& closing = route.segments[last].expected_window;
        if (closing.has_value()) {
            auto window = domain::make_time_window(first.value().start(), closing.value().end());
            if (window.has_value()) {
                CorridorPassage passage;
                passage.robot_id = route.robot_id;
                passage.corridor_id = resource;
                passage.entry_node = route.segments[index].from_node;
                passage.exit_node = route.segments[last].to_node;
                passage.direction =
                    passage_direction(map, *corridor, passage.entry_node, passage.exit_node);
                passage.window = std::move(window).value();
                passages.push_back(std::move(passage));
            }
        }

        index = last + 1;
    }
    return passages;
}

std::vector<IntersectionCrossing> intersection_crossings(const map::Map& map,
                                                         const domain::Route& route,
                                                         const SafetyBuffer& buffer) {
    std::vector<IntersectionCrossing> crossings;

    for (std::size_t index = 0; index < route.nodes.size(); ++index) {
        const domain::Intersection* intersection = intersection_at(map, route.nodes[index]);
        if (intersection == nullptr) {
            continue;
        }
        const std::optional<domain::TimeWindow> window = node_window(route, index, buffer);
        if (!window.has_value()) {
            continue;
        }

        IntersectionCrossing crossing;
        crossing.robot_id = route.robot_id;
        crossing.intersection_id = intersection->id;
        crossing.window = window.value();

        if (index > 0 && index < route.segments.size()) {
            const std::optional<domain::Movement> movement = domain::find_movement(
                *intersection, route.segments[index - 1].edge_id, route.segments[index].edge_id);
            if (movement.has_value()) {
                crossing.movement_id = movement.value().id;
            }
        }
        crossings.push_back(std::move(crossing));
    }
    return crossings;
}

// -------------------------------------------------------------------- detector

ConflictDetector::ConflictDetector(const map::Map& map, const core::IClock& clock)
    : ConflictDetector(map, clock, ConflictDetectorConfig{}) {}

ConflictDetector::ConflictDetector(const map::Map& map,
                                   const core::IClock& clock,
                                   ConflictDetectorConfig config)
    : map_(map), clock_(clock), config_(std::move(config)) {}

std::string_view ConflictDetector::name() const noexcept {
    return "ConflictDetector";
}

std::vector<domain::Conflict> ConflictDetector::detect(const ConflictQuery& query) const {
    Scene scene;
    for (const domain::Route& route : query.routes) {
        collect(scene, map_, route, config_.node_buffer);
    }
    return detect_in(map_, scene, query.reservations, clock_.now(), config_);
}

std::vector<domain::Conflict> ConflictDetector::detect_between(const domain::Route& lhs,
                                                               const domain::Route& rhs) const {
    Scene scene;
    collect(scene, map_, lhs, config_.node_buffer);
    collect(scene, map_, rhs, config_.node_buffer);
    return detect_in(map_, scene, {}, clock_.now(), config_);
}

}  // namespace traffic::reservation
