/// The reservation manager. docs/06_TRAFFIC_RESERVATION.md §6, §13~26, §30~37,
/// docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §7~12, §15~16, §22~23,
/// docs/24_DOMAIN_MODEL.md §30.
///
/// §39 names fifteen tests. Eleven of them are here —
/// `test_reservation_grant`, `test_reservation_conflict`, `test_priority`,
/// `test_priority_aging`, `test_reservation_extension`,
/// `test_extension_conflict`, `test_release`, `test_expiration`,
/// `test_concurrent_request`, `test_idempotency`, `test_recovery` and
/// `test_affected_robot` — and the other three (`test_corridor_capacity`,
/// `test_bidirectional_conflict`, `test_intersection_conflict`) are in
/// reservation_table_test.cpp, where the rules they exercise live.
///
/// The tests that matter most are the ones about what is *not* taken back:
/// a grant is never revoked to satisfy a competing request, and an active claim
/// is never expired on a timer. Both are places where a plausible optimisation
/// puts two robots in one corridor.

#include "traffic/reservation/reservation_manager.h"

#include <span>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "reservation_test_map.h"
#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/reservation.h"
#include "traffic/domain/reservation_state.h"
#include "traffic/domain/values.h"

namespace traffic::reservation {
namespace {

using core::ReservationId;
using core::ResourceId;
using core::RobotId;
using domain::Priority;
using domain::ReservationState;
using test::at;
using test::reference_map;
using test::request;
using test::seconds;

/// A map, a table and a manager over it.
struct Fixture {
    map::Map graph;
    ReservationTable table;
    ReservationManager manager;

    explicit Fixture(ReservationPolicy policy = test::unbuffered_policy())
        : graph(reference_map()), table(graph), manager(table, policy) {}

    ReservationDecision ask(const ReservationRequest& value, int now_second) {
        return manager.request(value, at(now_second));
    }

    /// Grants \p reservation_id and fails the test if it was refused, so that
    /// a test about what happens next cannot pass on a corridor nobody got.
    void own(const std::string& reservation_id,
             const std::string& robot,
             const std::string& resource_id,
             int start_second,
             int duration_seconds,
             int now_second) {
        const ReservationDecision decision =
            ask(request(reservation_id, robot, resource_id, start_second, duration_seconds),
                now_second);
        ASSERT_TRUE(is_granted(decision)) << to_string(decision.reason);
    }

    [[nodiscard]] ReservationState state_of(const std::string& reservation_id) const {
        const ReservationRecord* found = manager.get_reservation(ReservationId{reservation_id});
        return found == nullptr ? ReservationState::cancelled : found->reservation.state;
    }
};

ReservationRequest with_priority(ReservationRequest value, int priority) {
    value.priority = Priority{priority};
    return value;
}

// -------------------------------------------------------------------- grant

TEST(ReservationManager, manager_free_resource_is_granted) {
    Fixture fixture;

    const ReservationDecision decision =
        fixture.ask(request("RES-01", "R01", "CORRIDOR-01", 10, 10), 0);

    EXPECT_EQ(decision.status, DecisionStatus::granted);
    EXPECT_EQ(decision.reason, DenialReason::none);
    EXPECT_EQ(decision.window_start, at(10));
    EXPECT_EQ(decision.window_end, at(20));
    EXPECT_FALSE(decision.replayed);
    EXPECT_EQ(fixture.manager.metrics().granted, 1U);
    EXPECT_NE(fixture.manager.get_reservation(ReservationId{"RES-01"}), nullptr);
}

TEST(ReservationManager, manager_granted_window_includes_the_safety_buffers) {
    // The default policy, which carries §13's example buffers.
    Fixture fixture{ReservationPolicy{}};

    const ReservationDecision decision =
        fixture.ask(request("RES-01", "R01", "CORRIDOR-01", 5, 10), 0);

    ASSERT_TRUE(is_granted(decision));

    // The buffered window, not the requested one, is what is recorded — and so
    // what everyone else is kept out of. A buffer nobody is excluded from
    // protects nothing.
    EXPECT_EQ(decision.window_start, at(5) - fixture.manager.policy().entry_buffer);
    EXPECT_EQ(decision.window_end, at(15) + fixture.manager.policy().exit_buffer);
}

TEST(ReservationManager, manager_empty_identifier_is_rejected) {
    Fixture fixture;

    ReservationRequest nameless = request("", "R01", "CORRIDOR-01", 0, 10);

    const ReservationDecision decision = fixture.ask(nameless, 0);

    EXPECT_EQ(decision.status, DecisionStatus::rejected);
    EXPECT_EQ(decision.reason, DenialReason::empty_id);
    EXPECT_EQ(fixture.table.size(), 0U);
}

TEST(ReservationManager, manager_request_beyond_the_horizon_is_rejected) {
    ReservationPolicy policy = test::unbuffered_policy();
    policy.horizon = seconds(10);
    Fixture fixture{policy};

    const ReservationDecision decision =
        fixture.ask(request("RES-01", "R01", "CORRIDOR-01", 100, 10), 0);

    // §28~29. Reserving that far ahead locks a corridor nobody is near, which
    // reads on the floor as traffic stopping for robots that are not there.
    EXPECT_EQ(decision.status, DecisionStatus::rejected);
    EXPECT_EQ(decision.reason, DenialReason::beyond_horizon);
}

// ----------------------------------------------------------------- conflict

TEST(ReservationManager, manager_second_robot_on_a_busy_corridor_waits) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 10, 10, 0);

