#include "traffic/state/waiting.h"

#include <array>
#include <utility>

namespace traffic::state {
namespace {

// One table, so `to_string` and the parser cannot drift apart. A name that
// serialises one way and parses another shows up as a robot whose reason
// changes across a restart.
constexpr std::array<std::pair<WaitingReason, std::string_view>, 7> kNames{{
    {WaitingReason::resource_occupied, "RESOURCE_OCCUPIED"},
    {WaitingReason::higher_priority_robot, "HIGHER_PRIORITY_ROBOT"},
    {WaitingReason::human_blockage, "HUMAN_BLOCKAGE"},
    {WaitingReason::reservation_denied, "RESERVATION_DENIED"},
    {WaitingReason::congestion, "CONGESTION"},
    {WaitingReason::deadlock_recovery, "DEADLOCK_RECOVERY"},
    {WaitingReason::safety_stop, "SAFETY_STOP"},
}};

}  // namespace

std::string_view to_string(WaitingReason reason) noexcept {
    for (const auto& [value, name] : kNames) {
        if (value == reason) {
            return name;
        }
    }
    return "UNKNOWN";
}

std::optional<WaitingReason> waiting_reason_from_string(std::string_view name) noexcept {
    for (const auto& [value, text] : kNames) {
        if (text == name) {
            return value;
        }
    }
    return std::nullopt;
}

bool is_traffic_resolvable(WaitingReason reason) noexcept {
    switch (reason) {
        case WaitingReason::resource_occupied:
        case WaitingReason::higher_priority_robot:
        case WaitingReason::reservation_denied:
        case WaitingReason::congestion:
        case WaitingReason::deadlock_recovery:
            return true;

        // Neither clears because we rearranged reservations, and replanning
        // for them produces routes that cannot help.
        case WaitingReason::human_blockage:
        case WaitingReason::safety_stop:
            return false;
    }
    return false;
}

core::Duration waited_for(const WaitingContext& context, core::TimePoint now) noexcept {
    if (now <= context.since) {
        return core::Duration::zero();
    }
    return now - context.since;
}

}  // namespace traffic::state
