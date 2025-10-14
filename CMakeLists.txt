# Need Version 3.23 for FILE_SET
cmake_minimum_required(VERSION 3.23)

project(vtz)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(BUILD "${CMAKE_CURRENT_BINARY_DIR}")

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

if(PROJECT_IS_TOP_LEVEL)
    if(UNIX)
        add_compile_options(-fdiagnostics-color=always)
    endif()

    set(LINK_FLAGS_SANITIZE "-fsanitize=address,undefined")
    set(CMAKE_CXX_FLAGS_SANITIZE "-fsanitize=address,undefined -O0 -g")

    set(CMAKE_CXX_FLAGS_PROFILE "-Og -g")

    include(cmake/get_cpm.cmake)

    CPMAddPackage("gh:fmtlib/fmt#11.2.0")
    CPMAddPackage("gh:google/googletest@1.17.0")
    CPMAddPackage("gh:martinus/unordered_dense@4.7.0")

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
    add_library(vtz::vtz ALIAS vtz)

    file(GLOB_RECURSE headers include/*.h)
    target_sources(
        vtz
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES ${headers}
    )


    add_executable(test_vtz test/test_vtz.cpp)
    target_link_libraries(
        test_vtz
        PRIVATE
            GTest::gtest_main
            vtz::vtz
            fmt::fmt
    )
endif()
