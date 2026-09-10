/// The state manager. docs/23_SYSTEM_ARCHITECTURE.md §22,
/// docs/22_IMPLEMENTATION_WORKFLOW.md Phase 3, docs/04_ROBOT_TASK_MODEL.md
/// §20~22.
///
/// Phase 3's completion criteria are State Update, State Query, State Version,
/// State Transition and Stale State Detection, and docs/04_ROBOT_TASK_MODEL.md
/// §25 names test_robot_state_transition, test_invalid_robot_transition,
/// test_waiting_state, test_robot_timeout and test_route_assignment. All of
/// them are here.

#include "traffic/state/state_manager.h"

#include <chrono>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/core/ids.h"
#include "traffic/domain/robot.h"
#include "traffic/domain/robot_state.h"
#include "traffic/domain/values.h"

namespace traffic::state {
namespace {

using core::EdgeId;
using core::NodeId;
using core::RobotId;
using domain::Position;
using domain::RobotState;

core::Duration seconds(int count) {
    return std::chrono::duration_cast<core::Duration>(core::Seconds{count});
}

core::TimePoint at(int second) {
    return core::kTimeOrigin + seconds(second);
}

domain::Robot robot(std::string id) {
    domain::Robot value;
    value.id = RobotId{std::move(id)};
    value.capabilities.width = 0.8;
    value.capabilities.length = 1.2;
    value.capabilities.max_speed = 1.5;
    return value;
}

RobotStateUpdate telemetry(std::string id, int second, Position position) {
    RobotStateUpdate update;
    update.robot_id = RobotId{std::move(id)};
    update.timestamp = at(second);
    update.position = position;
    update.battery = 75.0;
    return update;
}

/// Registers R01 and R02 at t=0.
///
/// Fills a manager rather than returning one: StateManager holds the fleet
/// and is deliberately neither copyable nor movable, so there is no such thing
/// as a spare one to hand back.
void register_two(StateManager& manager) {
    EXPECT_TRUE(manager.register_robot(robot("R01"), at(0)).has_value());
    EXPECT_TRUE(manager.register_robot(robot("R02"), at(0)).has_value());
}

/// Puts a robot on the road, since `idle` has no progress to interrupt and so
/// cannot go straight to `waiting`.
void set_moving(StateManager& manager, const char* id, int second) {
    EXPECT_TRUE(manager.assign_state(RobotId{id}, RobotState::moving, at(second), std::nullopt)
                    .has_value());
}

WaitingContext held_on(std::string resource, int since) {
    WaitingContext context;
    context.reason = WaitingReason::resource_occupied;
    context.resource = core::ResourceId{std::move(resource)};
    context.since = at(since);
    return context;
}

// ==================================================================== register

TEST(StateRegister, state_register_adds_a_robot_in_idle) {
    StateManager manager;

    ASSERT_TRUE(manager.register_robot(robot("R01"), at(0)).has_value());

    const domain::RobotStateSnapshot* snapshot = manager.snapshot(RobotId{"R01"});
    ASSERT_NE(snapshot, nullptr);
    EXPECT_EQ(snapshot->state, RobotState::idle);
    EXPECT_EQ(snapshot->observed_at, at(0));
    EXPECT_TRUE(manager.is_registered(RobotId{"R01"}));
    EXPECT_EQ(manager.robot_count(), 1U);
}

TEST(StateRegister, state_register_keeps_the_robot_capabilities) {
    StateManager manager;
    ASSERT_TRUE(manager.register_robot(robot("R01"), at(0)).has_value());

    const RobotRecord* record = manager.record(RobotId{"R01"});
    ASSERT_NE(record, nullptr);
    EXPECT_DOUBLE_EQ(record->robot.capabilities.width, 0.8);
}

TEST(StateRegister, state_register_refuses_an_unnamed_robot) {
    StateManager manager;

    const auto status = manager.register_robot(domain::Robot{}, at(0));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), RegistrationRejection::empty_robot_id);
}

TEST(StateRegister, state_register_refuses_a_duplicate) {
    // Re-registering would silently reset whatever the robot was doing. A
    // fleet that registers the same id twice has a configuration problem, and
    // hiding it behind an update makes it harder to find.
    StateManager manager;
    ASSERT_TRUE(manager.register_robot(robot("R01"), at(0)).has_value());

    const auto status = manager.register_robot(robot("R01"), at(5));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), RegistrationRejection::already_registered);
    EXPECT_EQ(manager.robot_count(), 1U);
}

