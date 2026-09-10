/// Tests that logging is wired up.
///
/// docs/01_REQUIREMENTS.md NFR-004 makes traffic decisions traceable by
/// requirement, so "logging works" is a Phase 0 completion condition
/// (docs/22_IMPLEMENTATION_WORKFLOW.md Phase 0).

#include "traffic/infrastructure/logging.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#include <gtest/gtest.h>

namespace traffic::infrastructure {
namespace {

class LoggingTest : public ::testing::Test {
protected:
    void TearDown() override {
        shutdown_logging();
        if (!log_file_.empty()) {
            std::error_code ec;
            std::filesystem::remove(log_file_, ec);
        }
    }

    std::filesystem::path log_file_;
};

TEST_F(LoggingTest, logging_init_sets_requested_level) {
    init_logging(LogConfig{.level = LogLevel::debug});
    EXPECT_EQ(current_level(), LogLevel::debug);

    init_logging(LogConfig{.level = LogLevel::warn});
    EXPECT_EQ(current_level(), LogLevel::warn);
}

TEST_F(LoggingTest, logging_init_is_idempotent) {
    init_logging(LogConfig{.level = LogLevel::info});
    EXPECT_NO_THROW(init_logging(LogConfig{.level = LogLevel::info}));
    EXPECT_EQ(current_level(), LogLevel::info);
}

TEST_F(LoggingTest, logging_writes_records_to_file) {
    log_file_ = std::filesystem::temp_directory_path() / "tc_logging_test.log";
    std::error_code ec;
    std::filesystem::remove(log_file_, ec);

    init_logging(LogConfig{
        .level = LogLevel::info,
        .file_path = log_file_.string(),
        .async = false,
        .flush_every_record = true,
    });

    // A decision record shaped as docs/20_CODING_GUIDELINES.md §21 asks for.
    TC_LOG_INFO("robot_id={} resource_id={} decision={} reason={}",
                "R001",
                "CORRIDOR-01",
                "GRANT",
                "NO_CONFLICT");

    shutdown_logging();

    std::ifstream in{log_file_};
    ASSERT_TRUE(in.is_open());
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string contents = buffer.str();

    EXPECT_NE(contents.find("robot_id=R001"), std::string::npos);
    EXPECT_NE(contents.find("resource_id=CORRIDOR-01"), std::string::npos);
    EXPECT_NE(contents.find("decision=GRANT"), std::string::npos);
    EXPECT_NE(contents.find("reason=NO_CONFLICT"), std::string::npos);
}

TEST_F(LoggingTest, logging_suppresses_records_below_level) {
    log_file_ = std::filesystem::temp_directory_path() / "tc_logging_level_test.log";
    std::error_code ec;
    std::filesystem::remove(log_file_, ec);

    init_logging(LogConfig{
        .level = LogLevel::error,
        .file_path = log_file_.string(),
        .async = false,
        .flush_every_record = true,
    });

    TC_LOG_INFO("this must not be written");
    TC_LOG_ERROR("this must be written");

    shutdown_logging();

    std::ifstream in{log_file_};
    ASSERT_TRUE(in.is_open());
    std::stringstream buffer;
    buffer << in.rdbuf();
    const std::string contents = buffer.str();

    EXPECT_EQ(contents.find("this must not be written"), std::string::npos);
    EXPECT_NE(contents.find("this must be written"), std::string::npos);
}

TEST_F(LoggingTest, logging_parse_log_level_accepts_known_names) {
    EXPECT_EQ(parse_log_level("trace", LogLevel::info), LogLevel::trace);
    EXPECT_EQ(parse_log_level("debug", LogLevel::info), LogLevel::debug);
    EXPECT_EQ(parse_log_level("warn", LogLevel::info), LogLevel::warn);
    EXPECT_EQ(parse_log_level("warning", LogLevel::info), LogLevel::warn);
    EXPECT_EQ(parse_log_level("error", LogLevel::info), LogLevel::error);
    EXPECT_EQ(parse_log_level("critical", LogLevel::info), LogLevel::critical);
    EXPECT_EQ(parse_log_level("off", LogLevel::info), LogLevel::off);
}

TEST_F(LoggingTest, logging_parse_log_level_unknown_name_returns_fallback) {
    EXPECT_EQ(parse_log_level("verbose", LogLevel::warn), LogLevel::warn);
    EXPECT_EQ(parse_log_level("", LogLevel::critical), LogLevel::critical);
}

TEST_F(LoggingTest, logging_level_names_round_trip) {
    for (const auto level : {LogLevel::trace,
                             LogLevel::debug,
                             LogLevel::info,
                             LogLevel::warn,
                             LogLevel::error,
                             LogLevel::critical,
                             LogLevel::off}) {
        EXPECT_EQ(parse_log_level(to_string(level), LogLevel::info), level);
    }
}

}  // namespace
}  // namespace traffic::infrastructure
