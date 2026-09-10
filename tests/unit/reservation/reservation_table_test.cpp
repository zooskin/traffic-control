/// The reservation table. docs/06_TRAFFIC_RESERVATION.md §7~12, §21~24,
/// docs/24_DOMAIN_MODEL.md §30, docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §7~8,
/// §11~12.
///
/// This is where §39's `test_corridor_capacity`, `test_bidirectional_conflict`
/// and `test_intersection_conflict` live, along with the overlap test §38's
/// Test 1 asks for. The manager's tests cover the rest of §39.
///
/// The one to read first is `table_consecutive_claims_are_not_two_holders`.
/// Counting reservations that overlap the *request* rather than each other is
/// the obvious implementation, and it refuses corridors that are empty — a bug
/// that costs throughput everywhere and never announces itself.

#include "traffic/reservation/reservation_table.h"

#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "reservation_test_map.h"
#include "traffic/core/ids.h"
#include "traffic/domain/reservation.h"
#include "traffic/domain/reservation_state.h"
#include "traffic/domain/values.h"

namespace traffic::reservation {
namespace {

using core::ReservationId;
using core::ResourceId;
using core::RobotId;
using domain::ReservationState;
using domain::TimeWindow;
using test::at;
using test::record;
using test::reference_map;
using test::resource;

ResourceUsage going(TravelDirection direction) {
    ResourceUsage usage;
    usage.direction = direction;
    return usage;
}

ResourceUsage crossing(std::string movement) {
    ResourceUsage usage;
    usage.movement = core::MovementId{std::move(movement)};
    return usage;
}

TimeWindow window(int from_second, int to_second) {
    return TimeWindow{at(from_second), at(to_second)};
}

/// A table over the shared map, with whatever records a test needs already in
/// it.
struct Fixture {
    map::Map graph;
    ReservationTable table;

    Fixture() : graph(reference_map()), table(graph) {}

    void add(ReservationRecord value) { ASSERT_TRUE(table.insert(std::move(value)).has_value()); }

    [[nodiscard]] AdmissionVerdict check(const std::string& resource_id,
                                         const std::string& robot,
                                         int from_second,
                                         int to_second,
                                         ResourceUsage usage = ResourceUsage{}) const {
        return table.check(
            ResourceId{resource_id}, RobotId{robot}, window(from_second, to_second), usage);
    }
};

// ------------------------------------------------------------------ capacity

TEST(ReservationTable, table_capacity_is_read_from_the_map) {
    const Fixture fixture;

    EXPECT_EQ(fixture.table.capacity_of(resource("CORRIDOR-01")), 1U);
    EXPECT_EQ(fixture.table.capacity_of(resource("CORRIDOR-02")), 2U);
    EXPECT_EQ(fixture.table.capacity_of(resource("INTERSECTION-01")), 2U);
    EXPECT_EQ(fixture.table.capacity_of(resource("BAY-01")), 1U);

    // An edge no corridor claims is its own resource.
    EXPECT_EQ(fixture.table.capacity_of(resource("E-SOLO")), 1U);
}

TEST(ReservationTable, table_unknown_resource_admits_one_robot) {
    const Fixture fixture;

    // Not in the map at all. Guessing high here would grant a second robot
    // entry to something nobody has measured.
    EXPECT_EQ(fixture.table.capacity_of(resource("NOT-ON-THE-MAP")), 1U);
}

// ------------------------------------------------------------------- overlap

TEST(ReservationTable, table_free_resource_admits_the_claim) {
    Fixture fixture;

    const AdmissionVerdict verdict = fixture.check("CORRIDOR-01", "R01", 10, 20);

    EXPECT_TRUE(is_admitted(verdict));
    EXPECT_EQ(verdict.peak_holders, 0U);
    EXPECT_TRUE(verdict.blockers.empty());
}

TEST(ReservationTable, table_overlapping_claim_on_single_lane_corridor_is_refused) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 10, 20));

    const AdmissionVerdict verdict = fixture.check("CORRIDOR-01", "R02", 15, 25);

    EXPECT_EQ(verdict.result, AdmissionResult::capacity_exceeded);
    EXPECT_EQ(verdict.peak_holders, 1U);
    ASSERT_EQ(verdict.blocking_robots.size(), 1U);
    EXPECT_EQ(verdict.blocking_robots.front(), RobotId{"R01"});
}