TEST(StateRegister, state_register_deregisters_a_robot) {
    StateManager manager;
    register_two(manager);

    EXPECT_TRUE(manager.deregister_robot(RobotId{"R01"}));
    EXPECT_FALSE(manager.is_registered(RobotId{"R01"}));
    EXPECT_EQ(manager.robot_count(), 1U);
    EXPECT_FALSE(manager.deregister_robot(RobotId{"R01"}));
}

TEST(StateRegisterName, registration_rejection_every_value_has_a_name) {
    for (const RegistrationRejection rejection :
         {RegistrationRejection::empty_robot_id, RegistrationRejection::already_registered}) {
        EXPECT_NE(to_string(rejection), "UNKNOWN");
    }
}

// ====================================================================== update

TEST(StateUpdate, state_update_records_what_the_robot_reported) {
    StateManager manager;
    register_two(manager);

    RobotStateUpdate update = telemetry("R01", 5, Position{3.0, 4.0, 0.0});
    update.current_node = NodeId{"N7"};
    update.velocity = domain::Velocity{1.0, 0.0};
    update.reported_state = RobotState::moving;

    ASSERT_TRUE(manager.apply(update).has_value());

    const domain::RobotStateSnapshot* snapshot = manager.snapshot(RobotId{"R01"});
    ASSERT_NE(snapshot, nullptr);
    EXPECT_EQ(snapshot->state, RobotState::moving);
    EXPECT_EQ(snapshot->position, (Position{3.0, 4.0, 0.0}));
    EXPECT_EQ(snapshot->current_node, NodeId{"N7"});
    EXPECT_DOUBLE_EQ(snapshot->battery, 75.0);
    EXPECT_EQ(snapshot->observed_at, at(5));
}

TEST(StateUpdate, state_update_without_a_state_leaves_the_state_alone) {
    // The common case. Position and battery change constantly; the state
    // rarely does, so most messages carry none.
    StateManager manager;
    register_two(manager);
    ASSERT_TRUE(manager.assign_state(RobotId{"R01"}, RobotState::reserving, at(1), std::nullopt)
                    .has_value());

    ASSERT_TRUE(manager.apply(telemetry("R01", 5, Position{1.0, 0.0, 0.0})).has_value());

    EXPECT_EQ(manager.snapshot(RobotId{"R01"})->state, RobotState::reserving);
}

TEST(StateUpdate, state_update_for_an_unregistered_robot_is_refused) {
    // Not a state change — a configuration problem. Creating a record on
    // demand would let a typo add a robot to the fleet.
    StateManager manager;
    register_two(manager);

    const auto status = manager.apply(telemetry("R99", 5, Position{}));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::unknown_robot);
    EXPECT_EQ(manager.robot_count(), 2U);
}

TEST(StateUpdate, state_update_older_than_what_is_held_is_refused) {
    // Applying it would move the fleet's picture backwards, and a decision
    // taken on the result would describe a world that has already changed.
    StateManager manager;
    register_two(manager);
    ASSERT_TRUE(manager.apply(telemetry("R01", 10, Position{5.0, 0.0, 0.0})).has_value());

    const auto status = manager.apply(telemetry("R01", 4, Position{1.0, 0.0, 0.0}));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::stale_timestamp);
    EXPECT_EQ(manager.snapshot(RobotId{"R01"})->position, (Position{5.0, 0.0, 0.0}));
}

TEST(StateUpdate, state_update_at_the_same_instant_is_accepted) {
    // A robot may split one measurement across two messages. Refusing the
    // second would drop half of it.
    StateManager manager;
    register_two(manager);
    ASSERT_TRUE(manager.apply(telemetry("R01", 10, Position{5.0, 0.0, 0.0})).has_value());

    EXPECT_TRUE(manager.apply(telemetry("R01", 10, Position{5.0, 0.0, 0.0})).has_value());
}

TEST(StateUpdate, state_update_refuses_a_state_the_machine_rejects) {
    // docs/00_INDEX.md D-001: recovery from `failed` has to go through `idle`
    // so the robot's resources are released. Jumping back to `moving` leaves
    // the reservation table holding entries no robot owns.
    StateManager manager;
    register_two(manager);
    ASSERT_TRUE(
        manager.assign_state(RobotId{"R01"}, RobotState::failed, at(1), std::nullopt).has_value());

    RobotStateUpdate update = telemetry("R01", 5, Position{});
    update.reported_state = RobotState::moving;

    const auto status = manager.apply(update);

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::invalid_transition);
    EXPECT_EQ(manager.snapshot(RobotId{"R01"})->state, RobotState::failed);
}

