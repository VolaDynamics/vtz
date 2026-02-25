# Need Version 3.23 for FILE_SET
cmake_minimum_required(VERSION 3.23)

project(vtz)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD "${CMAKE_CURRENT_BINARY_DIR}")

# Symbols should be hidden by default
# set(CMAKE_CXX_VISIBILITY_PRESET hidden)
# set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
# TODO: fix symbol visibility
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Code should be position independent
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

option(VTZ_ONLY "Only build the vtz library. Don't build tests or benchmarks" OFF)

set(
    VTZ_TZDATA_PATH_VARS
    "VTZ_TZDATA_PATH"
    CACHE
    STRING
    "List of enviroment variable names to check for timezone source files"
)

option(
    VTZ_CHECK_IN_TZDATA
    "If this option is enabled, vtz will look for the timezone source 
    files inside ${VTZ_TZDATA_PATH}/tzdata/. Otherwise, it will look 
    directly inside ${VTZ_TZDATA_PATH}/"
    OFF
)

option(
    VTZ_REQUIRE_TZDATA
    "If this option is enabled, vtz will ONLY check for tz database 
    source files. vtz will not attempt to load compiled tzif files from 
    /usr/share/zoneinfo. Note: windows does not have compiled tzif files
    so vtz acts as if this option is always enabled on that platform."
    OFF
)


# Set up ccache/sccache for the project
find_program(COMPILER_CACHE sccache ccache)
if(COMPILER_CACHE)
    message("Using compile cache @ ${COMPILER_CACHE}")
    set(CMAKE_C_COMPILER_LAUNCHER "${COMPILER_CACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${COMPILER_CACHE}")
endif()

# Add dev compile flags (if on unix)
if(PROJECT_IS_TOP_LEVEL)
    if(UNIX)
        add_compile_options(
            -fdiagnostics-color=always
            -Werror=uninitialized
            -Wunused
            -Wimplicit-fallthrough
            -Wnarrowing
            -Wunused-parameter
        )
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-Wshorten-64-to-32)
        endif()
    elseif(MSVC)
        add_compile_options("/W4")
    endif()
endif()

# (For development) add SANITIZE and PROFILE compile flags when on unix
if(PROJECT_IS_TOP_LEVEL AND UNIX)
    set(LINK_FLAGS_SANITIZE "-fsanitize=address,undefined")
    set(CMAKE_CXX_FLAGS_SANITIZE "-fsanitize=address,undefined -O0 -g")
    set(CMAKE_CXX_FLAGS_PROFILE "-Og -g")
endif()

if(PROJECT_IS_TOP_LEVEL AND NOT VTZ_ONLY)
    add_subdirectory(etc/examples)
endif()

# (For development) download dependencies
if(PROJECT_IS_TOP_LEVEL AND NOT VTZ_ONLY)
    include(cmake/get_cpm.cmake)

    set(RUN_HAVE_STD_REGEX 1)
    set(HAVE_STD_REGEX ON)

    CPMAddPackage("gh:fmtlib/fmt#11.2.0")

    # For aquiring ankerl::unordered_dense
    #
    # We have provisioned headers for v4.8.1 @ etc/3rd/ankerl
    # CPMAddPackage("gh:martinus/unordered_dense@4.8.1")


    CPMAddPackage(
        URI "gh:abseil/abseil-cpp#20260107.1"
        OPTIONS
            "ABSL_INTERNAL_AT_LEAST_CXX20 ON")

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


