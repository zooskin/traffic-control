/// Tests for strongly typed identifiers.
///
/// Naming follows docs/20_CODING_GUIDELINES.md §45:
///   <component>_<condition>_<expected>

#include "traffic/core/ids.h"
#include "traffic/core/strong_id.h"

#include <gtest/gtest.h>

#include <map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace traffic::core {
namespace {

TEST(StrongId, strong_id_default_constructed_is_empty) {
    const RobotId id;
    EXPECT_TRUE(id.empty());
    EXPECT_EQ(id.value(), "");
}

TEST(StrongId, strong_id_constructed_from_string_keeps_value) {
    const RobotId id{"R001"};
    EXPECT_FALSE(id.empty());
    EXPECT_EQ(id.value(), "R001");
}

TEST(StrongId, strong_id_same_value_compares_equal) {
    EXPECT_EQ(RobotId{"R001"}, RobotId{"R001"});
    EXPECT_NE(RobotId{"R001"}, RobotId{"R002"});
}

TEST(StrongId, strong_id_orders_lexicographically) {
    EXPECT_LT(RobotId{"R001"}, RobotId{"R002"});
    EXPECT_GT(NodeId{"N010"}, NodeId{"N002"});
}

/// The whole reason this type exists: docs/20_CODING_GUIDELINES.md §15.
/// A RobotId must not silently become a TaskId.
TEST(StrongId, strong_id_distinct_tags_are_unrelated_types) {
    static_assert(!std::is_same_v<RobotId, TaskId>);
    static_assert(!std::is_same_v<NodeId, EdgeId>);
    static_assert(!std::is_convertible_v<RobotId, TaskId>);
    static_assert(!std::is_convertible_v<NodeId, EdgeId>);
    static_assert(!std::is_assignable_v<RobotId&, TaskId>);
    SUCCEED();
}

/// Implicit construction from a raw string would defeat the separation above.
TEST(StrongId, strong_id_construction_from_raw_string_is_explicit) {
    static_assert(!std::is_convertible_v<std::string, RobotId>);
    static_assert(!std::is_convertible_v<const char*, RobotId>);
    static_assert(std::is_constructible_v<RobotId, std::string>);
    static_assert(std::is_constructible_v<RobotId, const char*>);
    SUCCEED();
}

TEST(StrongId, strong_id_is_usable_as_ordered_map_key) {
    std::map<RobotId, int> by_robot;
    by_robot.emplace(RobotId{"R002"}, 2);
    by_robot.emplace(RobotId{"R001"}, 1);
    by_robot.emplace(RobotId{"R003"}, 3);

    // Ordered containers give a reproducible iteration order, which
    // docs/20_CODING_GUIDELINES.md §23 requires of anything feeding a decision.
    std::vector<std::string> seen;
    seen.reserve(by_robot.size());
    for (const auto& [id, value] : by_robot) {
        seen.push_back(id.value());
    }
    EXPECT_EQ(seen, (std::vector<std::string>{"R001", "R002", "R003"}));
}

TEST(StrongId, strong_id_is_usable_as_unordered_map_key) {
    std::unordered_map<TaskId, int> by_task;
    by_task.emplace(TaskId{"TASK-00001"}, 7);

    const auto it = by_task.find(TaskId{"TASK-00001"});
    ASSERT_NE(it, by_task.end());
    EXPECT_EQ(it->second, 7);
}

TEST(StrongId, strong_id_set_deduplicates_equal_values) {
    const std::set<ResourceId> resources{
        ResourceId{"CORRIDOR-01"},
        ResourceId{"CORRIDOR-01"},
        ResourceId{"CORRIDOR-02"},
    };
    EXPECT_EQ(resources.size(), 2U);
}

TEST(StrongId, strong_id_streams_its_value) {
    std::ostringstream out;
    out << ReservationId{"RES-00001"};
    EXPECT_EQ(out.str(), "RES-00001");
}

TEST(StrongId, strong_id_to_string_returns_value) {
    const DeadlockId id{"DL00012"};
    EXPECT_EQ(to_string(id), "DL00012");
}

/// Formats named in docs/24_DOMAIN_MODEL.md §26 must survive round-tripping.
TEST(StrongId, strong_id_preserves_human_readable_formats) {
    EXPECT_EQ(RobotId{"R001"}.value(), "R001");
    EXPECT_EQ(TaskId{"TASK-00001"}.value(), "TASK-00001");
    EXPECT_EQ(EdgeId{"EDGE-001"}.value(), "EDGE-001");
    EXPECT_EQ(CorridorId{"CORRIDOR-01"}.value(), "CORRIDOR-01");
    EXPECT_EQ(ReservationId{"RES-00001"}.value(), "RES-00001");
}

}  // namespace
}  // namespace traffic::core
