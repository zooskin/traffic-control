#pragma once

/// \file
/// Logging setup.
///
/// docs/20_CODING_GUIDELINES.md §20 requires every domain decision to be
/// logged; §21 lists the fields a decision record should carry (timestamp,
/// robot_id, task_id, resource_id, event_id, decision, reason); §22 forbids
/// logging inside tight loops.
///
/// docs/01_REQUIREMENTS.md NFR-004 makes traceability of traffic decisions a
/// requirement, not a debugging aid.
///
/// Only the setup lives here. Call sites use the `TC_LOG_*` macros so that the
/// backend stays replaceable and disabled levels cost nothing.

#include <spdlog/spdlog.h>

#include <string>

namespace traffic::infrastructure {

enum class LogLevel { trace, debug, info, warn, error, critical, off };

struct LogConfig {
    LogLevel level{LogLevel::info};

    /// Empty disables file output.
    std::string file_path{};

    /// Write from a background thread. Keeps the decision loop off the I/O
    /// path — see docs/23_SYSTEM_ARCHITECTURE.md §21.
    bool async{false};

    /// Flush every record. Costly; for post-mortem debugging only.
    bool flush_every_record{false};
};

/// Installs the project logger. Idempotent — a second call reconfigures.
void init_logging(const LogConfig& config);

/// Restores the default logger and drains pending records.
void shutdown_logging();

/// Current level of the project logger.
[[nodiscard]] LogLevel current_level();

[[nodiscard]] LogLevel parse_log_level(std::string_view name, LogLevel fallback);
[[nodiscard]] std::string_view to_string(LogLevel level);

}  // namespace traffic::infrastructure

// Level check happens before argument evaluation, so a disabled TC_LOG_TRACE
// does not pay for formatting its arguments.
#define TC_LOG_TRACE(...) SPDLOG_TRACE(__VA_ARGS__)
#define TC_LOG_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define TC_LOG_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define TC_LOG_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define TC_LOG_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)
#define TC_LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