    const ReservationDecision decision =
        fixture.ask(request("RES-02", "R02", "CORRIDOR-01", 15, 10), 0);

    EXPECT_EQ(decision.status, DecisionStatus::wait);
    EXPECT_EQ(decision.reason, DenialReason::resource_occupied);
    ASSERT_EQ(decision.blocking_robots.size(), 1U);
    EXPECT_EQ(decision.blocking_robots.front(), RobotId{"R01"});

    // §15: when it will be worth asking again.
    ASSERT_TRUE(decision.next_available.has_value());
    EXPECT_EQ(decision.next_available.value(), at(20));

    // A WAIT is not a claim. Recording one would hold the corridor for a robot
    // that was told to stop.
    EXPECT_EQ(fixture.manager.get_reservation(ReservationId{"RES-02"}), nullptr);
    EXPECT_EQ(fixture.manager.metrics().waited, 1U);
}

TEST(ReservationManager, manager_retry_after_release_is_granted) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 20, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(1)).has_value());
    ASSERT_EQ(fixture.ask(request("RES-02", "R02", "CORRIDOR-01", 5, 10), 2).status,
              DecisionStatus::wait);

    ASSERT_TRUE(fixture.manager.release(ReservationId{"RES-01"}, at(6)).has_value());

    // The same request id: nothing was written for it the first time, so the
    // retry is a fresh decision rather than a replay.
    const ReservationDecision decision =
        fixture.ask(request("RES-02", "R02", "CORRIDOR-01", 7, 10), 7);

    EXPECT_TRUE(is_granted(decision));
    EXPECT_FALSE(decision.replayed);
}

// ------------------------------------------------------------- concurrency

TEST(ReservationManager, manager_concurrent_requests_do_not_exceed_capacity) {
    Fixture fixture;

    // §33: three robots asking for one single-lane corridor at once.
    std::vector<ReservationRequest> batch = {
        request("RES-01", "R01", "CORRIDOR-01", 10, 10),
        request("RES-02", "R02", "CORRIDOR-01", 10, 10),
        request("RES-03", "R03", "CORRIDOR-01", 10, 10),
    };

    const std::vector<ReservationDecision> decisions =
        fixture.manager.request_all(std::move(batch), at(0));

    ASSERT_EQ(decisions.size(), 3U);
    int granted = 0;
    for (const ReservationDecision& decision : decisions) {
        granted += is_granted(decision) ? 1 : 0;
    }
    EXPECT_EQ(granted, 1);
    EXPECT_EQ(fixture.table.holding_count(), 1U);
}

