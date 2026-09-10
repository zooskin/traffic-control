#pragma once

/// \file
/// The fleet's live picture of itself.
/// docs/23_SYSTEM_ARCHITECTURE.md §22, docs/22_IMPLEMENTATION_WORKFLOW.md
/// Phase 3, docs/04_ROBOT_TASK_MODEL.md §20~22.
///
/// docs/23_SYSTEM_ARCHITECTURE.md §22 gives RobotState to StateManager and says
/// no other component may change it. That single sentence is what this class
/// is: one owner, one version counter, one place a robot's state can move.
///
/// **Versions are fleet-wide.** docs/23_SYSTEM_ARCHITECTURE.md §2.4 talks about
/// "a plan computed against state version 100" — one number for the whole
/// fleet, not one per robot. So the manager holds the counter, bumps it on
/// every accepted change, and stamps each snapshot with the fleet version at
/// which it was last touched. That makes both questions answerable: how old is
/// this plan, and which robots have moved since it was made.
///
/// **Two doors in, and they are not the same door.** `apply` takes what a
/// robot said about itself; `assign_state` takes what traffic control decided
/// about it. Only the second may set `reserving`, `waiting` or `replanning` —
/// see decision D-009. Keeping them apart is what stops a replayed telemetry
/// message from asserting that a reservation exists.
///
/// **It reports, it does not judge.** The manager knows how long a robot has
/// been going nowhere; it does not decide that this makes the robot blocked.
/// That ladder is `StallPolicy` (stall_policy.h) applied by the traffic
/// controller, because escalating a robot to `failed` releases its resources
/// and is a traffic decision, not a bookkeeping one.

#include <cstddef>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/robot.h"
#include "traffic/domain/robot_state.h"
#include "traffic/domain/values.h"
#include "traffic/state/robot_state_update.h"
#include "traffic/state/stall_policy.h"
#include "traffic/state/waiting.h"

namespace traffic::state {

/// Why a robot could not be added to the register.
enum class RegistrationRejection {
    empty_robot_id,

    /// A robot with that id is already registered. Re-registering would reset
    /// its state and lose whatever it was doing, so it is refused rather than
    /// treated as an update.
    already_registered,
};

[[nodiscard]] std::string_view to_string(RegistrationRejection rejection) noexcept;

/// Everything the manager holds about one robot.
///
/// Kept together so that "what we know about R01" is one lookup, and split
/// into observation and conclusion so the two never blur — D-009.
struct RobotRecord {
    /// Identity and fixed capabilities. Does not change after registration.
    domain::Robot robot;

    /// What was last observed, and at which fleet version.
    domain::RobotStateSnapshot snapshot;

    /// Why traffic control is holding it, when it is. Set only through
    /// `assign_state`, and cleared the moment the robot leaves `waiting`.
    std::optional<WaitingContext> waiting;

    /// The last place it was seen to be making progress.
    ProgressMark progress;

    /// The last command the robot acknowledged.
    /// docs/04_ROBOT_TASK_MODEL.md §22.
    std::optional<core::CommandId> last_ack_command;

    /// When *we* last touched this record, as opposed to when the robot took
    /// its measurement. §22's `controller_timestamp`.
    ///
    /// The two differ, and the gap is the thing worth seeing: a robot whose
    /// own timestamp is fresh but whose record is old is one we have stopped
    /// listening to.
    core::TimePoint controller_timestamp{core::kTimeOrigin};
};

/// Owns the state of every robot in the fleet.
class IStateManager {
public:
    IStateManager() = default;
    virtual ~IStateManager() = default;

    IStateManager(const IStateManager&) = delete;
    IStateManager& operator=(const IStateManager&) = delete;
    IStateManager(IStateManager&&) = delete;
    IStateManager& operator=(IStateManager&&) = delete;

    // ------------------------------------------------------------- register

    /// Adds a robot, in `idle`, observed at \p at.
    [[nodiscard]] virtual core::Status<RegistrationRejection> register_robot(
        domain::Robot robot, core::TimePoint at) = 0;

    /// Removes a robot from the register.
    ///
    /// Its state goes with it. Whatever it was holding has to be released
    /// first; this class does not know about reservations and cannot do it.
    [[nodiscard]] virtual bool deregister_robot(const core::RobotId& robot_id) = 0;

    [[nodiscard]] virtual bool is_registered(const core::RobotId& robot_id) const = 0;
    [[nodiscard]] virtual std::size_t robot_count() const noexcept = 0;

    // --------------------------------------------------------------- update

    /// Applies telemetry from a robot. docs/04_ROBOT_TASK_MODEL.md §20.
    [[nodiscard]] virtual core::Status<UpdateRejection> apply(const RobotStateUpdate& update) = 0;

    /// Sets a state traffic control decided on.
    ///
    /// \p waiting must be present for `waiting` and absent for everything
    /// else: a hold with no reason recorded cannot be explained afterwards,
    /// and a reason attached to a robot that is not waiting is a leftover.
    [[nodiscard]] virtual core::Status<UpdateRejection> assign_state(
        const core::RobotId& robot_id,
        domain::RobotState state,
        core::TimePoint at,
        std::optional<WaitingContext> waiting) = 0;

