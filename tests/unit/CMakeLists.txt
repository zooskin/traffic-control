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

tc_add_test(tc_planning_tests
    LABEL   unit
    SOURCES
        planning/astar_planner_test.cpp
        planning/cost_model_test.cpp
        planning/heuristic_test.cpp
        planning/planning_record_test.cpp
        planning/route_request_test.cpp
        planning/route_stability_test.cpp
        planning/route_validator_test.cpp
        planning/sequential_fleet_planner_test.cpp
    LIBS
        tc::planning
)

tc_add_test(tc_state_tests
    LABEL   unit
    SOURCES
        state/robot_command_test.cpp
        state/robot_state_update_test.cpp
        state/state_change_test.cpp
        state/state_manager_test.cpp
        state/stall_policy_test.cpp
        state/waiting_test.cpp
    LIBS
        tc::state
)

tc_add_test(tc_task_tests
    LABEL   unit
    SOURCES
        task/task_assignment_test.cpp
        task/task_change_test.cpp
        task/task_manager_test.cpp
    LIBS
        tc::task
)

tc_add_test(tc_reservation_tests
    LABEL   unit
    SOURCES
        reservation/conflict_detector_test.cpp
        reservation/conflict_rules_test.cpp
        reservation/reservation_manager_test.cpp
        reservation/reservation_policy_test.cpp
        reservation/reservation_table_test.cpp
    LIBS
        tc::reservation
)

tc_add_test(tc_infrastructure_tests
    LABEL   unit
    SOURCES
        infrastructure/logging_test.cpp
    LIBS
        tc::infrastructure
)
