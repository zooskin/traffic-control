# reservation — who owns a traffic resource, and when.
#
# docs/06_TRAFFIC_RESERVATION.md §40: this is not a reservation database. It is
# where a claim is checked, decided and recorded, and no other module changes
# reservation state.
#
# docs/12_SOFTWARE_ARCHITECTURE.md §16 puts reservation alongside planning and
# priority, above map and domain. It links tc::map because the map is the
# authority on capacity, on which resources are corridors, and on which
# movements exclude each other — keeping a second copy of those facts here is
# how the two would come to disagree, and a disagreement about a corridor's
# capacity is two robots inside it.
#
# It must not link tc::planning: they are the same tier. Congestion and
# expected waiting reach the cost model through planning's ITrafficConditions,
# which something above both implements in Phase 11.

add_library(tc_reservation
    conflict_detector.cpp
    conflict_rules.cpp
    reservation_manager.cpp
    reservation_policy.cpp
    reservation_request.cpp
    reservation_table.cpp
)
add_library(tc::reservation ALIAS tc_reservation)

target_include_directories(tc_reservation PUBLIC
    "${PROJECT_SOURCE_DIR}/include"
)

target_sources(tc_reservation PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${PROJECT_SOURCE_DIR}/include"
    FILES
        "${PROJECT_SOURCE_DIR}/include/traffic/reservation/conflict_detector.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/reservation/conflict_rules.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/reservation/reservation_manager.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/reservation/reservation_policy.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/reservation/reservation_request.h"
        "${PROJECT_SOURCE_DIR}/include/traffic/reservation/reservation_table.h"
)

target_link_libraries(tc_reservation
    PUBLIC tc::map
)

tc_configure_target(tc_reservation)

set_target_properties(tc_reservation PROPERTIES FOLDER "src")