TEST(ReservationManager, manager_higher_priority_request_is_decided_first) {
    Fixture fixture;

    std::vector<ReservationRequest> batch = {
        with_priority(request("RES-01", "R01", "CORRIDOR-01", 10, 10), 1),
        with_priority(request("RES-02", "R02", "CORRIDOR-01", 10, 10), 5),
    };

    const std::vector<ReservationDecision> decisions =
        fixture.manager.request_all(std::move(batch), at(0));

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_EQ(decisions[0].robot_id, RobotId{"R02"});
    EXPECT_TRUE(is_granted(decisions[0]));
    EXPECT_EQ(decisions[1].status, DecisionStatus::wait);
}

TEST(ReservationManager, manager_equal_priority_is_broken_the_same_way_every_run) {
    // docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §16. Two fixtures, the same two
    // requests, opposite input order: the answer must not depend on which one
    // the caller happened to put first.
    Fixture first;
    Fixture second;

    std::vector<ReservationRequest> forwards = {
        request("RES-01", "R01", "CORRIDOR-01", 10, 10),
        request("RES-02", "R02", "CORRIDOR-01", 10, 10),
    };
    std::vector<ReservationRequest> backwards = {
        request("RES-02", "R02", "CORRIDOR-01", 10, 10),
        request("RES-01", "R01", "CORRIDOR-01", 10, 10),
    };

    const std::vector<ReservationDecision> left =
        first.manager.request_all(std::move(forwards), at(0));
    const std::vector<ReservationDecision> right =
        second.manager.request_all(std::move(backwards), at(0));

    ASSERT_EQ(left.size(), 2U);
    ASSERT_EQ(right.size(), 2U);
    EXPECT_EQ(left[0].robot_id, right[0].robot_id);
    EXPECT_EQ(left[0].status, right[0].status);
    EXPECT_EQ(left[1].robot_id, right[1].robot_id);
    EXPECT_EQ(left[1].status, right[1].status);
}

TEST(ReservationManager, manager_waiting_raises_effective_priority) {
    ReservationPolicy policy = test::unbuffered_policy();
    policy.aging_per_second = 1.0;
    Fixture fixture{policy};

    // R09 has the corridor until 10.
    fixture.own("RES-09", "R09", "CORRIDOR-01", 0, 10, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-09"}, at(0)).has_value());

    // R01 has been asking since 0 and getting nowhere.
    ASSERT_EQ(fixture.ask(request("RES-01", "R01", "CORRIDOR-01", 0, 10), 0).status,
              DecisionStatus::wait);
    ASSERT_TRUE(fixture.manager.release(ReservationId{"RES-09"}, at(10)).has_value());

    // At 10 it is up against a robot with five times its base priority — and
    // ten seconds of waiting outweigh that. §17: this is what stops a
    // low-priority robot from waiting for ever.
    std::vector<ReservationRequest> batch = {
        with_priority(request("RES-01", "R01", "CORRIDOR-01", 10, 10), 0),
        with_priority(request("RES-02", "R02", "CORRIDOR-01", 10, 10), 5),
    };
    const std::vector<ReservationDecision> decisions =
        fixture.manager.request_all(std::move(batch), at(10));

    ASSERT_EQ(decisions.size(), 2U);
    EXPECT_EQ(decisions[0].robot_id, RobotId{"R01"});
    EXPECT_TRUE(is_granted(decisions[0]));
    EXPECT_EQ(decisions[0].effective_priority, Priority{10});
}

// --------------------------------------------------------------- idempotency

TEST(ReservationManager, manager_repeated_request_makes_one_reservation) {
    Fixture fixture;

    const ReservationRequest once = request("RES-01", "R01", "CORRIDOR-01", 10, 10);
    const ReservationDecision first = fixture.ask(once, 0);
    const ReservationDecision again = fixture.ask(once, 3);

    // §35. A retransmitted request is the same request.
    ASSERT_TRUE(is_granted(first));
    EXPECT_TRUE(is_granted(again));
    EXPECT_TRUE(again.replayed);
    EXPECT_EQ(again.window_start, first.window_start);
    EXPECT_EQ(again.window_end, first.window_end);
    EXPECT_EQ(fixture.table.size(), 1U);
    EXPECT_EQ(fixture.manager.metrics().granted, 1U);
    EXPECT_EQ(fixture.manager.metrics().replayed, 1U);
}

TEST(ReservationManager, manager_repeated_request_for_a_released_claim_is_rejected) {
    Fixture fixture;
    const ReservationRequest once = request("RES-01", "R01", "CORRIDOR-01", 0, 10);
    ASSERT_TRUE(is_granted(fixture.ask(once, 0)));
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(1)).has_value());
    ASSERT_TRUE(fixture.manager.release(ReservationId{"RES-01"}, at(5)).has_value());

    const ReservationDecision again = fixture.ask(once, 6);

    // docs/24_DOMAIN_MODEL.md §30. A late duplicate must not revive a finished
    // claim: another robot may be in the corridor by now.
    EXPECT_EQ(again.status, DecisionStatus::rejected);
    EXPECT_EQ(again.reason, DenialReason::already_terminal);
    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::released);
}

