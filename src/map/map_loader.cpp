#include "traffic/map/map_loader.h"

#include <array>
#include <fstream>
#include <sstream>
#include <utility>

#include <nlohmann/json.hpp>

namespace traffic::map {
namespace {

using Json = nlohmann::json;
using LoadResult = core::Result<Map, LoadError>;

[[nodiscard]] LoadError schema_error(std::string message) {
    return LoadError{LoadError::Kind::schema_violation, std::move(message), {}};
}

[[nodiscard]] LoadError value_error(std::string message) {
    return LoadError{LoadError::Kind::invalid_value, std::move(message), {}};
}

/// Reads a required field, naming the object when it is absent.
///
/// Every accessor here reports *which* element was wrong. "expected a string"
/// with no location is useless in a file with two thousand edges.
template<typename T>
[[nodiscard]] core::Result<T, LoadError> require(const Json& object,
                                                 std::string_view field,
                                                 std::string_view context) {
    using R = core::Result<T, LoadError>;

    const auto it = object.find(field);
    if (it == object.end()) {
        return R::failure(schema_error(std::string{context} + ": missing required field '" +
                                       std::string{field} + "'"));
    }
    try {
        return R::success(it->get<T>());
    } catch (const Json::exception& e) {
        return R::failure(schema_error(std::string{context} + ": field '" + std::string{field} +
                                       "' has the wrong type (" + e.what() + ")"));
    }
}

template<typename T>
[[nodiscard]] T optional_field(const Json& object, std::string_view field, T fallback) {
    const auto it = object.find(field);
    if (it == object.end() || it->is_null()) {
        return fallback;
    }
    try {
        return it->get<T>();
    } catch (const Json::exception&) {
        return fallback;
    }
}

[[nodiscard]] core::Result<domain::Node, LoadError> parse_node(const Json& json) {
    using R = core::Result<domain::Node, LoadError>;

    auto id = require<std::string>(json, "id", "node");
    if (!id.has_value()) {
        return R::failure(std::move(id).error());
    }
    const std::string node_id = std::move(id).value();
    const std::string context = "node '" + node_id + "'";

    domain::Node node;
    node.id = core::NodeId{node_id};
    node.position.x = optional_field<double>(json, "x", 0.0);
    node.position.y = optional_field<double>(json, "y", 0.0);
    node.position.z = optional_field<double>(json, "z", 0.0);
    node.capacity = optional_field<std::uint32_t>(json, "capacity", 1);

    const std::string type_name = optional_field<std::string>(json, "type", "NORMAL");
    const auto type = domain::node_type_from_string(type_name);
    if (!type.has_value()) {
        return R::failure(value_error(context + ": unknown node type '" + type_name + "'"));
    }
    node.type = *type;

    if (json.contains("orientation") && !json.at("orientation").is_null()) {
        node.orientation = optional_field<double>(json, "orientation", 0.0);
    }

    const std::string resource = optional_field<std::string>(json, "resource_id", "");
    if (!resource.empty()) {
        node.resource_id = core::ResourceId{resource};
    }

    return R::success(std::move(node));
}

[[nodiscard]] core::Result<domain::Edge, LoadError> parse_edge(const Json& json) {
    using R = core::Result<domain::Edge, LoadError>;

    auto id = require<std::string>(json, "id", "edge");
    if (!id.has_value()) {
        return R::failure(std::move(id).error());
    }
    const std::string edge_id = std::move(id).value();
    const std::string context = "edge '" + edge_id + "'";

    auto from = require<std::string>(json, "from", context);
    if (!from.has_value()) {
        return R::failure(std::move(from).error());
    }
    auto to = require<std::string>(json, "to", context);
    if (!to.has_value()) {
        return R::failure(std::move(to).error());
    }
    auto length = require<double>(json, "length", context);
    if (!length.has_value()) {
        return R::failure(std::move(length).error());
    }

    domain::Edge edge;
    edge.id = core::EdgeId{edge_id};
    edge.from_node = core::NodeId{std::move(from).value()};
    edge.to_node = core::NodeId{std::move(to).value()};
    edge.length = length.value();
    edge.width = optional_field<double>(json, "width", 0.0);
    edge.capacity = optional_field<std::uint32_t>(json, "capacity", 1);
    edge.enabled = optional_field<bool>(json, "enabled", true);

    // Left to the map defaults when absent, so a site does not have to repeat
    // the same speed on every edge (docs/03_MAP_GRAPH.md §23).
    edge.speed_limit = optional_field<double>(json, "speed_limit", 0.0);

    const std::string direction_name =
        optional_field<std::string>(json, "direction", "BIDIRECTIONAL");
    const auto direction = domain::edge_direction_from_string(direction_name);
    if (!direction.has_value()) {
        return R::failure(value_error(context + ": unknown direction '" + direction_name + "'"));
    }
    edge.direction = *direction;

    const std::string resource = optional_field<std::string>(json, "resource_id", "");
    if (!resource.empty()) {
        edge.resource_id = core::ResourceId{resource};
    }

    if (json.contains("travel_time_s") && !json.at("travel_time_s").is_null()) {
        const double seconds = optional_field<double>(json, "travel_time_s", 0.0);
        edge.travel_time_override =
            std::chrono::duration_cast<core::Duration>(std::chrono::duration<double>{seconds});
    }

    return R::success(std::move(edge));
}

[[nodiscard]] core::Result<domain::Corridor, LoadError> parse_corridor(const Json& json) {
    using R = core::Result<domain::Corridor, LoadError>;

    auto id = require<std::string>(json, "id", "corridor");
    if (!id.has_value()) {
        return R::failure(std::move(id).error());
    }
    const std::string corridor_id = std::move(id).value();
    const std::string context = "corridor '" + corridor_id + "'";

    auto entry = require<std::string>(json, "entry_node", context);
    if (!entry.has_value()) {
        return R::failure(std::move(entry).error());
    }
    auto exit_node = require<std::string>(json, "exit_node", context);
    if (!exit_node.has_value()) {
        return R::failure(std::move(exit_node).error());
    }
    auto edges = require<std::vector<std::string>>(json, "edges", context);
    if (!edges.has_value()) {
        return R::failure(std::move(edges).error());
    }

    std::vector<core::EdgeId> edge_ids;
    for (auto& raw : std::move(edges).value()) {
        edge_ids.emplace_back(std::move(raw));
    }

    const std::string direction_name =
        optional_field<std::string>(json, "direction", "BIDIRECTIONAL");
    const auto direction = domain::edge_direction_from_string(direction_name);
    if (!direction.has_value()) {
        return R::failure(value_error(context + ": unknown direction '" + direction_name + "'"));
    }

    auto corridor = domain::make_corridor(core::ResourceId{corridor_id},
                                          core::NodeId{std::move(entry).value()},
                                          core::NodeId{std::move(exit_node).value()},
                                          std::move(edge_ids),
                                          optional_field<std::uint32_t>(json, "capacity", 1),
                                          *direction);
    if (!corridor.has_value()) {
        return R::failure(
            value_error(context + ": " + std::string{domain::to_string(corridor.error())}));
    }
    return R::success(std::move(corridor).value());
}

[[nodiscard]] core::Result<domain::WaitingBay, LoadError> parse_waiting_bay(const Json& json) {
    using R = core::Result<domain::WaitingBay, LoadError>;

    auto id = require<std::string>(json, "id", "waiting bay");
    if (!id.has_value()) {
        return R::failure(std::move(id).error());
    }
    const std::string bay_id = std::move(id).value();
    const std::string context = "waiting bay '" + bay_id + "'";

    auto node_id = require<std::string>(json, "node_id", context);
    if (!node_id.has_value()) {
        return R::failure(std::move(node_id).error());
    }

    auto bay = domain::make_waiting_bay(core::ResourceId{bay_id},
                                        core::NodeId{std::move(node_id).value()},
                                        optional_field<std::uint32_t>(json, "capacity", 1));
    if (!bay.has_value()) {
        return R::failure(
            value_error(context + ": " + std::string{domain::to_string(bay.error())}));
    }

    domain::WaitingBay result = std::move(bay).value();
    result.compatible_robot_types =
        optional_field<std::vector<std::string>>(json, "compatible_robot_types", {});
    return R::success(std::move(result));
}

[[nodiscard]] core::Result<domain::Intersection, LoadError> parse_intersection(const Json& json) {
    using R = core::Result<domain::Intersection, LoadError>;

    auto id = require<std::string>(json, "id", "intersection");
    if (!id.has_value()) {
        return R::failure(std::move(id).error());
    }
    const std::string intersection_id = std::move(id).value();
    const std::string context = "intersection '" + intersection_id + "'";

    auto nodes = require<std::vector<std::string>>(json, "nodes", context);
    if (!nodes.has_value()) {
        return R::failure(std::move(nodes).error());
    }

    std::vector<core::NodeId> node_ids;
    for (auto& raw : std::move(nodes).value()) {
        node_ids.emplace_back(std::move(raw));
    }

    std::vector<core::EdgeId> edge_ids;
    for (auto& raw : optional_field<std::vector<std::string>>(json, "edges", {})) {
        edge_ids.emplace_back(std::move(raw));
    }

    auto intersection =
        domain::make_intersection(core::ResourceId{intersection_id},
                                  std::move(node_ids),
                                  std::move(edge_ids),
                                  optional_field<std::uint32_t>(json, "capacity", 1));
    if (!intersection.has_value()) {
        return R::failure(
            value_error(context + ": " + std::string{domain::to_string(intersection.error())}));
    }
    domain::Intersection result = std::move(intersection).value();

    for (const auto& movement_json : optional_field<Json>(json, "movements", Json::array())) {
        auto movement_id = require<std::string>(movement_json, "id", context + " movement");
        if (!movement_id.has_value()) {
            return R::failure(std::move(movement_id).error());
        }
        auto from_edge = require<std::string>(movement_json, "from_edge", context + " movement");
        if (!from_edge.has_value()) {
            return R::failure(std::move(from_edge).error());
        }
        auto to_edge = require<std::string>(movement_json, "to_edge", context + " movement");
        if (!to_edge.has_value()) {
            return R::failure(std::move(to_edge).error());
        }

        result.movements.push_back(
            domain::Movement{core::MovementId{std::move(movement_id).value()},
                             core::ResourceId{intersection_id},
                             core::EdgeId{std::move(from_edge).value()},
                             core::EdgeId{std::move(to_edge).value()}});
    }

    for (const auto& group_json : optional_field<Json>(json, "conflict_groups", Json::array())) {
        auto group_id = require<std::string>(group_json, "id", context + " conflict group");
        if (!group_id.has_value()) {
            return R::failure(std::move(group_id).error());
        }
        auto members =
            require<std::vector<std::string>>(group_json, "movements", context + " conflict group");
        if (!members.has_value()) {
            return R::failure(std::move(members).error());
        }

        domain::ConflictGroup group;
        group.id = core::ConflictGroupId{std::move(group_id).value()};
        for (auto& raw : std::move(members).value()) {
            group.movements.emplace_back(std::move(raw));
        }
        result.conflict_groups.push_back(std::move(group));
    }

    return R::success(std::move(result));
}

[[nodiscard]] Json node_to_json(const domain::Node& node) {
    Json json;
    json["id"] = node.id.value();
    json["type"] = domain::to_string(node.type);
    json["x"] = node.position.x;
    json["y"] = node.position.y;
    if (node.position.z != 0.0) {
        json["z"] = node.position.z;
    }
    json["capacity"] = node.capacity;
    if (node.orientation.has_value()) {
        json["orientation"] = *node.orientation;
    }
    if (!node.resource_id.empty()) {
        json["resource_id"] = node.resource_id.value();
    }
    return json;
}

[[nodiscard]] Json edge_to_json(const domain::Edge& edge) {
    Json json;
    json["id"] = edge.id.value();
    json["from"] = edge.from_node.value();
    json["to"] = edge.to_node.value();
    json["length"] = edge.length;
    json["width"] = edge.width;
    json["speed_limit"] = edge.speed_limit;
    json["direction"] = domain::to_string(edge.direction);
    json["capacity"] = edge.capacity;
    json["enabled"] = edge.enabled;
    if (!edge.resource_id.empty()) {
        json["resource_id"] = edge.resource_id.value();
    }
    if (edge.travel_time_override.has_value()) {
        json["travel_time_s"] = std::chrono::duration<double>{*edge.travel_time_override}.count();
    }
    return json;
}

}  // namespace

std::string_view to_string(LoadError::Kind kind) noexcept {
    switch (kind) {
        case LoadError::Kind::file_not_readable:
            return "FILE_NOT_READABLE";
        case LoadError::Kind::malformed_json:
            return "MALFORMED_JSON";
        case LoadError::Kind::schema_violation:
            return "SCHEMA_VIOLATION";
        case LoadError::Kind::invalid_value:
            return "INVALID_VALUE";
        case LoadError::Kind::validation_failed:
            return "VALIDATION_FAILED";
    }
    return "SCHEMA_VIOLATION";
}

std::string LoadError::to_string() const {
    std::string text{::traffic::map::to_string(kind)};
    text += ": ";
    text += message;
    if (kind == Kind::validation_failed) {
        text += '\n';
        text += report.to_string();
    }
    return text;
}

core::Result<Map, LoadError> load_map_from_json(std::string_view json_text) {
    Json json;
    try {
        json = Json::parse(json_text);
    } catch (const Json::parse_error& e) {
        return LoadResult::failure(LoadError{LoadError::Kind::malformed_json, e.what(), {}});
    }

    if (!json.is_object()) {
        return LoadResult::failure(schema_error("the top level of a map file must be an object"));
    }

    MapData data;
    data.map_id = optional_field<std::string>(json, "map_id", "unnamed");
    data.version = domain::MapVersion{optional_field<std::uint64_t>(json, "version", 0)};

    // docs/03_MAP_GRAPH.md §23: a site sets its defaults once instead of
    // repeating the same speed limit on every edge.
    const Json defaults = optional_field<Json>(json, "defaults", Json::object());
    const double default_speed = optional_field<double>(defaults, "speed_limit", 1.0);

    auto nodes = require<Json>(json, "nodes", "map");
    if (!nodes.has_value()) {
        return LoadResult::failure(std::move(nodes).error());
    }
    for (const auto& node_json : nodes.value()) {
        auto node = parse_node(node_json);
        if (!node.has_value()) {
            return LoadResult::failure(std::move(node).error());
        }
        data.nodes.push_back(std::move(node).value());
    }

    auto edges = require<Json>(json, "edges", "map");
    if (!edges.has_value()) {
        return LoadResult::failure(std::move(edges).error());
    }
    for (const auto& edge_json : edges.value()) {
        auto edge = parse_edge(edge_json);
        if (!edge.has_value()) {
            return LoadResult::failure(std::move(edge).error());
        }
        domain::Edge parsed = std::move(edge).value();
        if (parsed.speed_limit <= 0.0) {
            parsed.speed_limit = default_speed;
        }
        data.edges.push_back(std::move(parsed));
    }

    for (const auto& corridor_json : optional_field<Json>(json, "corridors", Json::array())) {
        auto corridor = parse_corridor(corridor_json);
        if (!corridor.has_value()) {
            return LoadResult::failure(std::move(corridor).error());
        }
        data.corridors.push_back(std::move(corridor).value());
    }

    for (const auto& intersection_json :
         optional_field<Json>(json, "intersections", Json::array())) {
        auto intersection = parse_intersection(intersection_json);
        if (!intersection.has_value()) {
            return LoadResult::failure(std::move(intersection).error());
        }
        data.intersections.push_back(std::move(intersection).value());
    }

    for (const auto& bay_json : optional_field<Json>(json, "waiting_bays", Json::array())) {
        auto bay = parse_waiting_bay(bay_json);
        if (!bay.has_value()) {
            return LoadResult::failure(std::move(bay).error());
        }
        data.waiting_bays.push_back(std::move(bay).value());
    }

    auto built = make_map(std::move(data));
    if (!built.has_value()) {
        return LoadResult::failure(LoadError{LoadError::Kind::validation_failed,
                                             "the map parsed but did not validate",
                                             std::move(built).error()});
    }
    return LoadResult::success(std::move(built).value());
}

core::Result<Map, LoadError> load_map_from_file(const std::filesystem::path& path) {
    std::ifstream in{path};
    if (!in.is_open()) {
        return LoadResult::failure(LoadError{
            LoadError::Kind::file_not_readable, "could not open '" + path.string() + "'", {}});
    }

    std::ostringstream buffer;
    buffer << in.rdbuf();
    return load_map_from_json(buffer.str());
}

std::string save_map_to_json(const Map& map, int indent) {
    Json json;
    json["map_id"] = map.id();
    json["version"] = map.version().value();

    json["nodes"] = Json::array();
    for (const auto& node : map.nodes()) {
        json["nodes"].push_back(node_to_json(node));
    }

    json["edges"] = Json::array();
    for (const auto& edge : map.edges()) {
        json["edges"].push_back(edge_to_json(edge));
    }

    if (!map.corridors().empty()) {
        json["corridors"] = Json::array();
        for (const auto& corridor : map.corridors()) {
            Json entry;
            entry["id"] = corridor.id.value();
            entry["entry_node"] = corridor.entry_node.value();
            entry["exit_node"] = corridor.exit_node.value();
            entry["capacity"] = corridor.capacity;
            entry["direction"] = domain::to_string(corridor.direction);
            entry["edges"] = Json::array();
            for (const auto& edge_id : corridor.edges) {
                entry["edges"].push_back(edge_id.value());
            }
            json["corridors"].push_back(std::move(entry));
        }
    }

    if (!map.intersections().empty()) {
        json["intersections"] = Json::array();
        for (const auto& intersection : map.intersections()) {
            Json entry;
            entry["id"] = intersection.id.value();
            entry["capacity"] = intersection.capacity;
            entry["nodes"] = Json::array();
            for (const auto& node_id : intersection.nodes) {
                entry["nodes"].push_back(node_id.value());
            }
            entry["edges"] = Json::array();
            for (const auto& edge_id : intersection.edges) {
                entry["edges"].push_back(edge_id.value());
            }
            entry["movements"] = Json::array();
            for (const auto& movement : intersection.movements) {
                Json m;
                m["id"] = movement.id.value();
                m["from_edge"] = movement.from_edge.value();
                m["to_edge"] = movement.to_edge.value();
                entry["movements"].push_back(std::move(m));
            }
            entry["conflict_groups"] = Json::array();
            for (const auto& group : intersection.conflict_groups) {
                Json g;
                g["id"] = group.id.value();
                g["movements"] = Json::array();
                for (const auto& movement_id : group.movements) {
                    g["movements"].push_back(movement_id.value());
                }
                entry["conflict_groups"].push_back(std::move(g));
            }
            json["intersections"].push_back(std::move(entry));
        }
    }

    if (!map.waiting_bays().empty()) {
        json["waiting_bays"] = Json::array();
        for (const auto& bay : map.waiting_bays()) {
            Json entry;
            entry["id"] = bay.id.value();
            entry["node_id"] = bay.node_id.value();
            entry["capacity"] = bay.capacity;
            if (!bay.compatible_robot_types.empty()) {
                entry["compatible_robot_types"] = bay.compatible_robot_types;
            }
            json["waiting_bays"].push_back(std::move(entry));
        }
    }

    return json.dump(indent);
}

core::Result<void, LoadError> save_map_to_file(const Map& map, const std::filesystem::path& path) {
    std::ofstream out{path};
    if (!out.is_open()) {
        return core::Result<void, LoadError>::failure(
            LoadError{LoadError::Kind::file_not_readable,
                      "could not open '" + path.string() + "' for writing",
                      {}});
    }
    out << save_map_to_json(map);
    return core::Result<void, LoadError>::success();
}

}  // namespace traffic::map