TEST(StateUpdate, state_update_refuses_a_robot_that_claims_a_controller_state) {
    // The rule of D-009. A robot reporting `waiting` is asserting we granted
    // it something.
    StateManager manager;
    register_two(manager);

    RobotStateUpdate update = telemetry("R01", 5, Position{});
    update.reported_state = RobotState::waiting;

    const auto status = manager.apply(update);

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::not_robot_reportable);
}

TEST(StateUpdate, state_update_records_the_last_acknowledged_command) {
    // docs/04_ROBOT_TASK_MODEL.md §22: how we tell "it has not started yet"
    // from "it did not hear us".
    StateManager manager;
    register_two(manager);

    RobotStateUpdate update = telemetry("R01", 5, Position{});
    update.last_ack_command_id = core::CommandId{"CMD-9"};

    ASSERT_TRUE(manager.apply(update).has_value());

    EXPECT_EQ(manager.record(RobotId{"R01"})->last_ack_command, core::CommandId{"CMD-9"});
}

TEST(StateUpdate, state_update_keeps_the_controller_timestamp_apart_from_the_robots) {
    // §22 again. The gap between the two is the thing worth seeing.
    StateManager manager;
    register_two(manager);

    ASSERT_TRUE(manager.apply(telemetry("R01", 7, Position{})).has_value());

    const RobotRecord* record = manager.record(RobotId{"R01"});
    ASSERT_NE(record, nullptr);
    EXPECT_EQ(record->snapshot.observed_at, at(7));
    EXPECT_EQ(record->controller_timestamp, at(7));
}

TEST(StateUpdate, state_update_rejects_a_malformed_message_before_touching_anything) {
    StateManager manager;
    register_two(manager);
    const domain::StateVersion before = manager.version();

    RobotStateUpdate update = telemetry("R01", 5, Position{});
    update.battery = 200.0;

    EXPECT_EQ(manager.apply(update).error(), UpdateRejection::battery_out_of_range);
    EXPECT_EQ(manager.version(), before);
}

// ===================================================== controller assignment

TEST(StateAssign, state_assign_sets_a_state_only_traffic_control_may_set) {
    StateManager manager;
    register_two(manager);

    ASSERT_TRUE(manager.assign_state(RobotId{"R01"}, RobotState::reserving, at(3), std::nullopt)
                    .has_value());

    EXPECT_EQ(manager.snapshot(RobotId{"R01"})->state, RobotState::reserving);
}

TEST(StateAssign, state_assign_records_why_a_robot_is_waiting) {
    // docs/04_ROBOT_TASK_MODEL.md §7. The reason decides the response: a wait
    // on a busy corridor is the system working, a wait on a person is not
    // something more planning can fix.
    StateManager manager;
    register_two(manager);
    set_moving(manager, "R01", 2);

    ASSERT_TRUE(
        manager.assign_state(RobotId{"R01"}, RobotState::waiting, at(3), held_on("CORRIDOR-01", 3))
            .has_value());

    const RobotRecord* record = manager.record(RobotId{"R01"});
    ASSERT_NE(record, nullptr);
    ASSERT_TRUE(record->waiting.has_value());
    EXPECT_EQ(record->waiting->reason, WaitingReason::resource_occupied);
    EXPECT_EQ(record->waiting->resource, core::ResourceId{"CORRIDOR-01"});
    EXPECT_EQ(waited_for(*record->waiting, at(13)), seconds(10));
}

TEST(StateAssign, state_assign_refuses_to_hold_a_robot_without_saying_why) {
    // A hold with no reason cannot be explained afterwards.
    StateManager manager;
    register_two(manager);
    set_moving(manager, "R01", 2);

    const auto status =
        manager.assign_state(RobotId{"R01"}, RobotState::waiting, at(3), std::nullopt);

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::waiting_reason_mismatch);
    EXPECT_EQ(manager.snapshot(RobotId{"R01"})->state, RobotState::moving);
}