TEST(ReservationManager, manager_reused_identifier_for_another_robot_is_rejected) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 10, 0);

    const ReservationDecision stolen =
        fixture.ask(request("RES-01", "R02", "CORRIDOR-01", 0, 10), 1);

    // Answering this with R01's grant would tell R02 it owns the corridor.
    EXPECT_EQ(stolen.status, DecisionStatus::rejected);
    EXPECT_EQ(stolen.reason, DenialReason::duplicate_request);
}

// ------------------------------------------------------------------ release

TEST(ReservationManager, manager_release_frees_the_resource) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 20, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(1)).has_value());

    ASSERT_TRUE(fixture.manager.release(ReservationId{"RES-01"}, at(6)).has_value());

    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::released);
    EXPECT_TRUE(fixture.manager.active_reservations(ResourceId{"CORRIDOR-01"}).empty());
    EXPECT_TRUE(is_granted(fixture.ask(request("RES-02", "R02", "CORRIDOR-01", 7, 10), 7)));
    EXPECT_EQ(fixture.manager.metrics().released, 1U);
}

TEST(ReservationManager, manager_grant_that_was_never_entered_is_cancelled_not_released) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 20, 0);

    const auto released = fixture.manager.release(ReservationId{"RES-01"}, at(1));

    // The lifecycle only allows active -> released. Counting a grant nobody
    // took up as a release would hide how often robots are told to go and do
    // not.
    ASSERT_FALSE(released.has_value());
    EXPECT_EQ(released.error(), ReservationError::invalid_transition);

    ASSERT_TRUE(fixture.manager.cancel(ReservationId{"RES-01"}, at(1)).has_value());
    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::cancelled);
}

TEST(ReservationManager, manager_failed_robot_gives_everything_back) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 20, 0);
    fixture.own("RES-02", "R01", "CORRIDOR-02", 0, 20, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(1)).has_value());

    const std::vector<ReservationId> gone = fixture.manager.release_all_for(RobotId{"R01"}, at(5));

    // A claim nobody will ever release blocks the resource for good, which is
    // the failure mode docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §21 is about.
    ASSERT_EQ(gone.size(), 2U);
    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::released);
    EXPECT_EQ(fixture.state_of("RES-02"), ReservationState::cancelled);
    EXPECT_EQ(fixture.table.holding_count(), 0U);
}

