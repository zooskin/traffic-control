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

#include <cstdlib>
#include <string_view>

namespace {

using namespace traffic;  // NOLINT(google-build-using-namespace) — entry point only

constexpr int kDemoSteps = 5;
constexpr core::Duration kDemoStep = core::Milliseconds{100};

}  // namespace

int main(int argc, char** argv) {
    auto level = infrastructure::LogLevel::info;
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg{argv[i]};
        if (arg.starts_with("--log-level=")) {
            level = infrastructure::parse_log_level(arg.substr(12), level);
        }
    }

    infrastructure::init_logging(infrastructure::LogConfig{.level = level});

    TC_LOG_INFO("traffic simulator starting (phase 0 skeleton)");

    // Time is injected, never read from the wall clock, so this loop produces
    // the same trace on every machine (docs/01_REQUIREMENTS.md NFR-003).
    core::SimulationClock clock;
    const core::IClock& time = clock;

    const core::RobotId robot{"R001"};
    const core::CorridorId corridor{"CORRIDOR-01"};

    for (int step = 0; step < kDemoSteps; ++step) {
        clock.advance(kDemoStep);
        TC_LOG_INFO("t={}ms robot_id={} resource_id={} decision={} reason={}",
                    std::chrono::duration_cast<core::Milliseconds>(
                        time.now().time_since_epoch())
                        .count(),
                    robot.value(),
                    corridor.value(),
                    "NONE",
                    "PHASE_0_SKELETON");
    }

    TC_LOG_INFO("traffic simulator finished");
    infrastructure::shutdown_logging();

    return EXIT_SUCCESS;
}
