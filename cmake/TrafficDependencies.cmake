# Third-party dependencies.
#
# FetchContent is used instead of vcpkg/Conan so that a clean checkout builds
# with nothing but CMake and a compiler, which keeps CI setup trivial. Revisit
# this (with an ADR) when Protobuf/gRPC enter the build — those are painful to
# build from source and are better served by a package manager.
#
# Every dependency is pinned to an exact tag. Do not use branch names.

include(FetchContent)

set(FETCHCONTENT_QUIET OFF)

# ---------------------------------------------------------------- fmt --------
FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        11.0.2
    GIT_SHALLOW    TRUE
    SYSTEM
)

# ------------------------------------------------------------- spdlog --------
# docs/19_TECHNOLOGY_DECISION.md §13
set(SPDLOG_FMT_EXTERNAL ON  CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
set(SPDLOG_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(SPDLOG_INSTALL       OFF CACHE BOOL "" FORCE)

FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.15.0
    GIT_SHALLOW    TRUE
    SYSTEM
)

# ------------------------------------------------------- nlohmann/json -------
# docs/19_TECHNOLOGY_DECISION.md §15 already settles on JSON for external data;
# this is an implementation of that decision, not a new one. Map files are
# hand-edited by site engineers, so the format has to be readable and
# diffable — a binary format would make a bad corridor definition impossible to
# review.
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
    SYSTEM
)

FetchContent_MakeAvailable(fmt spdlog nlohmann_json)

# --------------------------------------------------------- googletest --------
# docs/19_TECHNOLOGY_DECISION.md §11
if(TC_BUILD_TESTS)
    set(gtest_force_shared_crt ON  CACHE BOOL "" FORCE)
    set(BUILD_GMOCK            ON  CACHE BOOL "" FORCE)
    set(INSTALL_GTEST          OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.15.2
        GIT_SHALLOW    TRUE
        SYSTEM
    )
    FetchContent_MakeAvailable(googletest)
    include(GoogleTest)
endif()

# ---------------------------------------------------- google benchmark -------
# docs/19_TECHNOLOGY_DECISION.md §12
if(TC_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_TESTING     OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL     OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG        v1.9.0
        GIT_SHALLOW    TRUE
        SYSTEM
    )
    FetchContent_MakeAvailable(benchmark)
endif()
