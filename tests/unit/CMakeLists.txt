tc_add_test(tc_core_tests
    LABEL   unit
    SOURCES
        core/strong_id_test.cpp
        core/clock_test.cpp
    LIBS
        tc::core
)

tc_add_test(tc_infrastructure_tests
    LABEL   unit
    SOURCES
        infrastructure/logging_test.cpp
    LIBS
        tc::infrastructure
)