TEST(ReservationManager, manager_committed_claim_cannot_be_cancelled) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 20, 0);
    ASSERT_TRUE(fixture.manager.commit(ReservationId{"RES-01"}).has_value());

    const auto cancelled = fixture.manager.cancel(ReservationId{"RES-01"}, at(2));

    // §30. Past the commit point the robot is going in; withdrawing the
    // reservation removes the record that the corridor is occupied and nothing
    // else.
    ASSERT_FALSE(cancelled.has_value());
    EXPECT_EQ(cancelled.error(), ReservationError::committed);
    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::pending);
}

// ---------------------------------------------------------------- expiration

TEST(ReservationManager, manager_unused_grant_expires) {
    ReservationPolicy policy = test::unbuffered_policy();
    policy.grant_timeout = seconds(5);
    Fixture fixture{policy};
    fixture.own("RES-01", "R01", "CORRIDOR-01", 10, 10, 0);

    const SweepReport report = fixture.manager.sweep(at(6));

    // §22. The robot never entered, so taking the grant back cannot strand it.
    ASSERT_EQ(report.expired.size(), 1U);
    EXPECT_EQ(report.expired.front(), ReservationId{"RES-01"});
    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::expired);
    EXPECT_TRUE(report.overrunning.empty());
}

TEST(ReservationManager, manager_grant_within_its_timeout_survives_a_sweep) {
    ReservationPolicy policy = test::unbuffered_policy();
    policy.grant_timeout = seconds(30);
    Fixture fixture{policy};
    fixture.own("RES-01", "R01", "CORRIDOR-01", 10, 10, 0);

    const SweepReport report = fixture.manager.sweep(at(5));

    EXPECT_TRUE(report.expired.empty());
    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::pending);
}

TEST(ReservationManager, manager_active_claim_past_its_window_is_reported_not_expired) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 10, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(1)).has_value());

    const SweepReport report = fixture.manager.sweep(at(15));

    // §22 forbids releasing on a timer without checking the real state. The
    // robot may be standing in the corridor right now; freeing it here would
    // send the next robot in after it.
    EXPECT_TRUE(report.expired.empty());
    ASSERT_EQ(report.overrunning.size(), 1U);
    EXPECT_EQ(report.overrunning.front(), ReservationId{"RES-01"});
    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::active);

    // A window that has ended stops excluding anyone — which is exactly why
    // the overrun has to be reported rather than shrugged at. Only an
    // extension (§18) keeps the corridor held while the robot is still in it.
    EXPECT_TRUE(is_granted(fixture.ask(request("RES-02", "R02", "CORRIDOR-01", 15, 5), 15)));
}

TEST(ReservationManager, manager_active_claim_expires_when_the_caller_says_so) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 10, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(1)).has_value());

    // The path §22 does allow, once somebody has established that the robot is
    // not in there.
    ASSERT_TRUE(fixture.manager.expire(ReservationId{"RES-01"}, at(15)).has_value());

    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::expired);
    EXPECT_EQ(fixture.table.holding_count(), 0U);
    EXPECT_EQ(fixture.manager.metrics().expired, 1U);
}

// ----------------------------------------------------------------- extension

TEST(ReservationManager, manager_extension_moves_the_window_end) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 10, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(1)).has_value());

    const ExtensionOutcome outcome = fixture.manager.extend(ReservationId{"RES-01"}, at(20), at(9));

    EXPECT_TRUE(outcome.granted);
    EXPECT_EQ(outcome.new_end, at(20));
    EXPECT_TRUE(outcome.revoked.empty());
    EXPECT_EQ(fixture.manager.metrics().extended, 1U);

    // The longer hold is enforced, not merely recorded.
    EXPECT_FALSE(is_granted(fixture.ask(request("RES-02", "R02", "CORRIDOR-01", 15, 3), 9)));
}

TEST(ReservationManager, manager_shortening_is_not_an_extension) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 10, 0);

    const ExtensionOutcome outcome = fixture.manager.extend(ReservationId{"RES-01"}, at(5), at(2));

    EXPECT_FALSE(outcome.granted);
    EXPECT_EQ(outcome.error, ReservationError::not_an_extension);
}