TEST(StateAssign, state_assign_refuses_a_waiting_reason_for_a_robot_that_is_not_waiting) {
    StateManager manager;
    register_two(manager);

    const auto status =
        manager.assign_state(RobotId{"R01"}, RobotState::moving, at(3), held_on("CORRIDOR-01", 3));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::waiting_reason_mismatch);
}

TEST(StateAssign, state_assign_clears_the_waiting_reason_when_the_robot_moves_again) {
    // Leaving it behind would have the robot show up in the next report of
    // who is held and why.
    StateManager manager;
    register_two(manager);
    set_moving(manager, "R01", 2);
    ASSERT_TRUE(
        manager.assign_state(RobotId{"R01"}, RobotState::waiting, at(3), held_on("CORRIDOR-01", 3))
            .has_value());

    RobotStateUpdate resumed = telemetry("R01", 8, Position{1.0, 0.0, 0.0});
    resumed.reported_state = RobotState::moving;
    ASSERT_TRUE(manager.apply(resumed).has_value());

    EXPECT_FALSE(manager.record(RobotId{"R01"})->waiting.has_value());
}

TEST(StateAssign, state_assign_cannot_hold_a_robot_that_was_not_going_anywhere) {
    // `idle` has no progress to interrupt, so it cannot be waiting. A robot
    // with no task is not being held by traffic control; it is simply free.
    StateManager manager;
    register_two(manager);

    const auto status =
        manager.assign_state(RobotId{"R01"}, RobotState::waiting, at(3), held_on("CORRIDOR-01", 3));

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::invalid_transition);
}

TEST(StateAssign, state_assign_refuses_an_illegal_transition) {
    StateManager manager;
    register_two(manager);

    // `idle` has no progress to interrupt, so it cannot be blocked.
    const auto status =
        manager.assign_state(RobotId{"R01"}, RobotState::blocked, at(3), std::nullopt);

    ASSERT_FALSE(status.has_value());
    EXPECT_EQ(status.error(), UpdateRejection::invalid_transition);
}

TEST(StateAssign, state_assign_refuses_an_unregistered_robot) {
    StateManager manager;
    register_two(manager);

    EXPECT_EQ(manager.assign_state(RobotId{"R99"}, RobotState::moving, at(3), std::nullopt).error(),
              UpdateRejection::unknown_robot);
}

// ================================================================ association

TEST(StateAssociation, state_association_attaches_a_route_to_a_robot) {
    // docs/04_ROBOT_TASK_MODEL.md §17, §24.
    StateManager manager;
    register_two(manager);

    ASSERT_TRUE(manager.assign_route(RobotId{"R01"}, core::RouteId{"ROUTE-1"}, at(2)).has_value());

    EXPECT_EQ(manager.snapshot(RobotId{"R01"})->current_route, core::RouteId{"ROUTE-1"});
}

TEST(StateAssociation, state_association_clears_a_route) {
    StateManager manager;
    register_two(manager);
    ASSERT_TRUE(manager.assign_route(RobotId{"R01"}, core::RouteId{"ROUTE-1"}, at(2)).has_value());

    ASSERT_TRUE(manager.assign_route(RobotId{"R01"}, std::nullopt, at(3)).has_value());

    EXPECT_FALSE(manager.snapshot(RobotId{"R01"})->current_route.has_value());
}

TEST(StateAssociation, state_association_attaches_a_task_to_a_robot) {
    StateManager manager;
    register_two(manager);

    ASSERT_TRUE(manager.assign_task(RobotId{"R01"}, core::TaskId{"TASK-7"}, at(2)).has_value());

    EXPECT_EQ(manager.snapshot(RobotId{"R01"})->current_task, core::TaskId{"TASK-7"});
}

TEST(StateAssociation, state_association_does_not_leak_between_robots) {
    StateManager manager;
    register_two(manager);

    ASSERT_TRUE(manager.assign_route(RobotId{"R01"}, core::RouteId{"ROUTE-1"}, at(2)).has_value());

    EXPECT_FALSE(manager.snapshot(RobotId{"R02"})->current_route.has_value());
}

// ==================================================================== version

TEST(StateVersion, state_version_starts_at_zero_and_advances_with_each_change) {
    StateManager manager;
    EXPECT_EQ(manager.version(), domain::StateVersion{0});

    ASSERT_TRUE(manager.register_robot(robot("R01"), at(0)).has_value());
    const domain::StateVersion after_register = manager.version();
    EXPECT_GT(after_register, domain::StateVersion{0});

    ASSERT_TRUE(manager.apply(telemetry("R01", 1, Position{})).has_value());
    EXPECT_GT(manager.version(), after_register);
}

