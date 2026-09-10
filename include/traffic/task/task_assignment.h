#pragma once

/// \file
/// The seam between the task register and whatever decides who does the work.
/// docs/04_ROBOT_TASK_MODEL.md §16, §24 ("Task assignment interface").
///
/// §16 is explicit:
///
///     Task와 Robot assignment는 별도 모듈에서 담당한다.
///     Task Manager -> Assignment Engine -> Robot
///
/// So this header defines the shape of that call and **no policy whatsoever**.
/// There is no nearest-robot rule, no load balancing, no battery threshold and
/// no charger detour here, and none of it is coming in Phase 4.
///
/// Three reasons it is deferred rather than written now:
///
///   * It is not this phase's work. docs/22_IMPLEMENTATION_WORKFLOW.md Phase 4
///     asks for the task lifecycle; CLAUDE.md forbids starting the next
///     phase's work before this one's completion criteria are met.
///   * A sensible policy needs inputs this module is not allowed to reach.
///     Picking the nearest free robot means knowing where the robots are
///     (`state`) and what a route between two nodes costs (`planning`), and
///     both sit at or above `task` in the dependency order of CLAUDE.md. A
///     policy written here would either be blind or would invert that order.
///   * The right policy is an empirical question. Assignment quality shows up
///     as task throughput and waiting time, which are KPI numbers, and
///     docs/00_MASTER_PLAN.md §6 rules that algorithm choices wait for a
///     benchmark rather than being guessed at up front.
///
/// What the seam does fix is the shape of the conversation: requests carry
/// only what the task itself knows, proposals name a task and a robot, and
/// everything crossing it is ordered so two runs of a scenario assign work in
/// the same sequence.

#include <optional>
#include <span>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/values.h"
#include "traffic/task/task_manager.h"

namespace traffic::task {

/// One task looking for a robot.
///
/// Everything an engine gets from us, and nothing more. In particular there is
/// no route and no cost: what a journey costs is the planner's answer
/// (docs/05_GLOBAL_ROUTING.md), and an engine that wants one asks the planner
/// rather than being handed a stale copy through here.
struct AssignmentRequest {
    core::TaskId task_id;

    /// Where the work starts and ends. docs/24_DOMAIN_MODEL.md §6.
    core::NodeId source;
    core::NodeId destination;

    /// The base priority of docs/04_ROBOT_TASK_MODEL.md §15, not an effective
    /// traffic priority.
    domain::Priority priority;

    /// Empty when the task has no deadline.
    std::optional<core::TimePoint> deadline;

    /// When the request was drawn up, so an engine can measure how long a task
    /// has been waiting for a robot without reading a clock of its own.
    core::TimePoint requested_at{core::kTimeOrigin};

    [[nodiscard]] friend bool operator==(const AssignmentRequest&,
                                         const AssignmentRequest&) = default;
};

/// One engine's answer: this task, that robot.
///
/// A proposal, not a command. It becomes real only when the register accepts
/// it, and the register can refuse — the task may have been cancelled since
/// the request was drawn up.
struct AssignmentProposal {
    core::TaskId task_id;
    core::RobotId robot_id;

    [[nodiscard]] friend bool operator==(const AssignmentProposal&,
                                         const AssignmentProposal&) = default;
};

/// Decides which robot should do which task.
///
/// Deliberately not implemented in this module — see the file comment. An
/// implementation lives wherever it can legally see robot state and route
/// cost, and arrives with the benchmark that justifies its rule.
class ITaskAssignment {
public:
    ITaskAssignment() = default;
    virtual ~ITaskAssignment() = default;

    ITaskAssignment(const ITaskAssignment&) = delete;
    ITaskAssignment& operator=(const ITaskAssignment&) = delete;
    ITaskAssignment(ITaskAssignment&&) = delete;
    ITaskAssignment& operator=(ITaskAssignment&&) = delete;

    /// Proposes a robot for as many of \p requests as it can.
    ///
    /// A task an engine has no robot for is simply left out; there is no
    /// rejection vocabulary, because "no candidate" is the normal state of a
    /// busy fleet and not a failure anyone should have to handle.
    ///
    /// \p available_robots is supplied by the caller rather than looked up,
    /// which is what keeps this interface free of a dependency on `state`.
    ///
    /// Implementations must return proposals in a deterministic order for a
    /// given input — CLAUDE.md, docs/01_REQUIREMENTS.md NFR-003 — because the
    /// order they are applied in decides who gets the last free robot.
    [[nodiscard]] virtual std::vector<AssignmentProposal> propose(
        std::span<const AssignmentRequest> requests,
        std::span<const core::RobotId> available_robots) = 0;
};

/// Draws up a request for every task still waiting for a robot.
///
/// Highest priority first, ties broken by task id — the order of
/// `by_priority`. An engine is free to ignore the order, but it is offered one
/// so that the common "take them in turn until the robots run out" engine is
/// deterministic without having to sort anything itself.
[[nodiscard]] std::vector<AssignmentRequest> assignment_requests(const ITaskManager& tasks,
                                                                 core::TimePoint at);

/// Applies \p proposals to \p tasks, returning the ones it refused.
///
/// Refusals are returned rather than dropped or thrown. An engine that
/// proposes a robot for a task which has since been cancelled has not
/// malfunctioned — it answered a question that has gone stale — and it needs
/// to be told so it can offer that robot to something else.
[[nodiscard]] std::vector<AssignmentProposal> apply_assignments(
    ITaskManager& tasks, std::span<const AssignmentProposal> proposals, core::TimePoint at);

}  // namespace traffic::task