    /// Associates a route with a robot, or clears it with an empty value.
    /// docs/04_ROBOT_TASK_MODEL.md §17, §24.
    [[nodiscard]] virtual core::Status<UpdateRejection> assign_route(
        const core::RobotId& robot_id, std::optional<core::RouteId> route, core::TimePoint at) = 0;

    /// Associates a task, or clears it.
    [[nodiscard]] virtual core::Status<UpdateRejection> assign_task(
        const core::RobotId& robot_id, std::optional<core::TaskId> task, core::TimePoint at) = 0;

    // ---------------------------------------------------------------- query

    /// The record for one robot, or nullptr.
    [[nodiscard]] virtual const RobotRecord* record(const core::RobotId& robot_id) const = 0;

    /// The last observation of one robot, or nullptr.
    [[nodiscard]] virtual const domain::RobotStateSnapshot* snapshot(
        const core::RobotId& robot_id) const = 0;

    /// Every robot's last observation, in robot-id order.
    ///
    /// Ordered, not hashed. A fleet listing that came back differently on
    /// every run would make the scenarios of docs/26_TEST_SCENARIOS.md
    /// impossible to compare.
    [[nodiscard]] virtual std::vector<domain::RobotStateSnapshot> snapshots() const = 0;

    /// The robots in \p state, in id order.
    [[nodiscard]] virtual std::vector<core::RobotId> robots_in_state(
        domain::RobotState state) const = 0;

    /// The fleet-wide state version.
    [[nodiscard]] virtual domain::StateVersion version() const noexcept = 0;

    // -------------------------------------------------------------- staleness

    /// Robots whose last observation is older than \p max_age, in id order.
    ///
    /// docs/04_ROBOT_TASK_MODEL.md §21 keeps COMMUNICATION_LOST as a system
    /// event separate from the robot's own state, so this reports and does not
    /// move anyone to `unknown`. Deciding what silence means is the
    /// controller's.
    [[nodiscard]] virtual std::vector<core::RobotId> stale_robots(core::TimePoint now,
                                                                  core::Duration max_age) const = 0;

    /// How long since \p robot_id was last seen to move. Zero for a robot that
    /// is not registered.
    [[nodiscard]] virtual core::Duration time_since_progress(const core::RobotId& robot_id,
                                                             core::TimePoint now) const = 0;
};

/// The one implementation. docs/23_SYSTEM_ARCHITECTURE.md §22.
class StateManager final : public IStateManager {
public:
    /// \p policy supplies `minimum_progress`, which is what decides whether an
    /// observation advances a robot's progress mark. The rest of the policy —
    /// the T1/T2 ladder — is not applied here; it is exposed through
    /// `stall_policy()` for the caller that does apply it.
    explicit StateManager(StallPolicy policy = StallPolicy{});

    [[nodiscard]] const StallPolicy& stall_policy() const noexcept { return policy_; }

    [[nodiscard]] core::Status<RegistrationRejection> register_robot(domain::Robot robot,
                                                                     core::TimePoint at) override;
    [[nodiscard]] bool deregister_robot(const core::RobotId& robot_id) override;
    [[nodiscard]] bool is_registered(const core::RobotId& robot_id) const override;
    [[nodiscard]] std::size_t robot_count() const noexcept override;

    [[nodiscard]] core::Status<UpdateRejection> apply(const RobotStateUpdate& update) override;
    [[nodiscard]] core::Status<UpdateRejection> assign_state(
        const core::RobotId& robot_id,
        domain::RobotState state,
        core::TimePoint at,
        std::optional<WaitingContext> waiting) override;
    [[nodiscard]] core::Status<UpdateRejection> assign_route(const core::RobotId& robot_id,
                                                             std::optional<core::RouteId> route,
                                                             core::TimePoint at) override;
    [[nodiscard]] core::Status<UpdateRejection> assign_task(const core::RobotId& robot_id,
                                                            std::optional<core::TaskId> task,
                                                            core::TimePoint at) override;

    [[nodiscard]] const RobotRecord* record(const core::RobotId& robot_id) const override;
    [[nodiscard]] const domain::RobotStateSnapshot* snapshot(
        const core::RobotId& robot_id) const override;
    [[nodiscard]] std::vector<domain::RobotStateSnapshot> snapshots() const override;
    [[nodiscard]] std::vector<core::RobotId> robots_in_state(
        domain::RobotState state) const override;
    [[nodiscard]] domain::StateVersion version() const noexcept override;

    [[nodiscard]] std::vector<core::RobotId> stale_robots(core::TimePoint now,
                                                          core::Duration max_age) const override;
    [[nodiscard]] core::Duration time_since_progress(const core::RobotId& robot_id,
                                                     core::TimePoint now) const override;

private:
    /// Finds a record for writing, or nullptr.
    [[nodiscard]] RobotRecord* find(const core::RobotId& robot_id);
    [[nodiscard]] const RobotRecord* find(const core::RobotId& robot_id) const;

    /// Bumps the fleet version and stamps \p record with it.
    void touch(RobotRecord& record, core::TimePoint at);

    StallPolicy policy_;

    /// Ordered on purpose — see `snapshots()`.
    std::map<core::RobotId, RobotRecord> robots_;

    domain::StateVersion version_;
};

}  // namespace traffic::state
