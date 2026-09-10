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
    LIBS
        tc::domain
)

tc_add_test(tc_infrastructure_tests
    LABEL   unit
    SOURCES
        infrastructure/logging_test.cpp
    LIBS
        tc::infrastructure
)
