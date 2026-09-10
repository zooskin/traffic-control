#include "traffic/infrastructure/logging.h"

#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include <memory>
#include <vector>

namespace traffic::infrastructure {
namespace {

constexpr const char* kLoggerName = "traffic";

// Fields required by docs/20_CODING_GUIDELINES.md §21 that are not part of the
// message itself: timestamp, level, thread. The rest (robot_id, task_id,
// resource_id, event_id, decision, reason) belong to the call site.
constexpr const char* kPattern = "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v";

constexpr std::size_t kAsyncQueueSize = 8192;
constexpr std::size_t kAsyncThreadCount = 1;

[[nodiscard]] spdlog::level::level_enum to_spdlog(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::trace:    return spdlog::level::trace;
        case LogLevel::debug:    return spdlog::level::debug;
        case LogLevel::info:     return spdlog::level::info;
        case LogLevel::warn:     return spdlog::level::warn;
        case LogLevel::error:    return spdlog::level::err;
        case LogLevel::critical: return spdlog::level::critical;
        case LogLevel::off:      return spdlog::level::off;
    }
    return spdlog::level::info;
}

[[nodiscard]] LogLevel from_spdlog(spdlog::level::level_enum level) noexcept {
    switch (level) {
        case spdlog::level::trace:    return LogLevel::trace;
        case spdlog::level::debug:    return LogLevel::debug;
        case spdlog::level::info:     return LogLevel::info;
        case spdlog::level::warn:     return LogLevel::warn;
        case spdlog::level::err:      return LogLevel::error;
        case spdlog::level::critical: return LogLevel::critical;
        case spdlog::level::off:      return LogLevel::off;
        default:                      return LogLevel::info;
    }
}

}  // namespace

void init_logging(const LogConfig& config) {
    // Reconfiguring means dropping whatever was registered before.
    spdlog::drop(kLoggerName);

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    if (!config.file_path.empty()) {
        sinks.push_back(
            std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.file_path, true));
    }

    std::shared_ptr<spdlog::logger> logger;
    if (config.async) {
        spdlog::init_thread_pool(kAsyncQueueSize, kAsyncThreadCount);
        logger = std::make_shared<spdlog::async_logger>(kLoggerName,
                                                        sinks.begin(),
                                                        sinks.end(),
                                                        spdlog::thread_pool(),
                                                        spdlog::async_overflow_policy::block);
    } else {
        logger = std::make_shared<spdlog::logger>(kLoggerName, sinks.begin(), sinks.end());
    }

    logger->set_pattern(kPattern);
    logger->set_level(to_spdlog(config.level));
    logger->flush_on(config.flush_every_record ? to_spdlog(config.level)
                                               : spdlog::level::warn);

    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
}

void shutdown_logging() {
    spdlog::shutdown();
}

LogLevel current_level() {
    const auto& logger = spdlog::default_logger_raw();
    return logger != nullptr ? from_spdlog(logger->level()) : LogLevel::off;
}

LogLevel parse_log_level(std::string_view name, LogLevel fallback) {
    if (name == "trace") return LogLevel::trace;
    if (name == "debug") return LogLevel::debug;
    if (name == "info") return LogLevel::info;
    if (name == "warn" || name == "warning") return LogLevel::warn;
    if (name == "error" || name == "err") return LogLevel::error;
    if (name == "critical") return LogLevel::critical;
    if (name == "off") return LogLevel::off;
    return fallback;
}

std::string_view to_string(LogLevel level) {
    switch (level) {
        case LogLevel::trace:    return "trace";
        case LogLevel::debug:    return "debug";
        case LogLevel::info:     return "info";
        case LogLevel::warn:     return "warn";
        case LogLevel::error:    return "error";
        case LogLevel::critical: return "critical";
        case LogLevel::off:      return "off";
    }
    return "info";
}

}  // namespace traffic::infrastructure
