#pragma once

/// \file
/// The version counter for the task set.
///
/// docs/24_DOMAIN_MODEL.md §28 lists the entities that carry a version and
/// Task is not one of them, so no version field is added to `domain::Task`.
/// What a caller still needs is the question §28 says versions exist to
/// answer — "was this computed against a world that has since moved" — asked
/// of the tasks rather than of the robots.
///
/// So the counter lives with the owner instead of inside the entity, exactly
/// as docs/23_SYSTEM_ARCHITECTURE.md §22 places the task under TaskManager.
/// It is fleet-wide for the same reason `state::StateManager` keeps one
/// number for the whole fleet: "has anything I planned around changed" is a
/// question about the set, not about one task, and a per-task counter cannot
/// answer it without walking every task.
///
/// Tagged separately from `domain::StateVersion` so that a task version can
/// never be compared against a state version. The two move at completely
/// different rates — telemetry bumps the fleet version many times a second,
/// while a task changes a handful of times in its life.

#include "traffic/domain/values.h"

namespace traffic::task {

namespace tags {
struct TaskVersion {};
}  // namespace tags

/// Version of the task set. Bumped whenever a task is created, removed, or
/// actually changes.
using TaskVersion = domain::Version<tags::TaskVersion>;

}  // namespace traffic::task
