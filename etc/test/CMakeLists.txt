include(GoogleTest)

file(GLOB src include/test_vtz/*.cpp)

add_library(vtz_testing STATIC ${src})
add_library(vtz::testing ALIAS vtz_testing)
# Add this as an include directory to the test lib, so that
# we can access known_zones.h
target_include_directories(
    vtz_testing
    PUBLIC
        include
    PRIVATE
        "${VTZ_ROOT}/include/api"
        "${VTZ_ROOT}/include/impl")
target_link_libraries(
    vtz_testing
    PUBLIC
        # date::date and date::date-tz are used as a reference implementation
        # we test against it
        date::date
        date::date-tz
        GTest::gtest
        GTest::gmock
        fmt::fmt
)
if(WIN32 AND NOT VTZ_BUILD_SHARED)
    # Needed to fix export logic
    target_compile_definitions(vtz_testing PRIVATE VTZ_STATIC_DEFINE)
endif()


file(GLOB test_files test_impl/*.cpp)
add_executable(test_vtz ${test_files})
target_link_libraries(
    test_vtz
    PRIVATE
        vtz::testing
        vtz::extras # vtz::impl + Formatting code
)
gtest_discover_tests(
    test_vtz
    TEST_PREFIX "impl/"
    EXTRA_ARGS --build "${BUILD}" --testdata "${VTZ_ROOT}/etc/testdata"
    WORKING_DIRECTORY "${BUILD}")


file(GLOB src test_api/*.cpp)
add_executable(test_vtz_api ${src})
target_link_libraries(
    test_vtz_api
    PRIVATE
        vtz::testing
        vtz::vtz
)

set_target_properties(
    test_vtz
    test_vtz_api
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${BUILD}"
)


if(MSVC)
    # Test cases make use of shadowing intentionally - we want to disable:
    #
    # - C4456: declaration of '_test_context_ptr' hides previous local declaration
    # - C4459: declaration of '_current_test_context' hides global declaration
    target_compile_options(test_vtz PRIVATE /wd4456 /wd4459)
    target_compile_options(test_vtz_api PRIVATE /wd4456 /wd4459)
endif()

gtest_discover_tests(
    test_vtz_api
    TEST_PREFIX "api/"
    EXTRA_ARGS --build "${BUILD}" --testdata "${VTZ_ROOT}/etc/testdata"
    WORKING_DIRECTORY "${BUILD}")

if(VTZ_EMBED_TZDB)
    gtest_discover_tests(
        test_vtz_api
        TEST_PREFIX "api_embed/"
        EXTRA_ARGS --no_set_install --build "${BUILD}" --testdata "${VTZ_ROOT}/etc/testdata"
        WORKING_DIRECTORY "${BUILD}")
endif()



# Registers a standalone test with ctest.
#
# By default, the name of the test is 'api/<file stem>', but a suffix can be added to the
# name of the test.
#
# Sometimes an individual executable may be used for multiple tests, but it should only
# built once.
#
# This takes a cpp file as input, and extracts the stem of the file, using it as the
# executable name.
#
# OPTIONS
# - ARGS - CLI args
# - TEST_SUFFIX - test suffix
# - DEATH_TEST - if provided, we expect that the test will return a nonzero exit code
# - ENV_VARS - list of environment variables (and values) that are passed to the test
# - UNSET_ENV_VARS - list of env vars to unset
function(add_standalone_test FILE)
    cmake_parse_arguments(
        SA
        "DEATH_TEST"
        "TEST_SUFFIX"
        "ARGS;ENV_VARS;UNSET_ENV_VARS"
        ${ARGN}
    )

    cmake_path(GET FILE STEM exe_name)

    # Only create the executable target once
    if(NOT TARGET ${exe_name})
        add_executable(${exe_name} ${FILE})
        target_link_libraries(${exe_name} PRIVATE vtz::vtz)
        set_target_properties(${exe_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${BUILD}")
    endif()

    if(SA_DEATH_TEST)
        set(test_name "api/${exe_name}.death_test${SA_TEST_SUFFIX}")
    else()
        set(test_name "api/${exe_name}${SA_TEST_SUFFIX}")
    endif()

    add_test(
        NAME ${test_name}
        COMMAND ${exe_name} ${SA_ARGS}
        WORKING_DIRECTORY "${BUILD}"
    )

    if(SA_DEATH_TEST)
        set_tests_properties(${test_name} PROPERTIES WILL_FAIL TRUE)
    endif()

    if(SA_ENV_VARS)
        set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "${SA_ENV_VARS}")
    endif()

    if(SA_UNSET_ENV_VARS)
        set(unset_list "")
        foreach(var IN LISTS SA_UNSET_ENV_VARS)
            list(APPEND unset_list "${var}=unset:")
        endforeach()
        set_tests_properties(${test_name} PROPERTIES
            ENVIRONMENT_MODIFICATION "${unset_list}")
    endif()
endfunction()

add_standalone_test(
    standalone/test_tzdb_load.cpp
    TEST_SUFFIX .can_load_from_env
    ENV_VARS
        "VTZ_TZDATA_PATH=${BUILD}/data/tzdata"
)

add_standalone_test(
    standalone/test_tzdb_load.cpp
    TEST_SUFFIX .can_load_from_set_install
    ARGS
        "${BUILD}/data/tzdata"
    UNSET_ENV_VARS
        "VTZ_TZDATA_PATH"
)

add_standalone_test(
    standalone/test_tzdb_load.cpp
    DEATH_TEST
    TEST_SUFFIX .bad_env_path
    ENV_VARS
        VTZ_TZDATA_PATH=/bad/env/path
)

add_standalone_test(
    standalone/test_tzdb_load.cpp
    DEATH_TEST
    ARGS
        /bad/set_install/path
    TEST_SUFFIX .bad_set_install_path
    ENV_VARS
        "VTZ_TZDATA_PATH=${BUILD}/data/tzdata"
)
