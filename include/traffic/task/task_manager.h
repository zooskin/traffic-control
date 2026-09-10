#pragma once

/// \file
/// The owner of every task.
/// docs/23_SYSTEM_ARCHITECTURE.md §22, docs/22_IMPLEMENTATION_WORKFLOW.md
/// Phase 4, docs/04_ROBOT_TASK_MODEL.md §12~16.
///
/// docs/23_SYSTEM_ARCHITECTURE.md §22 gives Task to TaskManager and says no
/// other component may change it. That sentence is what this class is: one
/// owner, one version counter, one place a task's status can move.
///
/// **The state machine is the only authority on what may happen.** Every
/// lifecycle call goes through `domain::with_status` / `domain::assign_to`,
/// which consult the transition table of docs/24_DOMAIN_MODEL.md §6 and stamp
/// `started_at` / `completed_at` themselves. This class adds no second opinion
/// about which moves are legal — two tables would disagree eventually, and the
/// one in the domain is the one the domain tests cover.
///
/// **Time is a parameter, never a reading.** No clock is held here, exactly as
/// `state::StateManager` holds none. CLAUDE.md forbids reading the clock from
/// domain and logic code; passing the instant in also means a scenario replayed
/// from a log produces identical records, which docs/16_TEST_STRATEGY.md §14
/// depends on. A caller that has a `core::IClock` passes `clock.now()`.
///
/// **It owns tasks; it does not hand them out.** Choosing which robot should
/// do which task is a separate module — docs/04_ROBOT_TASK_MODEL.md §16 — and
/// task_assignment.h is the seam. Nothing here ranks robots or looks at where
/// they are.
///
/// **It reports, it does not judge.** `overdue_tasks` says which deadlines have
/// passed; it does not cancel those tasks or raise their priority. What an
/// overdue task deserves is a traffic-priority question
/// (docs/09_PRIORITY_MANAGER.md, Phase 8), and answering it here would put
/// policy in the register.

#include <cstddef>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/result.h"
#include "traffic/core/time.h"
#include "traffic/domain/task.h"
#include "traffic/domain/task_status.h"
#include "traffic/domain/values.h"
#include "traffic/task/task_change.h"
#include "traffic/task/task_version.h"

namespace traffic::task {

/// The priority band of docs/04_ROBOT_TASK_MODEL.md §15: 0 to 100, higher
/// wins.
///
/// Checked by the register rather than by `domain::Priority`, because that
/// type also carries the *effective* priority of Phase 8 — this number plus
/// waiting time and urgency — which deliberately leaves the band.
inline constexpr domain::Priority kMinTaskPriority{0};
inline constexpr domain::Priority kMaxTaskPriority{100};

/// Why a task operation was refused.
enum class TaskRejection {
    /// The task carries no id, or one of its endpoints has none.
    /// docs/24_DOMAIN_MODEL.md §26.
    empty_task_id,

    /// A task with that id is already registered.
    ///
    /// Refused rather than treated as an update: the second task would
    /// silently replace the first, and whichever robot was working on the
    /// first would then be running work nobody could look up.
    duplicate_task,

    /// No task with that id. Not a lifecycle problem — a lookup that is wrong
    /// about what exists.
    unknown_task,

    /// A task was offered already assigned, already running, or already
    /// finished.
    ///
    /// A task enters at `created` (docs/04_ROBOT_TASK_MODEL.md §14) and gets
    /// everywhere else through this register. One that arrives half-way
    /// through its life has a history the register never saw and cannot
    /// reconstruct.
    not_a_new_task,

    /// source == destination. docs/24_DOMAIN_MODEL.md §34.
    same_source_and_destination,

    /// The state machine of docs/24_DOMAIN_MODEL.md §6 refuses the move.
    invalid_transition,

    /// A task was assigned to a robot with no id.
    empty_robot_id,

    /// A change was asked of a task that has already finished.
    ///
    /// docs/24_DOMAIN_MODEL.md §30: terminal is terminal. Re-priming a
    /// finished task would double-count throughput.
    task_is_terminal,

    /// `remove` was called on a task that is still live.
    ///
    /// Dropping it would strand whichever robot is carrying it out, with no
    /// record left of what that robot was doing.
    task_still_live,

    /// Priority outside 0-100. docs/04_ROBOT_TASK_MODEL.md §15.
    priority_out_of_range,

    /// A deadline earlier than the task's own creation.
    ///
    /// Such a task is overdue from the instant it exists, so it would sit at
    /// the front of every urgency queue forever without ever being
    /// satisfiable.
    deadline_before_creation,
};

[[nodiscard]] std::string_view to_string(TaskRejection rejection) noexcept;

/// What an accepted lifecycle call produced.
///
/// The change when there was one, and nothing when the call was accepted
/// without moving anything — a repeated report of the status a task is already
/// in. Returning it rather than publishing it is the rule task_change.h
/// explains.
using TaskOutcome = core::Result<std::optional<TaskChange>, TaskRejection>;

/// Everything the register holds about one task.
struct TaskRecord {
    /// The task itself, as docs/24_DOMAIN_MODEL.md §6 defines it.
    domain::Task task;