TEST(ReservationManager, manager_extension_cancels_the_grant_queued_behind_it) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 10, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(1)).has_value());
    fixture.own("RES-02", "R02", "CORRIDOR-01", 10, 10, 1);

    // §18~20: R01 stopped for a person and is still inside. Refusing the
    // extension would not free the corridor, only make the table disagree with
    // the floor.
    const ExtensionOutcome outcome = fixture.manager.extend(ReservationId{"RES-01"}, at(15), at(9));

    EXPECT_TRUE(outcome.granted);
    ASSERT_EQ(outcome.revoked.size(), 1U);
    EXPECT_EQ(outcome.revoked.front(), ReservationId{"RES-02"});
    ASSERT_EQ(outcome.affected_robots.size(), 1U);
    EXPECT_EQ(outcome.affected_robots.front(), RobotId{"R02"});
    EXPECT_EQ(fixture.state_of("RES-02"), ReservationState::cancelled);
}

TEST(ReservationManager, manager_extension_blocked_by_a_robot_already_inside) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 10, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(1)).has_value());
    fixture.own("RES-02", "R02", "CORRIDOR-01", 10, 10, 1);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-02"}, at(10)).has_value());

    const ExtensionOutcome outcome =
        fixture.manager.extend(ReservationId{"RES-01"}, at(15), at(10));

    // Two robots are physically involved. Rewriting a window does not settle
    // that; §20's "WAIT / REPLAN" does, and it is the controller's to do.
    EXPECT_FALSE(outcome.granted);
    EXPECT_EQ(outcome.error, ReservationError::extension_blocked);
    ASSERT_EQ(outcome.affected_robots.size(), 1U);
    EXPECT_EQ(outcome.affected_robots.front(), RobotId{"R02"});
    EXPECT_EQ(fixture.state_of("RES-02"), ReservationState::active);

    const ReservationRecord* unchanged = fixture.manager.get_reservation(ReservationId{"RES-01"});
    ASSERT_NE(unchanged, nullptr);
    EXPECT_EQ(unchanged->reservation.window.end(), at(10));
}

TEST(ReservationManager, manager_extension_into_a_wide_corridor_displaces_nobody) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-02", 0, 10, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(1)).has_value());
    fixture.own("RES-02", "R02", "CORRIDOR-02", 10, 10, 1);

    // CORRIDOR-02 holds two. There is room for the overrun, so nothing is
    // taken from anybody.
    const ExtensionOutcome outcome = fixture.manager.extend(ReservationId{"RES-01"}, at(15), at(9));

    EXPECT_TRUE(outcome.granted);
    EXPECT_TRUE(outcome.revoked.empty());
    EXPECT_EQ(fixture.state_of("RES-02"), ReservationState::pending);
}

// ---------------------------------------------------------- affected robots

TEST(ReservationManager, manager_affected_robots_are_the_other_claimants) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-02", 0, 20, 0);
    fixture.own("RES-02", "R02", "CORRIDOR-02", 5, 10, 0);
    fixture.own("RES-03", "R03", "CORRIDOR-01", 0, 20, 0);

    const std::vector<RobotId> affected =
        fixture.manager.affected_robots(ResourceId{"CORRIDOR-02"}, ReservationId{"RES-01"});

    // §25. The owner is not affected by its own claim, and a robot on another
    // resource is not affected at all.
    ASSERT_EQ(affected.size(), 1U);
    EXPECT_EQ(affected.front(), RobotId{"R02"});
}

TEST(ReservationManager, manager_find_conflicts_answers_for_a_window) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 10, 0);

    // §24, asked without a request in hand.
    EXPECT_EQ(
        fixture.manager.find_conflicts(ResourceId{"CORRIDOR-01"}, domain::TimeWindow{at(5), at(15)})
            .size(),
        1U);
    EXPECT_TRUE(fixture.manager
                    .find_conflicts(ResourceId{"CORRIDOR-01"}, domain::TimeWindow{at(10), at(15)})
                    .empty());
}

// ------------------------------------------------------------- wait-for graph

