#include "traffic/domain/robot_state.h"

#include <array>
#include <cstddef>

namespace traffic::domain {
namespace {

constexpr std::array<RobotState, kRobotStateCount> kAllStates{
    RobotState::idle,
    RobotState::reserving,
    RobotState::moving,
    RobotState::waiting,
    RobotState::temporarily_stopped,
    RobotState::blocked,
    RobotState::replanning,
    RobotState::failed,
    RobotState::unknown,
};

[[nodiscard]] constexpr std::size_t index_of(RobotState state) noexcept {
    return static_cast<std::size_t>(state);
}

/// The transition table, written as a grid so the whole policy is visible at
/// once rather than scattered through an `if` chain.
///
/// Rows are the current state, columns the requested one, both in declaration
/// order. Notes on the entries that are not obvious:
///
///   * Every state may move to `failed` and to `unknown`. A robot can break
///     down or fall silent at any moment; refusing the transition would leave
///     the model asserting something the fleet has already contradicted.
///   * Every state may return to `idle` — arrival, task cancellation and
///     recovery all end with a robot holding no task.
///   * `idle` cannot jump straight to `waiting`, `temporarily_stopped`,
///     `blocked` or `replanning`. All four describe interrupted progress, and
///     an idle robot has no progress to interrupt.
///   * `unknown` may move anywhere: the first observation after a
///     communication gap has to be accepted whatever it reports.
///   * `failed` leads only to `idle` (after repair) or `unknown`. It must not
///     jump back to `moving`, because recovery has to release its resources
///     first; skipping that leaves the reservation table holding entries no
///     robot owns.
///   * The diagonal is true throughout — see is_transition_allowed.
constexpr std::array<std::array<bool, kRobotStateCount>, kRobotStateCount> kTransitions{{
    // to:      idle   reserving moving waiting t_stopped blocked replanning failed unknown
    /* idle      */ {{true, true, true, false, false, false, false, true, true}},
    /* reserving */ {{true, true, true, true, true, true, true, true, true}},
    /* moving    */ {{true, true, true, true, true, true, true, true, true}},
    /* waiting   */ {{true, true, true, true, true, true, true, true, true}},
    /* t_stopped */ {{true, true, true, true, true, true, true, true, true}},
    /* blocked   */ {{true, true, true, true, true, true, true, true, true}},
    /* replanning*/ {{true, true, true, true, true, true, true, true, true}},
    /* failed    */ {{true, false, false, false, false, false, false, true, true}},
    /* unknown   */ {{true, true, true, true, true, true, true, true, true}},
}};

// The table is indexed by the enumerator's underlying value, so the two must
// stay in the same order. If someone inserts a state in the middle of the enum
// without touching the table, this fails at compile time rather than silently
// permitting the wrong transitions.
static_assert(kAllStates.size() == kRobotStateCount);
static_assert(index_of(RobotState::idle) == 0);
static_assert(index_of(RobotState::reserving) == 1);
static_assert(index_of(RobotState::moving) == 2);
static_assert(index_of(RobotState::waiting) == 3);
static_assert(index_of(RobotState::temporarily_stopped) == 4);
static_assert(index_of(RobotState::blocked) == 5);
static_assert(index_of(RobotState::replanning) == 6);
static_assert(index_of(RobotState::failed) == 7);
static_assert(index_of(RobotState::unknown) == 8);

}  // namespace

std::span<const RobotState> robot_states() noexcept {
    return kAllStates;
}

bool is_transition_allowed(RobotState from, RobotState to) noexcept {
    return kTransitions[index_of(from)][index_of(to)];
}

bool is_halted(RobotState state) noexcept {
    switch (state) {
        case RobotState::waiting:
        case RobotState::temporarily_stopped:
        case RobotState::blocked:
        case RobotState::replanning:
            return true;
        case RobotState::idle:
        case RobotState::reserving:
        case RobotState::moving:
        case RobotState::failed:
        case RobotState::unknown:
            return false;
    }
    return false;
}

bool is_controllable(RobotState state) noexcept {
    switch (state) {
        case RobotState::idle:
        case RobotState::reserving:
        case RobotState::moving:
        case RobotState::waiting:
        case RobotState::temporarily_stopped:
        case RobotState::blocked:
        case RobotState::replanning:
            return true;
        case RobotState::failed:
        case RobotState::unknown:
            return false;
    }
    return false;
}

std::string_view to_string(RobotState state) noexcept {
    switch (state) {
        case RobotState::idle:
            return "IDLE";
        case RobotState::reserving:
            return "RESERVING";
        case RobotState::moving:
            return "MOVING";
        case RobotState::waiting:
            return "WAITING";
        case RobotState::temporarily_stopped:
            return "TEMPORARILY_STOPPED";
        case RobotState::blocked:
            return "BLOCKED";
        case RobotState::replanning:
            return "REPLANNING";
        case RobotState::failed:
            return "FAILED";
        case RobotState::unknown:
            return "UNKNOWN";
    }
    return "UNKNOWN";
}

std::optional<RobotState> robot_state_from_string(std::string_view name) noexcept {
    for (const RobotState state : kAllStates) {
        if (to_string(state) == name) {
            return state;
        }
    }
    return std::nullopt;
}

}  // namespace traffic::domain