TEST(ReservationTable, table_touching_windows_do_not_overlap) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 10, 20));

    // Half-open windows: the corridor is handed straight over at 20 without a
    // fabricated gap, and without two robots inside at any instant.
    EXPECT_TRUE(is_admitted(fixture.check("CORRIDOR-01", "R02", 20, 30)));
}

TEST(ReservationTable, table_claim_before_an_existing_one_is_admitted) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 20, 30));

    EXPECT_TRUE(is_admitted(fixture.check("CORRIDOR-01", "R02", 5, 15)));
}

TEST(ReservationTable, table_robot_does_not_block_itself) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 10, 20));

    // One robot occupies one place however many claims it holds. Counting its
    // own claim against it leaves it queueing behind itself, which is a
    // deadlock with one participant.
    EXPECT_TRUE(is_admitted(fixture.check("CORRIDOR-01", "R01", 15, 25)));
}

TEST(ReservationTable, table_consecutive_claims_are_not_two_holders) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-02", 0, 10));
    fixture.add(record("RES-02", "R02", "CORRIDOR-02", 10, 20));

    // Both overlap the request, neither overlaps the other: R01 is out before
    // R02 is in, so at no instant are two robots inside. A count of
    // overlapping *reservations* says two and refuses a corridor with room.
    const AdmissionVerdict verdict = fixture.check("CORRIDOR-02", "R03", 5, 15);

    EXPECT_EQ(verdict.peak_holders, 1U);
    EXPECT_TRUE(is_admitted(verdict));
}

TEST(ReservationTable, table_capacity_two_corridor_admits_a_second_robot) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-02", 0, 20));

    EXPECT_TRUE(is_admitted(fixture.check("CORRIDOR-02", "R02", 5, 15)));
}

TEST(ReservationTable, table_capacity_two_corridor_refuses_a_third_robot) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-02", 0, 20));
    fixture.add(record("RES-02", "R02", "CORRIDOR-02", 0, 20));

    const AdmissionVerdict verdict = fixture.check("CORRIDOR-02", "R03", 5, 15);

    EXPECT_EQ(verdict.result, AdmissionResult::capacity_exceeded);
    EXPECT_EQ(verdict.peak_holders, 2U);
    EXPECT_EQ(verdict.capacity, 2U);
}

TEST(ReservationTable, table_two_claims_by_one_robot_count_once) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-02", 0, 20));
    fixture.add(record("RES-02", "R01", "CORRIDOR-02", 0, 20));

    // Two reservations, one robot, one place taken — so the second slot of a
    // capacity-two corridor is still free.
    const AdmissionVerdict verdict = fixture.check("CORRIDOR-02", "R02", 5, 15);

    EXPECT_EQ(verdict.peak_holders, 1U);
    EXPECT_TRUE(is_admitted(verdict));
}

// ----------------------------------------------------------------- direction

TEST(ReservationTable, table_opposing_directions_cannot_share_a_corridor) {
    Fixture fixture;
    fixture.add(record("RES-01",
                       "R01",
                       "CORRIDOR-02",
                       0,
                       20,
                       ReservationState::active,
                       going(TravelDirection::forward)));

    // CORRIDOR-02 has room for two. Capacity is not the rule that stops this —
    // docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §7~8 is, and a corridor holds
    // one direction at a time.
    const AdmissionVerdict verdict =
        fixture.check("CORRIDOR-02", "R02", 5, 15, going(TravelDirection::reverse));

    EXPECT_EQ(verdict.result, AdmissionResult::opposing_direction);
    ASSERT_EQ(verdict.blocking_robots.size(), 1U);
    EXPECT_EQ(verdict.blocking_robots.front(), RobotId{"R01"});
}

