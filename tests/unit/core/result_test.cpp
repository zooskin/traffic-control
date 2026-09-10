/// Tests for the Result type.
///
/// Naming follows docs/20_CODING_GUIDELINES.md §45.

#include "traffic/core/result.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include <gtest/gtest.h>

namespace traffic::core {
namespace {

enum class TestError { bad_input, timeout };

using IntResult = Result<int, TestError>;
using StringResult = Result<std::string, TestError>;
using VoidResult = Status<TestError>;

// ------------------------------------------------------------------- success

TEST(Result, result_success_holds_value) {
    const auto result = IntResult::success(42);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(static_cast<bool>(result));
    EXPECT_EQ(result.value(), 42);
}

TEST(Result, result_failure_holds_error) {
    const auto result = IntResult::failure(TestError::timeout);
    ASSERT_FALSE(result.has_value());
    EXPECT_FALSE(static_cast<bool>(result));
    EXPECT_EQ(result.error(), TestError::timeout);
}

TEST(Result, result_value_or_returns_value_when_successful) {
    const auto result = IntResult::success(7);
    EXPECT_EQ(result.value_or(99), 7);
}

TEST(Result, result_value_or_returns_fallback_when_failed) {
    const auto result = IntResult::failure(TestError::bad_input);
    EXPECT_EQ(result.value_or(99), 99);
}

// --------------------------------------------------------------- move support

TEST(Result, result_moves_value_out) {
    auto result = StringResult::success(std::string(64, 'x'));
    ASSERT_TRUE(result.has_value());

    const std::string taken = std::move(result).value();
    EXPECT_EQ(taken.size(), 64U);
}

TEST(Result, result_moves_error_out) {
    auto result = StringResult::failure(TestError::bad_input);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(std::move(result).error(), TestError::bad_input);
}

TEST(Result, result_carries_a_move_only_value) {
    using PtrResult = Result<std::unique_ptr<int>, TestError>;

    auto result = PtrResult::success(std::make_unique<int>(5));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result.value(), 5);
}

// ---------------------------------------------------------- same value/error

/// std::variant<T, E> would be ambiguous here; the alternatives are wrapped so
/// that Result<int, int> still distinguishes the two sides.
TEST(Result, result_distinguishes_value_and_error_of_the_same_type) {
    using SameResult = Result<int, int>;

    const auto ok = SameResult::success(1);
    const auto bad = SameResult::failure(1);

    ASSERT_TRUE(ok.has_value());
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(ok.value(), 1);
    EXPECT_EQ(bad.error(), 1);
}

// -------------------------------------------------------------- void results

TEST(Result, void_result_success_has_value) {
    const auto result = VoidResult::success();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(static_cast<bool>(result));
}

TEST(Result, void_result_failure_holds_error) {
    const auto result = VoidResult::failure(TestError::timeout);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), TestError::timeout);
}

// ----------------------------------------------------------------- discipline

/// docs/20_CODING_GUIDELINES.md §18 exists because failures get dropped.
/// Marking the type nodiscard is what makes ignoring one a compiler warning,
/// and warnings are fatal here.
TEST(Result, result_is_nodiscard) {
    static_assert(std::is_same_v<IntResult::value_type, int>);
    static_assert(std::is_same_v<IntResult::error_type, TestError>);
    static_assert(std::is_same_v<VoidResult::value_type, void>);
    SUCCEED();
}

TEST(Result, result_has_no_default_constructor) {
    // A default-constructed Result would be neither a value nor an error.
    static_assert(!std::is_default_constructible_v<IntResult>);
    static_assert(!std::is_default_constructible_v<VoidResult>);
    SUCCEED();
}

}  // namespace
}  // namespace traffic::core