file(GLOB_RECURSE src include/api/*.cpp include/impl/*.cpp)
add_library(vtz_objects OBJECT ${src})
target_include_directories(
    vtz_objects
    PRIVATE
        include/impl
        include/api
)
target_compile_definitions(
    vtz_objects
    PRIVATE
        "VTZ_TZDATA_PATH_VARS=\"$<JOIN:${VTZ_TZDATA_PATH_VARS},\"$<COMMA>\">\""
        "$<$<BOOL:${VTZ_CHECK_IN_TZDATA}>:VTZ_CHECK_IN_TZDATA>"
        "$<$<BOOL:${VTZ_REQUIRE_TZDATA}>:VTZ_REQUIRE_TZDATA>"
)
target_link_libraries(
    vtz_objects
    PRIVATE
        unordered_dense::unordered_dense)
target_compile_features(
    vtz_objects
    PRIVATE
        cxx_std_17
)

add_library(vtz $<TARGET_OBJECTS:vtz_objects>)
add_library(vtz::vtz ALIAS vtz)


file(GLOB_RECURSE headers include/api/*.h)
target_sources(
    vtz
    PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include/api
    FILES ${headers}
)


set_target_properties(vtz PROPERTIES VERIFY_INTERFACE_HEADER_SETS ON)

add_library(ankerl_unordered_dense INTERFACE)
target_include_directories(ankerl_unordered_dense INTERFACE etc/3rd/ankerl)
add_library(unordered_dense::unordered_dense ALIAS ankerl_unordered_dense)

add_library(vtz_impl STATIC $<TARGET_OBJECTS:vtz_objects>)
add_library(vtz::impl ALIAS vtz_impl)

target_include_directories(
    vtz_impl
    PUBLIC
        include/impl
        include/api)

target_link_libraries(
    vtz_impl
    PUBLIC
        unordered_dense::unordered_dense)



include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(
    TARGETS
    vtz
    EXPORT vtz
    FILE_SET HEADERS
)
install(
    EXPORT vtz
    NAMESPACE vtz::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vtz
)

configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/vtzConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/vtzConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vtz)

install(FILES "${PROJECT_BINARY_DIR}/vtzConfig.cmake"
            #   "${PROJECT_BINARY_DIR}/vtzConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/vtz)

# Add tests and benchmarks
if(PROJECT_IS_TOP_LEVEL AND NOT VTZ_ONLY)
    file(GLOB_RECURSE src include/extras/*.cpp)
    add_library(vtz_extras ${src})
    target_include_directories(vtz_extras PUBLIC include/extras)
    target_link_libraries(
        vtz_extras
        PUBLIC
            fmt::fmt
            vtz::impl
    )
    add_library(vtz::extras ALIAS vtz_extras)

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

    # This is required by the Hinnant date library, which is used for tests and benchmarks
    file(
        DOWNLOAD
            "https://raw.githubusercontent.com/unicode-org/cldr/refs/heads/main/common/supplemental/windowsZones.xml"
            "${BUILD}/data/tzdata/windowsZones.xml"
        STATUS windows_zones_status
    )
    list(GET windows_zones_status 0 windows_zones_download_errc)
    if(NOT (windows_zones_download_errc STREQUAL "0") )
        list(GET windows_zones_status 1 windows_zones_download_msg)
        message(WARNING "Could not download windowsZones.xml! Error: ${windows_zones_download_msg}")
    endif()

    # Generate known_zones.h from tzdata.zi
    set(TZDATA_ZI_FILE "${BUILD}/data/tzdata/tzdata.zi")
    file(STRINGS "${TZDATA_ZI_FILE}" TZDATA_ZI_LINES REGEX "^Z ")
    set(KNOWN_ZONES_LIST "")
    foreach(line IN LISTS TZDATA_ZI_LINES)
        # Extract zone name (second field after "Z ")
        string(REGEX REPLACE "^Z ([^ ]+) .*" "\\1" zone_name "${line}")
        list(APPEND KNOWN_ZONES_LIST "\"${zone_name}\"")
    endforeach()
    list(JOIN KNOWN_ZONES_LIST ",\n        " KNOWN_ZONES)


    file(STRINGS "${TZDATA_ZI_FILE}" TZDATA_ZI_LINKS REGEX "^L ")
    set(KNOWN_LINKS_LIST "")
    foreach(line IN LISTS TZDATA_ZI_LINKS)
        # Extract Link of form "L <name> <alias>"
        string(REGEX REPLACE "^L ([^ ]+) ([^ ]+)" "{ \"\\1\", \"\\2\" }" zone_link "${line}")
        list(APPEND KNOWN_LINKS_LIST "zone_link${zone_link}")
    endforeach()
    list(JOIN KNOWN_LINKS_LIST ",\n        " KNOWN_LINKS)

    # Configure with KNOWN_ZONES and KNOWN_LINKS
    configure_file(
        "${PROJECT_SOURCE_DIR}/include/impl/vtz/known_zones.h.in"
        "${PROJECT_SOURCE_DIR}/include/impl/vtz/known_zones.h"
        @ONLY
    )


    file(GLOB test_files etc/test/*.cpp)
    add_executable(test_vtz ${test_files})
    target_link_libraries(
        test_vtz
        PRIVATE
            GTest::gtest_main
            vtz::extras # vtz::impl + Formatting code
            fmt::fmt
            date::date
            date::date-tz
    )


    file(GLOB src etc/test_api/*.cpp)
    add_executable(test_vtz_api ${src})
    target_link_libraries(
        test_vtz_api
        PRIVATE
            GTest::gtest_main
            vtz::vtz
            fmt::fmt
            date::date
            date::date-tz
    )

    find_program(zic_exe zic)
    if(zic_exe)
        add_custom_command(
            OUTPUT data/zoneinfo/America/New_York
            COMMAND
                "${zic_exe}"
                ARGS -b fat -d "${BUILD}/data/zoneinfo" "${BUILD}/data/tzdata/tzdata.zi"
            DEPENDS
                "${BUILD}/data/tzdata/tzdata.zi"
        )
        add_custom_target(
            compile_tzdata
            DEPENDS data/zoneinfo/America/New_York
        )
        add_dependencies(test_vtz compile_tzdata)
    else()
        message(WARNING "Cannot find zic.exe - unable to compile zoneinfo files for testing purposes")
    endif()

    file(GLOB bench_files etc/bench/*.cpp)
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
    )

    file(GLOB tools etc/tools/*.cpp)
    foreach(file IN LISTS tools)
        cmake_path(GET file STEM exe)
        add_executable(${exe} ${file})
        target_link_libraries(${exe} PRIVATE vtz::extras fmt::fmt)
    endforeach()
endif()
