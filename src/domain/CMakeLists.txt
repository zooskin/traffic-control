# domain — entities and value objects.
#
# docs/24_DOMAIN_MODEL.md §31: the domain does not depend on a database, the
# network, ROS, gRPC, a vendor SDK or the filesystem. It links core and nothing
# else. In particular it must not link tc::infrastructure — logging, config and
# metrics sit above the domain, not beneath it.

add_library(tc_domain
    conflict.cpp
    errors.cpp
    graph.cpp
    reservation.cpp
    reservation_state.cpp
    robot.cpp
    robot_state.cpp
    route.cpp
    task.cpp
    task_status.cpp
    traffic_decision.cpp
    traffic_event.cpp
    values.cpp
)
add_library(tc::domain ALIAS tc_domain)

target_include_directories(tc_domain PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_sources(tc_domain PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/conflict.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/errors.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/graph.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/reservation.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/reservation_state.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/robot.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/robot_state.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/route.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/task.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/task_status.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/traffic_decision.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/traffic_event.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/values.h"
)

target_link_libraries(tc_domain PUBLIC tc::core)

tc_configure_target(tc_domain)

set_target_properties(tc_domain PROPERTIES FOLDER "src")