TEST(ReservationTable, table_same_direction_shares_a_wide_corridor) {
    Fixture fixture;
    fixture.add(record("RES-01",
                       "R01",
                       "CORRIDOR-02",
                       0,
                       20,
                       ReservationState::active,
                       going(TravelDirection::forward)));

    EXPECT_TRUE(
        is_admitted(fixture.check("CORRIDOR-02", "R02", 5, 15, going(TravelDirection::forward))));
}

TEST(ReservationTable, table_head_on_claim_on_single_lane_corridor_is_refused) {
    Fixture fixture;
    fixture.add(record("RES-01",
                       "R01",
                       "CORRIDOR-01",
                       0,
                       20,
                       ReservationState::active,
                       going(TravelDirection::forward)));

    // The §38 Test 2 case. Refused twice over — direction and capacity — and
    // the reported reason names the direction, because that is what the
    // controller has to resolve.
    const AdmissionVerdict verdict =
        fixture.check("CORRIDOR-01", "R02", 5, 15, going(TravelDirection::reverse));

    EXPECT_FALSE(is_admitted(verdict));
    EXPECT_EQ(verdict.result, AdmissionResult::opposing_direction);
}

TEST(ReservationTable, table_direction_is_ignored_outside_a_corridor) {
    Fixture fixture;
    fixture.add(record("RES-01",
                       "R01",
                       "INTERSECTION-01",
                       0,
                       20,
                       ReservationState::active,
                       going(TravelDirection::forward)));

    // An intersection has no entry-to-exit axis, so a direction on one means
    // nothing there. Capacity two still applies.
    EXPECT_TRUE(is_admitted(
        fixture.check("INTERSECTION-01", "R02", 5, 15, going(TravelDirection::reverse))));
}

// -------------------------------------------------------------- intersection

TEST(ReservationTable, table_conflicting_movements_cannot_cross_together) {
    Fixture fixture;
    fixture.add(record(
        "RES-01", "R01", "INTERSECTION-01", 0, 20, ReservationState::active, crossing("M-NS")));

    // M-NS and M-WE share a conflict group. The intersection has room for two
    // robots, and these two are not a pair it has room for.
    const AdmissionVerdict verdict =
        fixture.check("INTERSECTION-01", "R02", 5, 15, crossing("M-WE"));

    EXPECT_EQ(verdict.result, AdmissionResult::movement_conflict);
}

TEST(ReservationTable, table_independent_movements_cross_together) {
    Fixture fixture;
    fixture.add(record(
        "RES-01", "R01", "INTERSECTION-01", 0, 20, ReservationState::active, crossing("M-WE")));

    // M-EW is in no group with M-WE: two robots passing each other on the same
    // axis do not exclude one another, and an occupancy count alone could not
    // tell that from the case above.
    EXPECT_TRUE(is_admitted(fixture.check("INTERSECTION-01", "R02", 5, 15, crossing("M-EW"))));
}

TEST(ReservationTable, table_intersection_capacity_still_applies_to_free_movements) {
    Fixture fixture;
    fixture.add(record(
        "RES-01", "R01", "INTERSECTION-01", 0, 20, ReservationState::active, crossing("M-WE")));
    fixture.add(record(
        "RES-02", "R02", "INTERSECTION-01", 0, 20, ReservationState::active, crossing("M-EW")));

    // Compatible movements are not a licence to exceed the space.
    EXPECT_EQ(fixture.check("INTERSECTION-01", "R03", 5, 15, crossing("M-EW")).result,
              AdmissionResult::capacity_exceeded);
}

// ----------------------------------------------------------------- lifecycle

TEST(ReservationTable, table_released_claim_stops_blocking) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 10, 20, ReservationState::active));
    ASSERT_FALSE(is_admitted(fixture.check("CORRIDOR-01", "R02", 15, 25)));

    ASSERT_TRUE(fixture.table.set_state(ReservationId{"RES-01"}, ReservationState::released, at(12))
                    .has_value());

    EXPECT_TRUE(is_admitted(fixture.check("CORRIDOR-01", "R02", 15, 25)));
    EXPECT_EQ(fixture.table.holding_count(), 0U);
}

