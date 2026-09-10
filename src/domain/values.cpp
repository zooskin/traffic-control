#include "traffic/domain/values.h"

#include "traffic/core/result.h"
#include "traffic/domain/errors.h"

namespace traffic::domain {

core::Result<TimeWindow, DomainError> make_time_window(core::TimePoint start, core::TimePoint end) {
    if (end <= start) {
        return core::Result<TimeWindow, DomainError>::failure(DomainError::invalid_time_window);
    }
    return core::Result<TimeWindow, DomainError>::success(TimeWindow{start, end});
}

}  // namespace traffic::domain
