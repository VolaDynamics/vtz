

file(GLOB examples src/*.cpp)

# Add a target for each example file
foreach(example_file IN LISTS examples)
    # Extract the target name from the filename
    # eg, src/vtz_chrono.cpp -> vtz_chrono
    cmake_path(GET example_file STEM target)

    # Add an executable for the example
    add_executable(${target} "${example_file}")

    # Add example dependencies.
    #
    # libformat is not a required dependency;
    # we use it for formatting output
    target_link_libraries(
        ${target}
        PRIVATE
            vtz::vtz
            fmt::fmt
    )

    target_compile_features(
        ${target}
        PRIVATE
            cxx_std_17
    )
endforeach()