TEST(ReservationTable, table_released_claim_is_never_revived) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 10, 20, ReservationState::active));
    ASSERT_TRUE(fixture.table.set_state(ReservationId{"RES-01"}, ReservationState::released, at(12))
                    .has_value());

    // docs/24_DOMAIN_MODEL.md §30. By now another robot may be in the
    // corridor; bringing the old grant back would give it two owners.
    const auto revived =
        fixture.table.set_state(ReservationId{"RES-01"}, ReservationState::active, at(13));

    ASSERT_FALSE(revived.has_value());
    EXPECT_EQ(revived.error(), TableError::invalid_transition);
}

TEST(ReservationTable, table_pending_claim_still_withholds_the_resource) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 10, 20, ReservationState::pending));

    // A granted claim the robot has not entered yet is still a claim. If it
    // stopped blocking, the robot would be told to go into a corridor that had
    // been handed to somebody else in the meantime.
    EXPECT_FALSE(is_admitted(fixture.check("CORRIDOR-01", "R02", 15, 25)));
}

TEST(ReservationTable, table_duplicate_reservation_id_is_refused) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 10, 20));

    const auto again = fixture.table.insert(record("RES-01", "R02", "CORRIDOR-02", 0, 5));

    ASSERT_FALSE(again.has_value());
    EXPECT_EQ(again.error(), TableError::duplicate_id);
}

TEST(ReservationTable, table_window_end_moves_and_start_does_not) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 10, 20, ReservationState::active));

    ASSERT_TRUE(fixture.table.set_window_end(ReservationId{"RES-01"}, at(30)).has_value());

    const ReservationRecord* stored = fixture.table.find(ReservationId{"RES-01"});
    ASSERT_NE(stored, nullptr);
    EXPECT_EQ(stored->reservation.window.start(), at(10));
    EXPECT_EQ(stored->reservation.window.end(), at(30));

    // The extended span is enforced, not merely recorded.
    EXPECT_FALSE(is_admitted(fixture.check("CORRIDOR-01", "R02", 25, 35)));
}

TEST(ReservationTable, table_window_end_before_start_is_refused) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 10, 20, ReservationState::active));

    const auto shrunk = fixture.table.set_window_end(ReservationId{"RES-01"}, at(5));

    ASSERT_FALSE(shrunk.has_value());
    EXPECT_EQ(shrunk.error(), TableError::invalid_window);
}

TEST(ReservationTable, table_unknown_reservation_is_reported) {
    Fixture fixture;

    const auto missing =
        fixture.table.set_state(ReservationId{"NOPE"}, ReservationState::active, at(1));

    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error(), TableError::unknown_reservation);
    EXPECT_EQ(fixture.table.find(ReservationId{"NOPE"}), nullptr);
}

// --------------------------------------------------------------------- query

TEST(ReservationTable, table_find_conflicts_lists_overlapping_holders) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-02", 0, 10));
    fixture.add(record("RES-02", "R02", "CORRIDOR-02", 5, 15));
    fixture.add(record("RES-03", "R03", "CORRIDOR-02", 30, 40));
    fixture.add(record("RES-04", "R04", "CORRIDOR-01", 0, 40));

    const std::vector<domain::Reservation> found =
        fixture.table.find_conflicts(resource("CORRIDOR-02"), window(6, 8));

    ASSERT_EQ(found.size(), 2U);
    EXPECT_EQ(found[0].id, ReservationId{"RES-01"});
    EXPECT_EQ(found[1].id, ReservationId{"RES-02"});
}

TEST(ReservationTable, table_find_contenders_excludes_the_requester) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-02", 0, 10));
    fixture.add(record("RES-02", "R02", "CORRIDOR-02", 0, 10));

    const std::vector<domain::Reservation> found =
        fixture.table.find_contenders(resource("CORRIDOR-02"), window(0, 10), RobotId{"R01"});

    ASSERT_EQ(found.size(), 1U);
    EXPECT_EQ(found.front().robot_id, RobotId{"R02"});
}

