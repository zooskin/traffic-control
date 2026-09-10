# Test pyramid per docs/16_TEST_STRATEGY.md §2.
#
#   unit         fast, isolated, no I/O
#   integration  module combinations
#   simulation   scenarios from docs/26_TEST_SCENARIOS.md
#   stress       scale and soak
#
# Every test must be deterministic (docs/20_CODING_GUIDELINES.md §46): no
# wall-clock reads, no unseeded randomness, no dependence on iteration order of
# unordered containers.

# Helper: register a test executable with ctest.
function(tc_add_test name)
    cmake_parse_arguments(ARG "" "LABEL" "SOURCES;LIBS" ${ARGN})

    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE GTest::gtest_main GTest::gmock ${ARG_LIBS})
    tc_configure_target(${name})
    set_target_properties(${name} PROPERTIES FOLDER "tests")

    gtest_discover_tests(${name}
        PROPERTIES LABELS "${ARG_LABEL}"
        DISCOVERY_MODE PRE_TEST
    )
endfunction()

add_subdirectory(unit)

# Phase 6+  : add_subdirectory(integration)
# Phase 13  : add_subdirectory(simulation)
# Phase 16  : add_subdirectory(stress)
