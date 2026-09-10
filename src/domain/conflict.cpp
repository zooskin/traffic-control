#include "traffic/domain/conflict.h"

#include <array>
#include <utility>

namespace traffic::domain {
namespace {

constexpr std::array<std::pair<ConflictType, std::string_view>, 7> kConflictTypeNames{{
    {ConflictType::node, "NODE_CONFLICT"},
    {ConflictType::edge, "EDGE_CONFLICT"},
    {ConflictType::head_on, "HEAD_ON"},
    {ConflictType::crossing, "CROSSING"},
    {ConflictType::corridor, "CORRIDOR_CONFLICT"},
    {ConflictType::resource, "RESOURCE_CONFLICT"},
    {ConflictType::temporal, "TEMPORAL_CONFLICT"},
}};

using ConflictResult = core::Result<Conflict, DomainError>;

}  // namespace

std::string_view to_string(ConflictType type) noexcept {
    for (const auto& [value, name] : kConflictTypeNames) {
        if (value == type) {
            return name;
        }
    }
    return "RESOURCE_CONFLICT";
}

std::optional<ConflictType> conflict_type_from_string(std::string_view name) noexcept {
    for (const auto& [value, candidate] : kConflictTypeNames) {
        if (candidate == name) {
            return value;
        }
    }
    return std::nullopt;
}

std::string_view to_string(ConflictSeverity severity) noexcept {
    switch (severity) {
        case ConflictSeverity::low:
            return "LOW";
        case ConflictSeverity::medium:
            return "MEDIUM";
        case ConflictSeverity::high:
            return "HIGH";
    }
    return "MEDIUM";
}

bool involves(const Conflict& conflict, const core::RobotId& robot) noexcept {
    return conflict.robot_a == robot || conflict.robot_b == robot;
}

std::optional<core::RobotId> counterpart(const Conflict& conflict, const core::RobotId& robot) {
    if (conflict.robot_a == robot) {
        return conflict.robot_b;
    }
    if (conflict.robot_b == robot) {
        return conflict.robot_a;
    }
    return std::nullopt;
}

core::Result<Conflict, DomainError> make_conflict(core::ConflictId id,
                                                  core::RobotId robot_a,
                                                  core::RobotId robot_b,
                                                  core::ResourceId resource_id,
                                                  ConflictType type,
                                                  ConflictSeverity severity,
                                                  core::TimePoint detected_at) {
    if (id.empty() || robot_a.empty() || robot_b.empty() || resource_id.empty()) {
        return ConflictResult::failure(DomainError::empty_id);
    }
    if (robot_a == robot_b) {
        // A robot in conflict with itself would enter the wait-for graph as a
        // self-loop, which every cycle detector reports as a deadlock.
        return ConflictResult::failure(DomainError::same_source_and_destination);
    }

    Conflict conflict;
    conflict.id = std::move(id);
    conflict.robot_a = std::move(robot_a);
    conflict.robot_b = std::move(robot_b);
    conflict.resource_id = std::move(resource_id);
    conflict.type = type;
    conflict.severity = severity;
    conflict.detected_at = detected_at;
    return ConflictResult::success(std::move(conflict));
}

}  // namespace traffic::domain
