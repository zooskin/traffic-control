#pragma once

/// \file
/// Strongly typed identifiers.
///
/// docs/20_CODING_GUIDELINES.md §15 forbids passing distinct identifiers around
/// as bare `std::string`. A `RobotId` must not be assignable to a `TaskId`, and
/// the compiler — not review — should be what enforces that.
///
/// docs/24_DOMAIN_MODEL.md §26 requires identifiers to stay human readable
/// (`R001`, `TASK-00001`, `CORRIDOR-01`) and leaves the concrete format to the
/// implementation.
///
/// Note on performance: the value is stored as a `std::string`, which is fine
/// at the current scale but will show up in reservation-table hot paths at 200+
/// robots. Interning to a numeric handle is the expected optimisation. It is
/// deliberately deferred — see docs/00_INDEX.md, changing this is an
/// architecture decision, not a local refactor.

#include <compare>
#include <cstddef>
#include <functional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace traffic::core {

/// A distinct identifier type, separated from every other by its \p Tag.
///
/// Ordering is defined so identifiers can key `std::map` / `std::set`.
/// docs/20_CODING_GUIDELINES.md §23 requires deterministic behaviour, and
/// ordered containers give a reproducible iteration order that the unordered
/// ones do not.
template <typename Tag>
class StrongId {
public:
    using value_type = std::string;
    using tag_type = Tag;

    StrongId() = default;

    explicit StrongId(std::string value) noexcept : value_(std::move(value)) {}
    explicit StrongId(std::string_view value) : value_(value) {}
    explicit StrongId(const char* value) : value_(value) {}

    [[nodiscard]] const std::string& value() const noexcept { return value_; }
    [[nodiscard]] bool empty() const noexcept { return value_.empty(); }

    [[nodiscard]] friend bool operator==(const StrongId&, const StrongId&) = default;
    [[nodiscard]] friend std::strong_ordering operator<=>(const StrongId&,
                                                          const StrongId&) = default;

    friend std::ostream& operator<<(std::ostream& os, const StrongId& id) {
        return os << id.value_;
    }

private:
    std::string value_;
};

/// Free function so generic code can stringify any identifier uniformly.
template <typename Tag>
[[nodiscard]] const std::string& to_string(const StrongId<Tag>& id) noexcept {
    return id.value();
}

}  // namespace traffic::core

namespace std {

template <typename Tag>
struct hash<::traffic::core::StrongId<Tag>> {
    [[nodiscard]] size_t operator()(const ::traffic::core::StrongId<Tag>& id) const noexcept {
        return hash<string>{}(id.value());
    }
};

}  // namespace std