TEST(ReservationManager, manager_publishes_the_wait_chain) {
    Fixture fixture;
    fixture.own("RES-01", "R02", "CORRIDOR-01", 0, 20, 0);
    fixture.own("RES-02", "R03", "CORRIDOR-02", 0, 20, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-02"}, at(0)).has_value());

    // §26's chain: R01 waits for C01, which R02 owns; R02 waits for C02, which
    // R03 owns. Two robots would have to hold CORRIDOR-02 for R02 to be
    // refused, so it is filled first.
    fixture.own("RES-03", "R04", "CORRIDOR-02", 0, 20, 0);
    ASSERT_EQ(fixture.ask(request("RES-04", "R01", "CORRIDOR-01", 5, 10), 1).status,
              DecisionStatus::wait);
    ASSERT_EQ(fixture.ask(request("RES-05", "R02", "CORRIDOR-02", 5, 10), 1).status,
              DecisionStatus::wait);

    const std::vector<WaitEdge> edges = fixture.manager.wait_for_edges();

    ASSERT_EQ(edges.size(), 3U);
    EXPECT_EQ(edges[0].waiting_robot, RobotId{"R01"});
    EXPECT_EQ(edges[0].holding_robot, RobotId{"R02"});
    EXPECT_EQ(edges[0].resource, ResourceId{"CORRIDOR-01"});
    EXPECT_EQ(edges[1].waiting_robot, RobotId{"R02"});
    EXPECT_EQ(edges[1].holding_robot, RobotId{"R03"});
    EXPECT_EQ(edges[2].waiting_robot, RobotId{"R02"});
    EXPECT_EQ(edges[2].holding_robot, RobotId{"R04"});
}

TEST(ReservationManager, manager_wait_ends_when_the_claim_is_granted) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 10, 0);
    ASSERT_TRUE(fixture.manager.activate(ReservationId{"RES-01"}, at(0)).has_value());
    ASSERT_EQ(fixture.ask(request("RES-02", "R02", "CORRIDOR-01", 0, 10), 1).status,
              DecisionStatus::wait);
    ASSERT_FALSE(fixture.manager.wait_for_edges().empty());

    ASSERT_TRUE(fixture.manager.release(ReservationId{"RES-01"}, at(9)).has_value());
    ASSERT_TRUE(is_granted(fixture.ask(request("RES-02", "R02", "CORRIDOR-01", 10, 10), 10)));

    // A stale wait edge is a deadlock report about a robot that is moving.
    EXPECT_TRUE(fixture.manager.wait_for_edges().empty());
    EXPECT_EQ(fixture.manager.waited_for(RobotId{"R02"}, ResourceId{"CORRIDOR-01"}, at(20)),
              core::Duration::zero());
}

// -------------------------------------------------------------------- log

TEST(ReservationManager, manager_records_every_decision) {
    Fixture fixture;
    fixture.own("RES-01", "R01", "CORRIDOR-01", 0, 10, 0);
    ASSERT_EQ(fixture.ask(request("RES-02", "R02", "CORRIDOR-01", 5, 10), 1).status,
              DecisionStatus::wait);

    const std::span<const ReservationDecision> log = fixture.manager.decision_log();

    // §36: robot, resource, decision, reason, owner, expected availability.
    ASSERT_EQ(log.size(), 2U);
    EXPECT_EQ(log[0].status, DecisionStatus::granted);
    EXPECT_EQ(log[1].status, DecisionStatus::wait);
    EXPECT_EQ(log[1].robot_id, RobotId{"R02"});
    EXPECT_EQ(log[1].reason, DenialReason::resource_occupied);
    ASSERT_EQ(log[1].blocking_robots.size(), 1U);
    EXPECT_EQ(log[1].blocking_robots.front(), RobotId{"R01"});
}

