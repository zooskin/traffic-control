# task — the register of what the warehouse asked for.
#
# docs/23_SYSTEM_ARCHITECTURE.md §22 gives Task to TaskManager and says no
# other component may change it. That ownership is what this module is.
#
# docs/12_SOFTWARE_ARCHITECTURE.md §16 puts task alongside domain, map and
# state, so it may use core and domain and nothing above. In particular it does
# not link tc::state: which robot should do a task is an assignment question
# (docs/04_ROBOT_TASK_MODEL.md §16), and a task register that could see robot
# positions would start answering it.

add_library(tc_task
    task_assignment.cpp
    task_change.cpp
    task_manager.cpp
)
add_library(tc::task ALIAS tc_task)

target_include_directories(tc_task PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_sources(tc_task PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
        "${PROJECT_SOURCE_DIR}/include/traffic/task/task_assignment.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/task/task_change.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/task/task_manager.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/task/task_version.h"
)

target_link_libraries(tc_task
    PUBLIC tc::domain
)

tc_configure_target(tc_task)

set_target_properties(tc_task PROPERTIES FOLDER "src")
