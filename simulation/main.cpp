/// Traffic simulator entry point.
///
/// Phase 0 scope: prove that the library links, that logging works and that
/// simulation time is driven by SimulationClock rather than by the wall clock.
/// The simulation world (map, robots, humans, task generator, metrics, replay)
/// is Phase 13 — see docs/02_SIMULATOR.md.

#include "traffic/core/clock.h"
#include "traffic/core/ids.h"
#include "traffic/core/time.h"
#include "traffic/infrastructure/logging.h"

#include <chrono>
#include <cstdlib>
#include <string_view>

namespace {

constexpr int kDemoSteps = 5;
constexpr traffic::core::Duration kDemoStep = traffic::core::Milliseconds{100};

constexpr std::string_view kLogLevelFlag = "--log-level=";

}  // namespace

int main(int argc, char** argv) {
    using traffic::core::CorridorId;
    using traffic::core::IClock;
    using traffic::core::Milliseconds;
    using traffic::core::RobotId;
    using traffic::core::SimulationClock;
    namespace infra = traffic::infrastructure;

    auto level = infra::LogLevel::info;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg.starts_with(kLogLevelFlag)) {
            level = infra::parse_log_level(arg.substr(kLogLevelFlag.size()), level);
        }
    }

    infra::init_logging(infra::LogConfig{.level = level});

    TC_LOG_INFO("traffic simulator starting (phase 0 skeleton)");

    // Time is injected, never read from the wall clock, so this loop produces
    // the same trace on every machine (docs/01_REQUIREMENTS.md NFR-003).
    SimulationClock clock;
    const IClock& time = clock;

    const RobotId robot{"R001"};
    const CorridorId corridor{"CORRIDOR-01"};

    for (int step = 0; step < kDemoSteps; ++step) {
        clock.advance(kDemoStep);
        TC_LOG_INFO("t={}ms robot_id={} resource_id={} decision={} reason={}",
                    std::chrono::duration_cast<Milliseconds>(time.now().time_since_epoch())
                        .count(),
                    robot.value(),
                    corridor.value(),
                    "NONE",
                    "PHASE_0_SKELETON");
    }

    TC_LOG_INFO("traffic simulator finished");
    infra::shutdown_logging();

    return EXIT_SUCCESS;
}
