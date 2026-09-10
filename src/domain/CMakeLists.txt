# domain — entities and value objects.
#
# docs/24_DOMAIN_MODEL.md §31: the domain does not depend on a database, the
# network, ROS, gRPC, a vendor SDK or the filesystem. It links core and nothing
# else. In particular it must not link tc::infrastructure — logging, config and
# metrics sit above the domain, not beneath it.

add_library(tc_domain
    errors.cpp
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
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/errors.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/domain/values.h"
)

target_link_libraries(tc_domain PUBLIC tc::core)

tc_configure_target(tc_domain)

set_target_properties(tc_domain PROPERTIES FOLDER "src")