    /// The task-set version at which this task last changed. Kept beside the
    /// task rather than inside it — see task_version.h.
    TaskVersion version;

    /// When *we* last touched this record, as opposed to when the work was
    /// created or started. Mirrors `state::RobotRecord::controller_timestamp`.
    core::TimePoint controller_timestamp{core::kTimeOrigin};
};

/// Owns every task in the system.
class ITaskManager {
public:
    ITaskManager() = default;
    virtual ~ITaskManager() = default;

    ITaskManager(const ITaskManager&) = delete;
    ITaskManager& operator=(const ITaskManager&) = delete;
    ITaskManager(ITaskManager&&) = delete;
    ITaskManager& operator=(ITaskManager&&) = delete;

    // --------------------------------------------------------------- register

    /// Takes ownership of a new task, accepted at \p at.
    ///
    /// Takes a built `domain::Task` rather than the arguments of
    /// `domain::make_task`, so the caller can supply the deadline of
    /// docs/04_ROBOT_TASK_MODEL.md §12 that the factory does not take. The
    /// invariants are therefore re-checked here: `Task` is an aggregate, so a
    /// caller can build one without going through the factory at all.
    [[nodiscard]] virtual core::Status<TaskRejection> create(domain::Task task,
                                                             core::TimePoint at) = 0;

    /// Forgets a finished task.
    ///
    /// History has to be trimmed somewhere — a fleet running for weeks would
    /// otherwise carry every task it ever completed — but only after the work
    /// is over.
    [[nodiscard]] virtual core::Status<TaskRejection> remove(const core::TaskId& task_id) = 0;

    [[nodiscard]] virtual bool contains(const core::TaskId& task_id) const = 0;
    [[nodiscard]] virtual std::size_t task_count() const noexcept = 0;

    // -------------------------------------------------------------- lifecycle

    /// Gives the task to a robot and moves it to `assigned`.
    ///
    /// Also the reassignment path: the transition table lets `planning` go
    /// back to `assigned`, so a task can change hands until it starts running
    /// and not afterwards. Which robot should get it is not decided here —
    /// docs/04_ROBOT_TASK_MODEL.md §16, task_assignment.h.
    [[nodiscard]] virtual TaskOutcome assign(const core::TaskId& task_id,
                                             core::RobotId robot_id,
                                             core::TimePoint at) = 0;

    /// Moves the task to \p next, or refuses.
    ///
    /// The single lifecycle door. `start_planning`, `start`, `hold`,
    /// `complete`, `cancel` and `fail` below are names for calls to it.
    [[nodiscard]] virtual TaskOutcome advance(const core::TaskId& task_id,
                                              domain::TaskStatus next,
                                              core::TimePoint at) = 0;

    // ------------------------------------------------------------- attributes

    /// Re-bands a live task. docs/04_ROBOT_TASK_MODEL.md §15.
    [[nodiscard]] virtual core::Status<TaskRejection> set_priority(const core::TaskId& task_id,
                                                                   domain::Priority priority,
                                                                   core::TimePoint at) = 0;

    /// Sets or clears the deadline of docs/04_ROBOT_TASK_MODEL.md §12.
    [[nodiscard]] virtual core::Status<TaskRejection> set_deadline(
        const core::TaskId& task_id,
        std::optional<core::TimePoint> deadline,
        core::TimePoint at) = 0;

    // ------------------------------------------------------------------ query

    /// The record for one task, or nullptr.
    [[nodiscard]] virtual const TaskRecord* record(const core::TaskId& task_id) const = 0;

    /// One task, or nullptr.
    [[nodiscard]] virtual const domain::Task* task(const core::TaskId& task_id) const = 0;

    /// Every task, in task-id order.
    ///
    /// Ordered, not hashed. A listing that came back differently on every run
    /// would make the scenarios of docs/26_TEST_SCENARIOS.md impossible to
    /// compare, and CLAUDE.md forbids an unordered container anywhere the
    /// iteration order can reach a result.
    [[nodiscard]] virtual std::vector<domain::Task> tasks() const = 0;

    /// The tasks in \p status, in id order.
    [[nodiscard]] virtual std::vector<core::TaskId> tasks_with_status(
        domain::TaskStatus status) const = 0;

    /// Every task ever given to \p robot_id, finished ones included, in id
    /// order.
    [[nodiscard]] virtual std::vector<core::TaskId> tasks_for_robot(
        const core::RobotId& robot_id) const = 0;

    /// The tasks \p robot_id is still occupied by, in id order.
    ///
    /// Plural on purpose. One robot doing one task at a time is an assignment
    /// policy (docs/04_ROBOT_TASK_MODEL.md §16), and this register holds no
    /// policy — it reports what is true so that the module which does hold the
    /// policy can enforce it.
    [[nodiscard]] virtual std::vector<core::TaskId> active_tasks_for_robot(
        const core::RobotId& robot_id) const = 0;