TEST(StateVersion, state_version_is_fleet_wide_not_per_robot) {
    // docs/23_SYSTEM_ARCHITECTURE.md §2.4 asks "was this plan computed against
    // a world that has since moved". That is one question about the fleet, so
    // one counter answers it — a per-robot counter could not.
    StateManager manager;
    register_two(manager);

    ASSERT_TRUE(manager.apply(telemetry("R01", 1, Position{})).has_value());
    const domain::StateVersion after_first = manager.version();

    ASSERT_TRUE(manager.apply(telemetry("R02", 1, Position{})).has_value());

    EXPECT_GT(manager.version(), after_first);
}

TEST(StateVersion, state_version_stamps_the_snapshot_that_changed) {
    // Both questions answerable: how old is this plan, and which robots have
    // moved since it was made.
    StateManager manager;
    register_two(manager);

    ASSERT_TRUE(manager.apply(telemetry("R01", 1, Position{})).has_value());
    const domain::StateVersion fleet = manager.version();

    EXPECT_EQ(manager.snapshot(RobotId{"R01"})->version, fleet);
    EXPECT_LT(manager.snapshot(RobotId{"R02"})->version, fleet);
}

TEST(StateVersion, state_version_does_not_move_for_a_refused_update) {
    StateManager manager;
    register_two(manager);
    const domain::StateVersion before = manager.version();

    EXPECT_FALSE(manager.apply(telemetry("R99", 1, Position{})).has_value());

    EXPECT_EQ(manager.version(), before);
}

