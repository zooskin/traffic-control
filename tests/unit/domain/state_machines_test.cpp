/// Tests for the three domain state machines.
///
/// Naming follows docs/20_CODING_GUIDELINES.md §45.
///
/// Each machine gets the same three structural checks — every state reachable,
/// self-transitions allowed, names round-trip — plus the specific invariants
/// docs/24_DOMAIN_MODEL.md §30 names.

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "traffic/domain/reservation_state.h"
#include "traffic/domain/robot_state.h"
#include "traffic/domain/task_status.h"

namespace traffic::domain {
namespace {

// =========================================================== RobotState

TEST(RobotStateMachine, robot_state_exposes_every_enumerator) {
    EXPECT_EQ(robot_states().size(), kRobotStateCount);
}

TEST(RobotStateMachine, robot_state_allows_self_transition) {
    // State updates arrive continuously and mostly report no change.
    for (const RobotState state : robot_states()) {
        EXPECT_TRUE(is_transition_allowed(state, state))
            << to_string(state) << " -> " << to_string(state);
    }
}

TEST(RobotStateMachine, robot_state_follows_the_nominal_task_cycle) {
    // idle -> reserving -> moving -> idle, the path a task takes when nothing
    // interferes (docs/24_DOMAIN_MODEL.md §5).
    EXPECT_TRUE(is_transition_allowed(RobotState::idle, RobotState::reserving));
    EXPECT_TRUE(is_transition_allowed(RobotState::reserving, RobotState::moving));
    EXPECT_TRUE(is_transition_allowed(RobotState::moving, RobotState::idle));
}

TEST(RobotStateMachine, robot_state_moving_can_be_interrupted) {
    EXPECT_TRUE(is_transition_allowed(RobotState::moving, RobotState::waiting));
    EXPECT_TRUE(is_transition_allowed(RobotState::moving, RobotState::temporarily_stopped));
    EXPECT_TRUE(is_transition_allowed(RobotState::moving, RobotState::blocked));
    EXPECT_TRUE(is_transition_allowed(RobotState::moving, RobotState::failed));
}

/// A person stepping into a corridor stops a robot; a stop that outlasts its
/// timeout becomes a blockage worth replanning for
/// (docs/25_TRAFFIC_CONTROL_SPECIFICATION.md §17~18).
TEST(RobotStateMachine, robot_state_temporary_stop_resolves_either_way) {
    EXPECT_TRUE(is_transition_allowed(RobotState::temporarily_stopped, RobotState::moving));
    EXPECT_TRUE(is_transition_allowed(RobotState::temporarily_stopped, RobotState::blocked));
}

TEST(RobotStateMachine, robot_state_idle_cannot_be_interrupted) {
    // All four describe interrupted progress, and an idle robot has none.
    EXPECT_FALSE(is_transition_allowed(RobotState::idle, RobotState::waiting));
    EXPECT_FALSE(is_transition_allowed(RobotState::idle, RobotState::temporarily_stopped));
    EXPECT_FALSE(is_transition_allowed(RobotState::idle, RobotState::blocked));
    EXPECT_FALSE(is_transition_allowed(RobotState::idle, RobotState::replanning));
}

/// Recovery has to release the failed robot's resources first. Allowing
/// failed -> moving would leave the reservation table holding entries that no
/// robot believes it owns.
TEST(RobotStateMachine, robot_state_failed_recovers_only_through_idle) {
    EXPECT_TRUE(is_transition_allowed(RobotState::failed, RobotState::idle));
    EXPECT_TRUE(is_transition_allowed(RobotState::failed, RobotState::unknown));

    EXPECT_FALSE(is_transition_allowed(RobotState::failed, RobotState::moving));
    EXPECT_FALSE(is_transition_allowed(RobotState::failed, RobotState::reserving));
    EXPECT_FALSE(is_transition_allowed(RobotState::failed, RobotState::waiting));
    EXPECT_FALSE(is_transition_allowed(RobotState::failed, RobotState::replanning));
}

TEST(RobotStateMachine, robot_state_any_state_can_fail_or_go_silent) {
    for (const RobotState state : robot_states()) {
        EXPECT_TRUE(is_transition_allowed(state, RobotState::failed)) << to_string(state);
        EXPECT_TRUE(is_transition_allowed(state, RobotState::unknown)) << to_string(state);
    }
}

/// The first observation after a communication gap has to be accepted whatever
/// it says; the controller has no basis to argue with it.
TEST(RobotStateMachine, robot_state_unknown_accepts_any_observation) {
    for (const RobotState state : robot_states()) {
        EXPECT_TRUE(is_transition_allowed(RobotState::unknown, state)) << to_string(state);
    }
}

TEST(RobotStateMachine, robot_state_halted_covers_the_stalled_states) {
    EXPECT_TRUE(is_halted(RobotState::waiting));
    EXPECT_TRUE(is_halted(RobotState::temporarily_stopped));
    EXPECT_TRUE(is_halted(RobotState::blocked));
    EXPECT_TRUE(is_halted(RobotState::replanning));

    EXPECT_FALSE(is_halted(RobotState::moving));
    EXPECT_FALSE(is_halted(RobotState::idle));
    EXPECT_FALSE(is_halted(RobotState::reserving));
}

TEST(RobotStateMachine, robot_state_uncontrollable_when_failed_or_unknown) {
    EXPECT_FALSE(is_controllable(RobotState::failed));
    EXPECT_FALSE(is_controllable(RobotState::unknown));

    for (const RobotState state : robot_states()) {
        if (state != RobotState::failed && state != RobotState::unknown) {
            EXPECT_TRUE(is_controllable(state)) << to_string(state);
        }
    }
}

TEST(RobotStateMachine, robot_state_names_round_trip) {
    for (const RobotState state : robot_states()) {
        const auto parsed = robot_state_from_string(to_string(state));
        ASSERT_TRUE(parsed.has_value()) << to_string(state);
        EXPECT_EQ(*parsed, state);
    }
}

TEST(RobotStateMachine, robot_state_names_are_unique) {
    std::vector<std::string> names;
    names.reserve(kRobotStateCount);
    for (const RobotState state : robot_states()) {
        names.emplace_back(to_string(state));
    }
    std::sort(names.begin(), names.end());
    EXPECT_EQ(std::adjacent_find(names.begin(), names.end()), names.end())
        << "two states share a wire name";
}

TEST(RobotStateMachine, robot_state_rejects_an_unknown_name) {
    EXPECT_FALSE(robot_state_from_string("NOT_A_STATE").has_value());
    EXPECT_FALSE(robot_state_from_string("").has_value());
    EXPECT_FALSE(robot_state_from_string("idle").has_value()) << "names are upper case";
}

// =========================================================== TaskStatus

TEST(TaskStatusMachine, task_status_exposes_every_enumerator) {
    EXPECT_EQ(task_statuses().size(), kTaskStatusCount);
}

TEST(TaskStatusMachine, task_status_allows_self_transition) {
    for (const TaskStatus status : task_statuses()) {
        EXPECT_TRUE(is_transition_allowed(status, status)) << to_string(status);
    }
}

TEST(TaskStatusMachine, task_status_follows_the_nominal_lifecycle) {
    EXPECT_TRUE(is_transition_allowed(TaskStatus::created, TaskStatus::assigned));
    EXPECT_TRUE(is_transition_allowed(TaskStatus::assigned, TaskStatus::planning));
    EXPECT_TRUE(is_transition_allowed(TaskStatus::planning, TaskStatus::running));
    EXPECT_TRUE(is_transition_allowed(TaskStatus::running, TaskStatus::completed));
}

TEST(TaskStatusMachine, task_status_running_and_waiting_alternate) {
    // Traffic control holds a robot and releases it repeatedly over one task.
    EXPECT_TRUE(is_transition_allowed(TaskStatus::running, TaskStatus::waiting));
    EXPECT_TRUE(is_transition_allowed(TaskStatus::waiting, TaskStatus::running));
}

TEST(TaskStatusMachine, task_status_replanning_returns_to_planning) {
    EXPECT_TRUE(is_transition_allowed(TaskStatus::running, TaskStatus::planning));
    EXPECT_TRUE(is_transition_allowed(TaskStatus::waiting, TaskStatus::planning));
}

/// docs/24_DOMAIN_MODEL.md §30, stated directly.
TEST(TaskStatusMachine, task_status_completed_never_runs_again) {
    EXPECT_FALSE(is_transition_allowed(TaskStatus::completed, TaskStatus::running));
    EXPECT_FALSE(is_transition_allowed(TaskStatus::completed, TaskStatus::planning));
    EXPECT_FALSE(is_transition_allowed(TaskStatus::completed, TaskStatus::assigned));
    EXPECT_FALSE(is_transition_allowed(TaskStatus::completed, TaskStatus::waiting));
}

TEST(TaskStatusMachine, task_status_terminal_states_are_absorbing) {
    for (const TaskStatus from : task_statuses()) {
        if (!is_terminal(from)) {
            continue;
        }
        for (const TaskStatus to : task_statuses()) {
            const bool allowed = is_transition_allowed(from, to);
            EXPECT_EQ(allowed, from == to) << to_string(from) << " -> " << to_string(to);
        }
    }
}

TEST(TaskStatusMachine, task_status_cannot_complete_before_running) {
    EXPECT_FALSE(is_transition_allowed(TaskStatus::created, TaskStatus::completed));
    EXPECT_FALSE(is_transition_allowed(TaskStatus::assigned, TaskStatus::completed));
    EXPECT_FALSE(is_transition_allowed(TaskStatus::planning, TaskStatus::completed));
    EXPECT_FALSE(is_transition_allowed(TaskStatus::waiting, TaskStatus::completed));
}

TEST(TaskStatusMachine, task_status_live_tasks_can_be_cancelled_or_fail) {
    for (const TaskStatus status : task_statuses()) {
        if (is_terminal(status)) {
            continue;
        }
        EXPECT_TRUE(is_transition_allowed(status, TaskStatus::cancelled)) << to_string(status);
        EXPECT_TRUE(is_transition_allowed(status, TaskStatus::failed)) << to_string(status);
    }
}

TEST(TaskStatusMachine, task_status_active_means_a_robot_is_committed) {
    EXPECT_TRUE(is_active(TaskStatus::assigned));
    EXPECT_TRUE(is_active(TaskStatus::planning));
    EXPECT_TRUE(is_active(TaskStatus::running));
    EXPECT_TRUE(is_active(TaskStatus::waiting));

    EXPECT_FALSE(is_active(TaskStatus::created)) << "not yet assigned to anyone";
    EXPECT_FALSE(is_active(TaskStatus::completed));
    EXPECT_FALSE(is_active(TaskStatus::cancelled));
    EXPECT_FALSE(is_active(TaskStatus::failed));
}

TEST(TaskStatusMachine, task_status_names_round_trip) {
    for (const TaskStatus status : task_statuses()) {
        const auto parsed = task_status_from_string(to_string(status));
        ASSERT_TRUE(parsed.has_value()) << to_string(status);
        EXPECT_EQ(*parsed, status);
    }
}

// ====================================================== ReservationState

TEST(ReservationStateMachine, reservation_state_exposes_every_enumerator) {
    EXPECT_EQ(reservation_states().size(), kReservationStateCount);
}

TEST(ReservationStateMachine, reservation_state_follows_the_nominal_lifecycle) {
    EXPECT_TRUE(is_transition_allowed(ReservationState::pending, ReservationState::active));
    EXPECT_TRUE(is_transition_allowed(ReservationState::active, ReservationState::released));
}

/// docs/24_DOMAIN_MODEL.md §30. Another robot may already be inside the
/// resource; reviving the old grant hands it to two owners at once.
TEST(ReservationStateMachine, reservation_state_released_never_becomes_active) {
    EXPECT_FALSE(is_transition_allowed(ReservationState::released, ReservationState::active));
    EXPECT_FALSE(is_transition_allowed(ReservationState::released, ReservationState::pending));
}

TEST(ReservationStateMachine, reservation_state_expired_never_becomes_active) {
    EXPECT_FALSE(is_transition_allowed(ReservationState::expired, ReservationState::active));
    EXPECT_FALSE(is_transition_allowed(ReservationState::cancelled, ReservationState::active));
}

/// The robot is already inside the resource. Cancelling would erase the only
/// record that it is occupied.
TEST(ReservationStateMachine, reservation_state_active_cannot_be_cancelled) {
    EXPECT_FALSE(is_transition_allowed(ReservationState::active, ReservationState::cancelled));
    EXPECT_TRUE(is_transition_allowed(ReservationState::active, ReservationState::expired));
}

TEST(ReservationStateMachine, reservation_state_pending_can_be_withdrawn) {
    EXPECT_TRUE(is_transition_allowed(ReservationState::pending, ReservationState::cancelled));
    EXPECT_TRUE(is_transition_allowed(ReservationState::pending, ReservationState::expired));
}

TEST(ReservationStateMachine, reservation_state_pending_cannot_be_released) {
    // Nothing was ever held, so there is nothing to give back.
    EXPECT_FALSE(is_transition_allowed(ReservationState::pending, ReservationState::released));
}

TEST(ReservationStateMachine, reservation_state_terminal_states_are_absorbing) {
    for (const ReservationState from : reservation_states()) {
        if (!is_terminal(from)) {
            continue;
        }
        for (const ReservationState to : reservation_states()) {
            EXPECT_EQ(is_transition_allowed(from, to), from == to)
                << to_string(from) << " -> " << to_string(to);
        }
    }
}

/// A pending request must block conflicting grants. If it did not, two robots
/// could both be told to proceed into the same corridor.
TEST(ReservationStateMachine, reservation_state_pending_and_active_withhold_the_resource) {
    EXPECT_TRUE(holds_resource(ReservationState::pending));
    EXPECT_TRUE(holds_resource(ReservationState::active));

    EXPECT_FALSE(holds_resource(ReservationState::released));
    EXPECT_FALSE(holds_resource(ReservationState::expired));
    EXPECT_FALSE(holds_resource(ReservationState::cancelled));
}

TEST(ReservationStateMachine, reservation_state_names_round_trip) {
    for (const ReservationState state : reservation_states()) {
        const auto parsed = reservation_state_from_string(to_string(state));
        ASSERT_TRUE(parsed.has_value()) << to_string(state);
        EXPECT_EQ(*parsed, state);
    }
}

}  // namespace
}  // namespace traffic::domain
