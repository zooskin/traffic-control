# planning — route computation.
#
# docs/12_SOFTWARE_ARCHITECTURE.md §16 puts planning above map and domain and
# below reservation. It must not link tc::reservation: congestion and expected
# waiting reach the cost model through ITrafficConditions, which Phase 6
# implements on the far side of the interface.

add_library(tc_planning
    cost_model.cpp
    planning_request.cpp
    route_request.cpp
)
add_library(tc::planning ALIAS tc_planning)

target_include_directories(tc_planning PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_sources(tc_planning PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
        "${PROJECT_SOURCE_DIR}/include/traffic/planning/cost_model.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/planning/planning_request.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/planning/route_planner.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/planning/route_request.h"
)

target_link_libraries(tc_planning
    PUBLIC tc::map
)

tc_configure_target(tc_planning)

set_target_properties(tc_planning PROPERTIES FOLDER "src")
