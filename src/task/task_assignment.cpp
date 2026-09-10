#include "traffic/task/task_assignment.h"

#include <span>
#include <utility>
#include <vector>

#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/domain/task.h"
#include "traffic/domain/task_status.h"
#include "traffic/task/task_manager.h"

namespace traffic::task {

std::vector<AssignmentRequest> assignment_requests(const ITaskManager& tasks, core::TimePoint at) {
    std::vector<AssignmentRequest> requests;

    // Only `created` tasks are looking for a robot. A task in `assigned` or
    // beyond already has one, and offering it again would let two engines hand
    // the same work to two robots.
    for (const core::TaskId& task_id : by_priority(tasks, domain::TaskStatus::created)) {
        const domain::Task* task = tasks.task(task_id);
        if (task == nullptr) {
            continue;
        }

        AssignmentRequest request;
        request.task_id = task->id;
        request.source = task->source;
        request.destination = task->destination;
        request.priority = task->priority;
        request.deadline = task->deadline;
        request.requested_at = at;
        requests.push_back(std::move(request));
    }

    return requests;
}

std::vector<AssignmentProposal> apply_assignments(ITaskManager& tasks,
                                                  std::span<const AssignmentProposal> proposals,
                                                  core::TimePoint at) {
    std::vector<AssignmentProposal> refused;
    for (const AssignmentProposal& proposal : proposals) {
        if (!tasks.assign(proposal.task_id, proposal.robot_id, at).has_value()) {
            // Not a malfunction: the engine answered a question that has since
            // gone stale. It is told so it can offer that robot elsewhere.
            refused.push_back(proposal);
        }
    }
    return refused;
}

}  // namespace traffic::task
