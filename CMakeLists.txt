cmake_minimum_required(VERSION 3.25)

project(traffic-control
    VERSION 0.1.0
    DESCRIPTION "Multi-robot traffic control software"
    LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(TC_BUILD_TESTS      "Build unit and integration tests" ${PROJECT_IS_TOP_LEVEL})
option(TC_BUILD_BENCHMARKS "Build micro benchmarks"           OFF)
option(TC_BUILD_SIMULATION "Build the traffic simulator"      ${PROJECT_IS_TOP_LEVEL})
option(TC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

include(TrafficWarnings)
include(TrafficDependencies)

add_subdirectory(src)

if(TC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(TC_BUILD_SIMULATION)
    add_subdirectory(simulation)
endif()

if(TC_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

message(STATUS "")
message(STATUS "traffic-control ${PROJECT_VERSION}")
message(STATUS "  compiler    : ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  build type  : ${CMAKE_BUILD_TYPE}")
message(STATUS "  tests       : ${TC_BUILD_TESTS}")
message(STATUS "  simulation  : ${TC_BUILD_SIMULATION}")
message(STATUS "  benchmarks  : ${TC_BUILD_BENCHMARKS}")
message(STATUS "  warn->error : ${TC_WARNINGS_AS_ERRORS}")
message(STATUS "")
