# state — the fleet's live picture of its robots.
#
# docs/23_SYSTEM_ARCHITECTURE.md §22 gives RobotState to StateManager and says
# no other component may change it. That ownership is what this module is.
#
# docs/12_SOFTWARE_ARCHITECTURE.md §16 puts state alongside domain and map, so
# it may use core and domain and nothing above. In particular it does not link
# tc::map: whether a node exists is the map's question, and a state manager
# that could answer it would start deciding where robots are allowed to be.

add_library(tc_state
    robot_state_update.cpp
    stall_policy.cpp
    waiting.cpp
)
add_library(tc::state ALIAS tc_state)

target_include_directories(tc_state PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_sources(tc_state PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
        "${PROJECT_SOURCE_DIR}/include/traffic/state/robot_state_update.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/state/stall_policy.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/state/waiting.h"
)

target_link_libraries(tc_state
    PUBLIC tc::domain
)

tc_configure_target(tc_state)

set_target_properties(tc_state PROPERTIES FOLDER "src")
