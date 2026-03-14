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
