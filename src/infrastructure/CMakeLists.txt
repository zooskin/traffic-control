# infrastructure — logging, config, metrics, clock wiring.
#
# docs/12_SOFTWARE_ARCHITECTURE.md §15. Domain code must not depend on this
# target; dependencies point the other way.

add_library(tc_infrastructure
    logging.cpp
)
add_library(tc::infrastructure ALIAS tc_infrastructure)

target_include_directories(tc_infrastructure PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_sources(tc_infrastructure PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
        "${PROJECT_SOURCE_DIR}/include/traffic/infrastructure/logging.h"
)

target_link_libraries(tc_infrastructure
    PUBLIC  tc::core
            spdlog::spdlog
)

# Compile out trace/debug records in release builds so TC_LOG_TRACE in a hot
# path costs nothing shipped (docs/20_CODING_GUIDELINES.md §22).
target_compile_definitions(tc_infrastructure PUBLIC
    $<IF:$<CONFIG:Debug>,SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE,SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
)

tc_configure_target(tc_infrastructure)

set_target_properties(tc_infrastructure PROPERTIES FOLDER "src")
