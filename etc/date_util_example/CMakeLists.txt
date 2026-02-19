cmake_minimum_required(VERSION 3.21)

project(vtz_api)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(PROJECT_IS_TOP_LEVEL AND UNIX)
    add_compile_options(
        -fdiagnostics-color=always
        -Werror=uninitialized
        -Wunused
        -Wimplicit-fallthrough
        -Wall
        -Wextra
    )
endif()

find_package(vtz REQUIRED)
add_executable(date src/date.cpp)
target_link_libraries(date vtz::vtz)