TEST(StateVersion, state_version_does_not_move_for_a_query) {
    StateManager manager;
    register_two(manager);
    const domain::StateVersion before = manager.version();

    (void)manager.snapshots();
    (void)manager.snapshot(RobotId{"R01"});
    (void)manager.robots_in_state(RobotState::idle);
    (void)manager.stale_robots(at(1'000), seconds(1));

    EXPECT_EQ(manager.version(), before);
}

// ====================================================================== query

TEST(StateQuery, state_query_lists_snapshots_in_robot_id_order) {
    // Ordered, not hashed. A fleet listing that came back differently on
    // every run would make the scenarios of docs/26_TEST_SCENARIOS.md
    // impossible to compare.
    StateManager manager;
    ASSERT_TRUE(manager.register_robot(robot("R03"), at(0)).has_value());
    ASSERT_TRUE(manager.register_robot(robot("R01"), at(0)).has_value());
    ASSERT_TRUE(manager.register_robot(robot("R02"), at(0)).has_value());

    const std::vector<domain::RobotStateSnapshot> snapshots = manager.snapshots();

    ASSERT_EQ(snapshots.size(), 3U);
    EXPECT_EQ(snapshots[0].robot_id, RobotId{"R01"});
    EXPECT_EQ(snapshots[1].robot_id, RobotId{"R02"});
    EXPECT_EQ(snapshots[2].robot_id, RobotId{"R03"});
}

TEST(StateQuery, state_query_finds_the_robots_in_a_state) {
    StateManager manager;
    register_two(manager);
    ASSERT_TRUE(manager.assign_state(RobotId{"R01"}, RobotState::reserving, at(1), std::nullopt)
                    .has_value());

    EXPECT_EQ(manager.robots_in_state(RobotState::reserving),
              (std::vector<RobotId>{RobotId{"R01"}}));
    EXPECT_EQ(manager.robots_in_state(RobotState::idle), (std::vector<RobotId>{RobotId{"R02"}}));
    EXPECT_TRUE(manager.robots_in_state(RobotState::failed).empty());
}

TEST(StateQuery, state_query_returns_nothing_for_an_unregistered_robot) {
    const StateManager manager;

    EXPECT_EQ(manager.snapshot(RobotId{"R01"}), nullptr);
    EXPECT_EQ(manager.record(RobotId{"R01"}), nullptr);
}

// ================================================================== staleness

TEST(StateStaleness, state_staleness_reports_a_robot_that_has_gone_quiet) {
    // docs/04_ROBOT_TASK_MODEL.md §21, docs/23_SYSTEM_ARCHITECTURE.md §5.
    StateManager manager;
    register_two(manager);
    ASSERT_TRUE(manager.apply(telemetry("R01", 100, Position{})).has_value());

    const std::vector<RobotId> stale = manager.stale_robots(at(110), seconds(30));

    EXPECT_EQ(stale, (std::vector<RobotId>{RobotId{"R02"}}));
}

TEST(StateStaleness, state_staleness_reports_nobody_when_everyone_is_fresh) {
    StateManager manager;
    register_two(manager);
    ASSERT_TRUE(manager.apply(telemetry("R01", 100, Position{})).has_value());
    ASSERT_TRUE(manager.apply(telemetry("R02", 100, Position{})).has_value());

    EXPECT_TRUE(manager.stale_robots(at(110), seconds(30)).empty());
}

TEST(StateStaleness, state_staleness_does_not_move_anyone_to_unknown) {
    // docs/04_ROBOT_TASK_MODEL.md §21 keeps COMMUNICATION_LOST as a system
    // event separate from the robot's own state. Deciding what silence means
    // is the controller's, and it may well be that the robot is fine.
    StateManager manager;
    register_two(manager);

    ASSERT_FALSE(manager.stale_robots(at(1'000), seconds(1)).empty());

    EXPECT_EQ(manager.snapshot(RobotId{"R01"})->state, RobotState::idle);
}

TEST(StateStaleness, state_staleness_lists_in_robot_id_order) {
    StateManager manager;
    ASSERT_TRUE(manager.register_robot(robot("R03"), at(0)).has_value());
    ASSERT_TRUE(manager.register_robot(robot("R01"), at(0)).has_value());

    EXPECT_EQ(manager.stale_robots(at(100), seconds(1)),
              (std::vector<RobotId>{RobotId{"R01"}, RobotId{"R03"}}));
}

// =================================================================== progress

TEST(StateProgress, state_progress_mark_advances_when_the_robot_moves) {
    StateManager manager;
    register_two(manager);

    ASSERT_TRUE(manager.apply(telemetry("R01", 10, Position{5.0, 0.0, 0.0})).has_value());

    EXPECT_EQ(manager.time_since_progress(RobotId{"R01"}, at(12)), seconds(2));
}

TEST(StateProgress, state_progress_mark_stays_put_for_a_robot_going_nowhere) {
    // A robot pressed against a pallet reports a position every hundred
    // milliseconds and has not moved. Advancing the mark on every observation
    // would make it look like it had just set off, forever.
    StateManager manager;
    register_two(manager);

    ASSERT_TRUE(manager.apply(telemetry("R01", 10, Position{5.0, 0.0, 0.0})).has_value());
    ASSERT_TRUE(manager.apply(telemetry("R01", 20, Position{5.01, 0.0, 0.0})).has_value());
    ASSERT_TRUE(manager.apply(telemetry("R01", 30, Position{5.02, 0.0, 0.0})).has_value());

    EXPECT_EQ(manager.time_since_progress(RobotId{"R01"}, at(30)), seconds(20));
}

TEST(StateProgress, state_progress_feeds_the_stall_ladder_without_applying_it) {
    // The manager reports how long a robot has been going nowhere; the policy
    // says what that means. Escalating a robot to `failed` releases its
    // resources, which is a traffic decision and not a bookkeeping one.
    StallPolicy policy;
    policy.stop_timeout = seconds(5);
    policy.blocked_timeout = seconds(30);

    StateManager manager{policy};
    ASSERT_TRUE(manager.register_robot(robot("R01"), at(0)).has_value());
    ASSERT_TRUE(manager.apply(telemetry("R01", 1, Position{5.0, 0.0, 0.0})).has_value());

    EXPECT_EQ(
        assess_stall(manager.stall_policy(), manager.time_since_progress(RobotId{"R01"}, at(3))),
        StallVerdict::temporarily_stopped);
    EXPECT_EQ(
        assess_stall(manager.stall_policy(), manager.time_since_progress(RobotId{"R01"}, at(20))),
        StallVerdict::blocked);

    // And the robot is still in whatever state it was actually reported in.
    EXPECT_EQ(manager.snapshot(RobotId{"R01"})->state, RobotState::idle);
}

TEST(StateProgress, state_progress_of_an_unregistered_robot_is_zero) {
    const StateManager manager;

    EXPECT_EQ(manager.time_since_progress(RobotId{"R01"}, at(100)), core::Duration::zero());
}

}  // namespace
}  // namespace traffic::state
