# Shared warning configuration.
#
# docs/20_CODING_GUIDELINES.md and docs/19_TECHNOLOGY_DECISION.md §28 require
# "Warnings = Error". Link tc::warnings into every project target.

add_library(tc_warnings INTERFACE)
add_library(tc::warnings ALIAS tc_warnings)

if(MSVC)
    target_compile_options(tc_warnings INTERFACE
        /W4
        /permissive-        # conformant preprocessor and two-phase lookup
        /Zc:__cplusplus     # report the real __cplusplus value
        /Zc:preprocessor
        /utf-8
        /EHsc
        /w14242 /w14254 /w14263 /w14265 /w14287
        /w14296 /w14311 /w14545 /w14546 /w14547
        /w14549 /w14555 /w14619 /w14640 /w14826
        /w14905 /w14906 /w14928
    )
    if(TC_WARNINGS_AS_ERRORS)
        target_compile_options(tc_warnings INTERFACE /WX)
    endif()
else()
    target_compile_options(tc_warnings INTERFACE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wformat=2
        -Wimplicit-fallthrough
    )
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(tc_warnings INTERFACE
            -Wmisleading-indentation
            -Wduplicated-cond
            -Wduplicated-branches
            -Wlogical-op
            -Wuseless-cast
        )
    endif()
    if(TC_WARNINGS_AS_ERRORS)
        target_compile_options(tc_warnings INTERFACE -Werror)
    endif()
endif()

# Helper applied to every library and executable in the project.
function(tc_configure_target target)
    target_link_libraries(${target} PRIVATE tc::warnings)
    target_compile_features(${target} PUBLIC cxx_std_20)
    set_target_properties(${target} PROPERTIES
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON)
endfunction()