TEST(ReservationTable, table_next_available_is_the_earliest_release) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 0, 30));
    fixture.add(record("RES-02", "R01", "CORRIDOR-01", 0, 12));

    const AdmissionVerdict verdict = fixture.check("CORRIDOR-01", "R02", 5, 15);

    ASSERT_FALSE(is_admitted(verdict));
    ASSERT_TRUE(verdict.next_available.has_value());
    EXPECT_EQ(verdict.next_available.value(), at(12));
}

TEST(ReservationTable, table_owner_and_active_queries_agree) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 0, 30, ReservationState::active));
    fixture.add(record("RES-02", "R02", "CORRIDOR-02", 0, 30, ReservationState::pending));

    EXPECT_EQ(fixture.table.owners_of(resource("CORRIDOR-01")),
              std::vector<RobotId>{RobotId{"R01"}});
    EXPECT_EQ(fixture.table.active_reservations(resource("CORRIDOR-01")).size(), 1U);

    // Pending is holding, not active: R02 owns nothing yet, and the corridor
    // is not free either.
    EXPECT_TRUE(fixture.table.active_reservations(resource("CORRIDOR-02")).empty());
    EXPECT_EQ(fixture.table.holders(resource("CORRIDOR-02")).size(), 1U);
    EXPECT_EQ(fixture.table.active_reservations().size(), 1U);
}

TEST(ReservationTable, table_reservations_for_a_robot_are_in_id_order) {
    Fixture fixture;
    fixture.add(record("RES-03", "R01", "CORRIDOR-01", 0, 5));
    fixture.add(record("RES-01", "R01", "CORRIDOR-02", 0, 5));
    fixture.add(record("RES-02", "R02", "CORRIDOR-02", 0, 5));

    const std::vector<domain::Reservation> mine = fixture.table.reservations_for(RobotId{"R01"});

    ASSERT_EQ(mine.size(), 2U);
    EXPECT_EQ(mine[0].id, ReservationId{"RES-01"});
    EXPECT_EQ(mine[1].id, ReservationId{"RES-03"});
}

TEST(ReservationTable, table_purge_drops_only_finished_claims) {
    Fixture fixture;
    fixture.add(record("RES-01", "R01", "CORRIDOR-01", 0, 10, ReservationState::released));
    fixture.add(record("RES-02", "R02", "CORRIDOR-02", 0, 10, ReservationState::active));
    fixture.add(record("RES-03", "R03", "CORRIDOR-02", 40, 50, ReservationState::released));

    EXPECT_EQ(fixture.table.purge_terminal_before(at(20)), 1U);
    EXPECT_FALSE(fixture.table.contains(ReservationId{"RES-01"}));
    EXPECT_TRUE(fixture.table.contains(ReservationId{"RES-02"}));
    EXPECT_TRUE(fixture.table.contains(ReservationId{"RES-03"}));
}

TEST(ReservationTable, table_repeated_queries_return_the_same_order) {
    Fixture first;
    Fixture second;

    // Same claims, inserted in opposite orders. Anything that let insertion
    // order or hashing reach the answer would show up here, and a scenario
    // that replayed differently on two machines would be untestable.
    for (int index = 0; index < 4; ++index) {
        first.add(record("RES-0" + std::to_string(index),
                         "R0" + std::to_string(index),
                         "CORRIDOR-02",
                         index,
                         index + 10));
    }
    for (int index = 3; index >= 0; --index) {
        second.add(record("RES-0" + std::to_string(index),
                          "R0" + std::to_string(index),
                          "CORRIDOR-02",
                          index,
                          index + 10));
    }

    const AdmissionVerdict left = first.check("CORRIDOR-02", "R09", 0, 20);
    const AdmissionVerdict right = second.check("CORRIDOR-02", "R09", 0, 20);

    EXPECT_EQ(left.result, right.result);
    EXPECT_EQ(left.peak_holders, right.peak_holders);
    EXPECT_EQ(left.blockers, right.blockers);
    EXPECT_EQ(left.blocking_robots, right.blocking_robots);
}

}  // namespace
}  // namespace traffic::reservation
