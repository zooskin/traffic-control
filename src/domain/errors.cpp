#include "traffic/domain/errors.h"

namespace traffic::domain {

std::string_view to_string(DomainError error) noexcept {
    switch (error) {
        case DomainError::empty_id:
            return "EMPTY_ID";
        case DomainError::invalid_transition:
            return "INVALID_TRANSITION";
        case DomainError::invalid_time_window:
            return "INVALID_TIME_WINDOW";
        case DomainError::empty_route:
            return "EMPTY_ROUTE";
        case DomainError::inconsistent_route:
            return "INCONSISTENT_ROUTE";
        case DomainError::same_source_and_destination:
            return "SAME_SOURCE_AND_DESTINATION";
        case DomainError::non_positive_value:
            return "NON_POSITIVE_VALUE";
        case DomainError::negative_value:
            return "NEGATIVE_VALUE";
        case DomainError::unknown_value:
            return "UNKNOWN_VALUE";
    }
    return "UNKNOWN_VALUE";
}

}  // namespace traffic::domain
