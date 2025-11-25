# Need Version 3.23 for FILE_SET
cmake_minimum_required(VERSION 3.23)

project(vtz)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD "${CMAKE_CURRENT_BINARY_DIR}")

# Add dev compile flags (if on unix)
if(PROJECT_IS_TOP_LEVEL AND UNIX)
    add_compile_options(
        -fdiagnostics-color=always
        -Werror=uninitialized
        -Wunused
        -Wimplicit-fallthrough
    )
endif()

# (For development) add SANITIZE and PROFILE compile flags when on unix
if(PROJECT_IS_TOP_LEVEL AND UNIX)
    set(LINK_FLAGS_SANITIZE "-fsanitize=address,undefined")
    set(CMAKE_CXX_FLAGS_SANITIZE "-fsanitize=address,undefined -O0 -g")
    set(CMAKE_CXX_FLAGS_PROFILE "-Og -g")
endif()

# (For development) download dependencies
if(PROJECT_IS_TOP_LEVEL)
    include(cmake/get_cpm.cmake)

    CPMAddPackage("gh:fmtlib/fmt#11.2.0")
    CPMAddPackage("gh:martinus/unordered_dense@4.7.0")

    # Test dependencies
    CPMAddPackage("gh:google/googletest@1.17.0")
    CPMAddPackage(
        URI gh:HowardHinnant/date@3.0.4
        OPTIONS
            "BUILD_TZ_LIB ON"
            # Don't perform any downloading; we will obtain the tzdb via cmake
            # for testing purposes
            "MANUAL_TZ_DB ON"
    )
    CPMAddPackage(
        URI gh:google/benchmark@1.9.4
        OPTIONS
            "BENCHMARK_ENABLE_GTEST_TESTS OFF"
    )
endif()

file(GLOB_RECURSE sources include/*.cpp)
add_library(
    vtz
    STATIC
        ${sources})

target_link_libraries(
    vtz
    PRIVATE
        fmt::fmt
    PUBLIC
        unordered_dense::unordered_dense
)
target_compile_features(
    vtz
    PRIVATE
        cxx_std_17
)
add_library(vtz::vtz ALIAS vtz)

file(GLOB_RECURSE headers include/*.h)
target_sources(
    vtz
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES ${headers}
)

# Add tests and benchmarks
if(PROJECT_IS_TOP_LEVEL)
    set(tzdb_file tzdb-2025b.tar.lz)
    cmake_path(GET tzdb_file STEM tzdb_stem)

    file(
        DOWNLOAD
            "https://data.iana.org/time-zones/releases/${tzdb_file}"
            "${BUILD}/${tzdb_file}"
    )

    file(
        ARCHIVE_EXTRACT
        INPUT
            "${BUILD}/${tzdb_file}"
        DESTINATION
            "${BUILD}/data"
    )

    file(
        CREATE_LINK
            "${BUILD}/data/${tzdb_stem}"
            "${BUILD}/data/tzdata"
        SYMBOLIC
    )

    # This is required by the Hinnant date library, which is used for tests and bencmarks
    file(
        DOWNLOAD
            "https://raw.githubusercontent.com/unicode-org/cldr/refs/heads/main/common/supplemental/windowsZones.xml"
            "${BUILD}/data/tzdata/windowsZones.xml"
    )

    add_executable(test_vtz
        test/test_vtz.cpp
        test/test_civil.cpp
        test/test_math.cpp
        test/test_versus_hinnant_tz.cpp
        test/test_formatting.cpp
        test/test_parsing.cpp
        test/test_tzfile.cpp
    )
    target_link_libraries(
        test_vtz
        PRIVATE
            GTest::gtest_main
            vtz::vtz
            fmt::fmt
            date::date
            date::date-tz
    )
    add_executable(
        bench_vtz
        bench/bench_vtz.cpp
    )
    target_compile_features(bench_vtz PRIVATE cxx_std_20)
    target_link_libraries(
        bench_vtz
        PRIVATE
            vtz::vtz
            benchmark::benchmark
            fmt::fmt
            date::date-tz
    )
endif()
