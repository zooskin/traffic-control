#include "traffic/domain/values.h"

#include <cmath>

#include "traffic/core/result.h"
#include "traffic/domain/errors.h"

namespace traffic::domain {

double distance(const Position& from, const Position& to) noexcept {
    const double dx = to.x - from.x;
    const double dy = to.y - from.y;
    const double dz = to.z - from.z;
    return std::sqrt((dx * dx) + (dy * dy) + (dz * dz));
}

core::Result<TimeWindow, DomainError> make_time_window(core::TimePoint start, core::TimePoint end) {
    if (end <= start) {
        return core::Result<TimeWindow, DomainError>::failure(DomainError::invalid_time_window);
    }
    return core::Result<TimeWindow, DomainError>::success(TimeWindow{start, end});
}

}  // namespace traffic::domain
