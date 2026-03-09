include(GoogleTest)

file(GLOB test_files test_impl/*.cpp)
add_executable(test_vtz ${test_files} test_main.cpp)
target_link_libraries(
    test_vtz
    PRIVATE
        GTest::gtest
        vtz::extras # vtz::impl + Formatting code
        fmt::fmt
        date::date
        date::date-tz
)
gtest_discover_tests(
    test_vtz
    TEST_PREFIX "impl/"
    EXTRA_ARGS --vtz_set_install "${BUILD}/data/tzdata"
    WORKING_DIRECTORY "${VTZ_ROOT}")


file(GLOB src test_api/*.cpp)
add_executable(test_vtz_api ${src} test_main.cpp)
target_link_libraries(
    test_vtz_api
    PRIVATE
        GTest::gmock
        vtz::vtz
        fmt::fmt
        date::date
        date::date-tz
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
    EXTRA_ARGS --vtz_set_install "${BUILD}/data/tzdata"
    WORKING_DIRECTORY "${VTZ_ROOT}")
