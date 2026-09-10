#pragma once

/// \file
/// Result type for operations that can fail.
///
/// docs/20_CODING_GUIDELINES.md §18: domain failures are reported through an
/// explicit result type, not by throwing. The guideline names
/// `std::expected<Route, PlanningError>` as the shape to aim for — that is
/// C++23, and docs/19_TECHNOLOGY_DECISION.md §10 pins the project to C++20, so
/// this is the project-defined equivalent the same section permits.
///
/// Deliberately small. It carries a value or an error and nothing else: no
/// monadic chaining, no reference support. Add those when a call site needs
/// them rather than in anticipation.
///
/// Migration note: the interface mirrors `std::expected` closely enough
/// (`has_value`, `value`, `error`, `value_or`, `operator bool`) that moving to
/// the standard type on a future C++23 switch is mostly a rename.

#include <cassert>
#include <type_traits>
#include <utility>
#include <variant>

namespace traffic::core {

namespace detail {

// Wrapping each alternative keeps std::variant unambiguous when T and E are the
// same type — Result<int, int> is a perfectly reasonable thing to want.
template<typename T>
struct OkStorage {
    T value;
};

template<typename E>
struct ErrStorage {
    E error;
};

}  // namespace detail

/// Holds either a \p T or an \p E.
///
/// `[[nodiscard]]` on the class: silently dropping a Result discards a failure
/// that nothing else will report.
template<typename T, typename E>
class [[nodiscard]] Result {
public:
    using value_type = T;
    using error_type = E;

    static_assert(!std::is_reference_v<T>, "Result does not hold references");
    static_assert(!std::is_reference_v<E>, "Result does not hold references");

    [[nodiscard]] static Result success(T value) {
        return Result{detail::OkStorage<T>{std::move(value)}};
    }

    [[nodiscard]] static Result failure(E error) {
        return Result{detail::ErrStorage<E>{std::move(error)}};
    }

    [[nodiscard]] bool has_value() const noexcept { return storage_.index() == 0; }

    explicit operator bool() const noexcept { return has_value(); }

    /// \pre has_value()
    [[nodiscard]] const T& value() const& noexcept {
        assert(has_value() && "Result::value() on a failed Result");
        return std::get<0>(storage_).value;
    }

    /// \pre has_value()
    [[nodiscard]] T&& value() && noexcept {
        assert(has_value() && "Result::value() on a failed Result");
        return std::move(std::get<0>(storage_).value);
    }

    /// \pre !has_value()
    [[nodiscard]] const E& error() const& noexcept {
        assert(!has_value() && "Result::error() on a successful Result");
        return std::get<1>(storage_).error;
    }

    /// \pre !has_value()
    [[nodiscard]] E&& error() && noexcept {
        assert(!has_value() && "Result::error() on a successful Result");
        return std::move(std::get<1>(storage_).error);
    }

    [[nodiscard]] T value_or(T fallback) const& {
        return has_value() ? value() : std::move(fallback);
    }

private:
    explicit Result(detail::OkStorage<T> ok) : storage_(std::move(ok)) {}
    explicit Result(detail::ErrStorage<E> err) : storage_(std::move(err)) {}

    std::variant<detail::OkStorage<T>, detail::ErrStorage<E>> storage_;
};

/// Result of an operation that yields nothing but can still fail — a state
/// transition, a validation, a release.
template<typename E>
class [[nodiscard]] Result<void, E> {
public:
    using value_type = void;
    using error_type = E;

    [[nodiscard]] static Result success() { return Result{std::monostate{}}; }

    [[nodiscard]] static Result failure(E error) {
        return Result{detail::ErrStorage<E>{std::move(error)}};
    }

    [[nodiscard]] bool has_value() const noexcept { return storage_.index() == 0; }

    explicit operator bool() const noexcept { return has_value(); }

    /// \pre !has_value()
    [[nodiscard]] const E& error() const& noexcept {
        assert(!has_value() && "Result::error() on a successful Result");
        return std::get<1>(storage_).error;
    }

    /// \pre !has_value()
    [[nodiscard]] E&& error() && noexcept {
        assert(!has_value() && "Result::error() on a successful Result");
        return std::move(std::get<1>(storage_).error);
    }

private:
    // A variant rather than an optional, so this specialisation stores its
    // error exactly the way the primary template does. That is worth more than
    // the slightly shorter code an optional gives: one storage mechanism to
    // reason about instead of two.
    //
    // It also removes a real hazard. The assert above disappears under NDEBUG,
    // and dereferencing an empty optional there is undefined — a fault that
    // corrupts something else and gets diagnosed hours later somewhere
    // unrelated. `std::get` on the wrong alternative throws instead, which
    // inside a noexcept function terminates at the offending call. A crash at
    // the fault beats silent corruption away from it, and it is the contract
    // std::expected documents for the same accessor.
    explicit Result(std::variant<std::monostate, detail::ErrStorage<E>> storage)
        : storage_(std::move(storage)) {}

    std::variant<std::monostate, detail::ErrStorage<E>> storage_;
};

/// An operation that reports only whether it succeeded.
template<typename E>
using Status = Result<void, E>;

}  // namespace traffic::core
