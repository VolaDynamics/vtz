# Contributing

## Building the project

The project can be built with:

```sh
cmake -B build && cmake --build build
```

If you want to build with a specific build configuration (eg, Release), this can
be done via `CMAKE_BUILD_TYPE=<build mode>`.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

In addition to the standard build configurations, this project also adds build
types `Sanitize` and `Profile`. `Sanitize` builds with address sanitizer and UB
sanitizer both enabled, while `Profile` builds with `-Og -g` (some optimization,
but debug info present), and is useful for profiling.

### Rebuilding quickly

If no .cpp files have been added, or nothing about the build configuration has
changed, you can rebuild with:

```sh
cmake --build build
```

To do a clean build, use the `--target` option:

```sh
cmake --build build --target clean
```

## Running Tests

The simplest way to run tests is with the test executable:

```sh
build/test_vtz --gtest_filter=<test_name>
```

For example, `TEST( vtz, tz_string ) { ... }` can be run with
`build/test_vtz --gtest_filter=vtz.tz_string`.

This project makes a small modification to the standard gtest. `ASSERT_EQ` will
print out both the lhs and rhs values of the comparison. For instance:

```cpp
// at test/test_tz_file.cpp:18
ASSERT_EQ( tz.abbr1.sv(), "KST" );
```

Produces the following output:

```
tz.abbr1.sv() == "KST"
└── (/opt/pages/vtz/etc/test/test_tzfile.cpp:18) "KST" == "KST"
```

If running a large number of checks in a loop, `ASSERT_EQ_QUIET` should be
preferred.