    /// The task-set version. See task_version.h.
    [[nodiscard]] virtual TaskVersion version() const noexcept = 0;

    // --------------------------------------------------------------- deadline

    /// Live tasks whose deadline has passed at \p now, in id order.
    ///
    /// Finished tasks are excluded even when they missed their deadline: this
    /// answers "what still needs attention", and nothing can be done for work
    /// that is over. Whether a completed task met its deadline is a question
    /// for the KPI report, which reads `completed_at` against `deadline`.
    [[nodiscard]] virtual std::vector<core::TaskId> overdue_tasks(core::TimePoint now) const = 0;
};

/// The one implementation. docs/23_SYSTEM_ARCHITECTURE.md §22.
class TaskManager final : public ITaskManager {
public:
    TaskManager() = default;

    [[nodiscard]] core::Status<TaskRejection> create(domain::Task task,
                                                     core::TimePoint at) override;
    [[nodiscard]] core::Status<TaskRejection> remove(const core::TaskId& task_id) override;
    [[nodiscard]] bool contains(const core::TaskId& task_id) const override;
    [[nodiscard]] std::size_t task_count() const noexcept override;

    [[nodiscard]] TaskOutcome assign(const core::TaskId& task_id,
                                     core::RobotId robot_id,
                                     core::TimePoint at) override;
    [[nodiscard]] TaskOutcome advance(const core::TaskId& task_id,
                                      domain::TaskStatus next,
                                      core::TimePoint at) override;

    [[nodiscard]] core::Status<TaskRejection> set_priority(const core::TaskId& task_id,
                                                           domain::Priority priority,
                                                           core::TimePoint at) override;
    [[nodiscard]] core::Status<TaskRejection> set_deadline(const core::TaskId& task_id,
                                                           std::optional<core::TimePoint> deadline,
                                                           core::TimePoint at) override;

    [[nodiscard]] const TaskRecord* record(const core::TaskId& task_id) const override;
    [[nodiscard]] const domain::Task* task(const core::TaskId& task_id) const override;
    [[nodiscard]] std::vector<domain::Task> tasks() const override;
    [[nodiscard]] std::vector<core::TaskId> tasks_with_status(
        domain::TaskStatus status) const override;
    [[nodiscard]] std::vector<core::TaskId> tasks_for_robot(
        const core::RobotId& robot_id) const override;
    [[nodiscard]] std::vector<core::TaskId> active_tasks_for_robot(
        const core::RobotId& robot_id) const override;
    [[nodiscard]] TaskVersion version() const noexcept override;

    [[nodiscard]] std::vector<core::TaskId> overdue_tasks(core::TimePoint now) const override;

private:
    /// Finds a record for writing, or nullptr.
    [[nodiscard]] TaskRecord* find(const core::TaskId& task_id);
    [[nodiscard]] const TaskRecord* find(const core::TaskId& task_id) const;

    /// Bumps the task-set version and stamps \p record with it.
    void touch(TaskRecord& record, core::TimePoint at);

    /// Ordered on purpose — see `tasks()`.
    std::map<core::TaskId, TaskRecord> tasks_;

    TaskVersion version_;
};

// --------------------------------------------------------------------- naming

// Names for the lifecycle steps docs/22_IMPLEMENTATION_WORKFLOW.md Phase 4
// lists as its completion criteria. Free functions rather than members: they
// add no behaviour, and an interface that can be implemented wrongly in seven
// places instead of one is a worse interface.

/// A route is being computed for the task.
[[nodiscard]] TaskOutcome start_planning(ITaskManager& tasks,
                                         const core::TaskId& task_id,
                                         core::TimePoint at);

/// The robot is executing it. Also the way back from `waiting`.
[[nodiscard]] TaskOutcome start(ITaskManager& tasks,
                                const core::TaskId& task_id,
                                core::TimePoint at);

/// Execution is interrupted but the task is still live.
[[nodiscard]] TaskOutcome hold(ITaskManager& tasks,
                               const core::TaskId& task_id,
                               core::TimePoint at);

/// Finished successfully.
[[nodiscard]] TaskOutcome complete(ITaskManager& tasks,
                                   const core::TaskId& task_id,
                                   core::TimePoint at);

/// Withdrawn before completion.
[[nodiscard]] TaskOutcome cancel(ITaskManager& tasks,
                                 const core::TaskId& task_id,
                                 core::TimePoint at);

/// Could not be completed.
[[nodiscard]] TaskOutcome fail(ITaskManager& tasks,
                               const core::TaskId& task_id,
                               core::TimePoint at);

/// The tasks in \p status, highest priority first, ties broken by task id.
///
/// docs/04_ROBOT_TASK_MODEL.md §15: the base priority is a plain number and
/// the waiting-time part of the decision belongs to traffic priority, not
/// here. The tie-break on id is what makes two runs of the same scenario order
/// an equal-priority backlog the same way.
[[nodiscard]] std::vector<core::TaskId> by_priority(const ITaskManager& tasks,
                                                    domain::TaskStatus status);

}  // namespace traffic::task
