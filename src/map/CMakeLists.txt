# map — the traffic graph and its validation.
#
# docs/12_SOFTWARE_ARCHITECTURE.md §16 puts map alongside domain in the
# dependency order: it may use core and domain, and nothing above it.

add_library(tc_map
    map.cpp
    map_validator.cpp
)
add_library(tc::map ALIAS tc_map)

target_include_directories(tc_map PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_sources(tc_map PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
        "${PROJECT_SOURCE_DIR}/include/traffic/map/map.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/map/map_validator.h"
)

target_link_libraries(tc_map PUBLIC tc::domain)

tc_configure_target(tc_map)

set_target_properties(tc_map PROPERTIES FOLDER "src")
