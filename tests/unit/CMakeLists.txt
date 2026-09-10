tc_add_test(tc_core_tests
    LABEL   unit
    SOURCES
        core/strong_id_test.cpp
        core/clock_test.cpp
        core/result_test.cpp
    LIBS
        tc::core
)

tc_add_test(tc_domain_tests
    LABEL   unit
    SOURCES
        domain/values_test.cpp
        domain/state_machines_test.cpp
        domain/entities_test.cpp
        domain/resources_test.cpp
    LIBS
        tc::domain
)

tc_add_test(tc_map_tests
    LABEL   unit
    SOURCES
        map/map_test.cpp
        map/map_loader_test.cpp
    LIBS
        tc::map
)

# The reference map is the ground the simulation scenarios of
# docs/26_TEST_SCENARIOS.md stand on, so the loader test reads the real file
# rather than a copy that could drift from it.
target_compile_definitions(tc_map_tests PRIVATE
    TC_REFERENCE_MAP_PATH="${PROJECT_SOURCE_DIR}/configs/maps/reference_map.json"
)

tc_add_test(tc_infrastructure_tests
    LABEL   unit
    SOURCES
        infrastructure/logging_test.cpp
    LIBS
        tc::infrastructure
)