TEST(ReservationManager, manager_decision_log_is_bounded) {
    ReservationPolicy policy = test::unbuffered_policy();
    policy.decision_log_capacity = 2;
    Fixture fixture{policy};

    for (int index = 0; index < 5; ++index) {
        const ReservationDecision decision = fixture.ask(
            request("RES-0" + std::to_string(index), "R01", "CORRIDOR-01", index * 10, 5),
            index * 10);
        ASSERT_TRUE(is_granted(decision));
    }

    // A log that grows for the life of the process is a leak in a system meant
    // to run for weeks.
    EXPECT_EQ(fixture.manager.decision_log().size(), 2U);
    EXPECT_EQ(fixture.manager.decision_log()[1].reservation_id, ReservationId{"RES-04"});
}

// --------------------------------------------------------------- recovery

TEST(ReservationManager, manager_recovery_restores_consistent_claims) {
    Fixture fixture;

    const std::vector<ReservationRecord> persisted = {
        test::record("RES-01", "R01", "CORRIDOR-01", 0, 20, ReservationState::active),
        test::record("RES-02", "R02", "CORRIDOR-02", 0, 20, ReservationState::pending),
    };

    const RecoveryReport report = fixture.manager.recover(persisted, at(1));

    // §32, and §38's Test 7.
    EXPECT_EQ(report.restored.size(), 2U);
    EXPECT_TRUE(report.rejected.empty());
    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::active);
    EXPECT_EQ(fixture.manager.active_reservations(ResourceId{"CORRIDOR-01"}).size(), 1U);

    // And the restored claims refuse what they refused before the restart.
    EXPECT_EQ(fixture.ask(request("RES-03", "R03", "CORRIDOR-01", 5, 5), 1).status,
              DecisionStatus::wait);
}

TEST(ReservationManager, manager_recovery_refuses_claims_that_contradict_each_other) {
    Fixture fixture;

    const std::vector<ReservationRecord> persisted = {
        test::record("RES-01", "R01", "CORRIDOR-01", 0, 20, ReservationState::active),
        test::record("RES-02", "R02", "CORRIDOR-01", 5, 25, ReservationState::active),
    };

    const RecoveryReport report = fixture.manager.recover(persisted, at(1));

    // Restoring both would put two robots in a single-lane corridor on the
    // strength of a file. §32 sends the loser back to be re-derived from where
    // the robot actually is.
    ASSERT_EQ(report.restored.size(), 1U);
    EXPECT_EQ(report.restored.front(), ReservationId{"RES-01"});
    ASSERT_EQ(report.rejected.size(), 1U);
    EXPECT_EQ(report.rejected.front(), ReservationId{"RES-02"});
}

TEST(ReservationManager, manager_recovery_expires_a_grant_that_outlived_its_window) {
    Fixture fixture;

    const std::vector<ReservationRecord> persisted = {
        test::record("RES-01", "R01", "CORRIDOR-01", 0, 10, ReservationState::pending),
    };

    const RecoveryReport report = fixture.manager.recover(persisted, at(50));

    ASSERT_EQ(report.expired.size(), 1U);
    EXPECT_EQ(report.expired.front(), ReservationId{"RES-01"});
    EXPECT_TRUE(report.restored.empty());
    EXPECT_EQ(fixture.state_of("RES-01"), ReservationState::expired);
}

TEST(ReservationManager, manager_recovery_is_the_same_whichever_order_the_records_arrive) {
    Fixture first;
    Fixture second;

    const ReservationRecord early =
        test::record("RES-01", "R01", "CORRIDOR-01", 0, 20, ReservationState::active);
    const ReservationRecord late =
        test::record("RES-02", "R02", "CORRIDOR-01", 5, 25, ReservationState::active);

    const RecoveryReport forwards = first.manager.recover({early, late}, at(1));
    const RecoveryReport backwards = second.manager.recover({late, early}, at(1));

    // The persisted order is whatever a file happened to hold. The claim that
    // started first wins either way.
    EXPECT_EQ(forwards.restored, backwards.restored);
    EXPECT_EQ(forwards.rejected, backwards.rejected);
}

}  // namespace
}  // namespace traffic::reservation
