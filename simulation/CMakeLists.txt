# Traffic simulator — docs/02_SIMULATOR.md.
#
# Phase 0 ships only the entry point, so that "build produces a runnable
# example" is verifiable now (docs/22_IMPLEMENTATION_WORKFLOW.md Phase 0).
# The simulation world arrives in Phase 13.

add_executable(tc_simulator
    main.cpp
)

target_link_libraries(tc_simulator PRIVATE
    tc::core
    tc::infrastructure
)

tc_configure_target(tc_simulator)

set_target_properties(tc_simulator PROPERTIES FOLDER "simulation")
