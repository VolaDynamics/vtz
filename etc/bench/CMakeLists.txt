#
# Logic for vtz benchmarks
##########################

# We want the 'bench' target to go in VTZ's build directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${BUILD}")

# <date/tz.h> has two modes of operation. it can read the tz database source,
# or it can read compiled tzif files.
#
# Reading the tz database source works on all platforms, but reading compiled
# tzif files only works on Unix platforms.
#
# If VTZ_ALSO_BENCH_HINNANT_OS_TZDB=ON, then we will compile benchmarks against
# a version of the Hinnant date library with USE_OS_TZDB=1.

if(VTZ_ALSO_BENCH_HINNANT_OS_TZDB)
    add_library(date_os_tzdb_tz STATIC "${date_SOURCE_DIR}/src/tz.cpp")
    target_include_directories(date_os_tzdb_tz
        PUBLIC "${date_SOURCE_DIR}/include")
    target_compile_definitions(date_os_tzdb_tz
        PRIVATE
            # We need to rename namespace 'date' to 'date_os_tzdb'
            # this avoids symbol conflicts with the version of the
            # date library compiled with 'USE_OS_TZDB=0'
            date=date_os_tzdb
            BUILD_TZ_LIB=1
            AUTO_DOWNLOAD=0
            HAS_REMOTE_API=0
            USE_OS_TZDB=1
            INSTALL=.
            ONLY_C_LOCALE=0
            HAS_STRING_VIEW=1
    )
    target_compile_features(date_os_tzdb_tz PRIVATE cxx_std_17)
    find_package(Threads)
    target_link_libraries(date_os_tzdb_tz PRIVATE Threads::Threads)
endif()

# We benchmark against abseil, so we need to add it as a dependency with CPM
CPMAddPackage(
    URI "gh:abseil/abseil-cpp#20260107.1"
    OPTIONS
        "ABSL_INTERNAL_AT_LEAST_CXX20 ON"
)

# Google Benchmark is used for the benchmarks themselves
CPMAddPackage(
    URI gh:google/benchmark@1.9.4
    OPTIONS
        "BENCHMARK_ENABLE_TESTING OFF"
        "BENCHMARK_ENABLE_GTEST_TESTS OFF"
)

file(GLOB bench_files src/*.cpp)

if(VTZ_ALSO_BENCH_HINNANT_OS_TZDB)
    set_source_files_properties(
        src/bench_date_with_os_tzdb.cpp
        PROPERTIES COMPILE_OPTIONS
            "-UUSE_OS_TZDB;-DUSE_OS_TZDB=1;-Ddate=date_os_tzdb"
    )
else()
    list(REMOVE_ITEM bench_files
        "${CMAKE_CURRENT_SOURCE_DIR}/src/bench_date_with_os_tzdb.cpp")
endif()


add_executable(bench_vtz ${bench_files})
target_compile_features(bench_vtz PRIVATE cxx_std_20)

target_link_libraries(
    bench_vtz
    PRIVATE
        vtz::impl
        benchmark::benchmark
        fmt::fmt
        date::date-tz
        absl::time
        absl::time_zone
        $<$<BOOL:${VTZ_ALSO_BENCH_HINNANT_OS_TZDB}>:date_os_tzdb_tz>
)
