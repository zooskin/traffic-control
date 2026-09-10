#pragma once

/// \file
/// Domain-level failures.
///
/// docs/24_DOMAIN_MODEL.md §34: invalid state is prevented at construction
/// where possible. Entities are therefore built through factory functions that
/// return `Result<T, DomainError>` rather than through public constructors that
/// can produce a broken object.
///
/// One enum for the whole domain, not one per entity. The set is small, the
/// values read the same wherever they surface, and a single enum keeps the
/// factory signatures uniform.

#include <string_view>

namespace traffic::domain {

enum class DomainError {
    /// An identifier was empty. docs/24_DOMAIN_MODEL.md §26 requires every
    /// entity to carry one.
    empty_id,

    /// A state machine rejected the requested move.
    /// docs/24_DOMAIN_MODEL.md §5, docs/20_CODING_GUIDELINES.md §37.
    invalid_transition,

    /// end_time is not strictly after start_time.
    /// docs/24_DOMAIN_MODEL.md §34.
    invalid_time_window,

    /// A route was built with no nodes. docs/24_DOMAIN_MODEL.md §34.
    empty_route,

    /// Route nodes and edges do not describe a connected path.
    inconsistent_route,

    /// A task was created with source == destination.
    /// docs/24_DOMAIN_MODEL.md §34.
    same_source_and_destination,

    /// A quantity that must be positive was zero or negative — length,
    /// capacity, speed limit.
    non_positive_value,

    /// A quantity that must not be negative was negative.
    negative_value,

    /// An enum value outside its defined set, typically from parsing.
    unknown_value,
};

[[nodiscard]] std::string_view to_string(DomainError error) noexcept;

}  // namespace traffic::domain
