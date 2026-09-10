# core — shared primitives only.
#
# docs/12_SOFTWARE_ARCHITECTURE.md §4: core must not depend on any higher
# module, and it must not depend on infrastructure (logging, config, metrics).
# Keep this target's link list empty apart from warnings.

add_library(tc_core
    clock.cpp
)
add_library(tc::core ALIAS tc_core)

target_include_directories(tc_core PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_sources(tc_core PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
        "${PROJECT_SOURCE_DIR}/include/traffic/core/clock.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/core/ids.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/core/result.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/core/strong_id.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/core/time.h"
)

tc_configure_target(tc_core)

set_target_properties(tc_core PROPERTIES FOLDER "src")
