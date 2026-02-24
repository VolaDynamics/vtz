# vtz

vtz intends to be the fastest timezone library in the world. It provides
optimally performant timezone conversions, while also being convenient and easy
to use, with minimal configuration required.

If you want to better understand timezones, I recommend reading [_Theory and
pragmatics of the `tz` code and data_][tz-theory], which discusses the theory
behind the [IANA Time Zone Database][tz-database-wiki]. From hereon we will
refer to it simply as the tz database.

[tz-theory]: https://data.iana.org/time-zones/tzdb-2022b/theory.html
[tz-database]: https://www.iana.org/time-zones
[tz-database-wiki]: https://en.wikipedia.org/wiki/Tz_database

## Building the library

You can build the library with:

```sh
cmake -B build -G Ninja
cmake --build build
```

Then, install with `cmake --install`:

```sh
cmake --install build --prefix /path/to/install
```

By default, this will build the library, tests, and benchmarks. To build _only_
the library, use `-DVTZ_ONLY=1`:

```sh
cmake -B build -G Ninja -DVTZ_ONLY=1
cmake --build build
```

## Using the library in your project

The fastest and most convinient way to use `vtz` as a dependency in your project
is with [cpm][cpm]:

```cmake
CPMAddPackage("gh:voladynamics/vtz@1.0.0")
```

If `vtz` is installed on the system, it can also be located with
[`find_package`][find-package]:

```cmake
find_package(vtz REQUIRED)
```

With either method, you can link against `vtz` using
`target_link_libraries(... vtz::vtz)`:

```cmake
# use VTZ as private dependency of your application
target_link_libraries(my_app PRIVATE vtz::vtz)
```

[cpm]: https://github.com/cpm-cmake/CPM.cmake
[find-package]: https://cmake.org/cmake/help/latest/command/find_package.html

## API basics - Using vtz

vtz aims to provide an interface which is familiar to people who have used
either the [Hinnant Date Library][hinnant-date], or who have used the [timezone
library added in C++20][tz-cpp20].

[hinnant-date]: https://github.com/HowardHinnant/date
[tz-cpp20]: https://en.cppreference.com/w/cpp/chrono.html#Time_zone

## Looking up a Timezone

You can look up a timezone with `vtz::locate_zone( tzname )`, or you can get the
current timezone with `vtz::current_zone()`:

```cpp
// Get the current zone
auto tz    = vtz::current_zone();
// Get the timezone representing US Eastern Time
auto tz_ny = vtz::locate_zone( "America/New_York" );
```

Because there are some 340 timezones in the tz database, time zones are loaded
lazily on-request. There is a one-time upfront penalty the first time that a
zone is loaded, however, subsequent calls to `locate_zone` with a given zone
should be on the order of 7-10 ns.



## Getting the current offset from UTC

The most basic question you can ask is probably, "What is the offset from UTC,
for a given time, in a given zone?"

You can get the answer to this question for a given zone with
`time_zone::offset( T )`, which takes a time as input, and returns the offset at
that time:

```cpp
// Get (and display) the current offset from UTC, Eastern Time
std::chrono::seconds offset = tz_ny->offset( t_utc );
fmt::println( "current utc offset: {}s ({}h)",
    offset.count(),
    offset.count() / 3600 );
```

Example output, assuming that daylight savings time is not in effect at `t_utc`:

```
current utc offset: -18000s (-5h)
```

This offset is a measure of the number of seconds you add to UTC time, to get
the current local time. For example, if it's 7pm UTC time, and the offset in the
current zone at the current time is `-5h`, then it's 2pm local time.

If you prefer working with a raw number of seconds, you can avoid dealing with
`std::chrono` using the `_s` overloads, which take seconds as input, and return
seconds as output:

```cpp
// Get seconds since the unix epoch
int64_t utc_seconds    = std::time( nullptr );

// Get the current offset, in seconds
int64_t offset_seconds = tz_ny->offset_s( utc_seconds );

fmt::println( "current utc offset: {}s ({}h)",
    offset_seconds,
    offset_seconds / 3600 );
```

<details>
<summary>Why is the offset given in seconds?</summary>

The offset is given in seconds because real timezones may have an offset that is
not a whole number of hours. For example, Nepal (`Asia/Kathmandu`) _currently_
has an offset of `+5:45`!

Additionally, the IANA timezone database uses _mean solar time_ as the offset
for dates and times prior to the standardization of time within a given zone.
For instance, before railway time was introduced in 1883, `America/New_York` had
an offset of `-4:56:02`.

</details>

## Converting from UTC to local

You can use `time_zone::to_local` to convert a system time to a local time
within a zone:

```cpp
auto t_utc   = std::chrono::system_clock::now();
auto t_local = tz_ny->to_local( t_utc );
```

```cpp

// Obtain it as a local timestamp.
int64_t local_seconds = tz->to_local_s( utc_seconds );
```

## Getting the offset within a zone
