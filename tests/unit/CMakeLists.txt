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
    LIBS
        tc::map
)

tc_add_test(tc_infrastructure_tests
    LABEL   unit
    SOURCES
        infrastructure/logging_test.cpp
    LIBS
        tc::infrastructure
)
