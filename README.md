# vtz

vtz intends to be the fastest timezone library in the world. It provides
optimally performant timezone conversions, while also being convenient and easy
to use, with minimal configuration required.

## Getting Started

vtz derives it's API from the [`std::chrono::time_zone`] API, with some
additional functions added for convenience.

vtz also provides [`vtz::format(...)`][format.h] and
[`vtz::format_to(..., buffer)`][format.h] for formatting, and
[`vtz::parse()`][parse.h] and for parsing.

For a quick introduction with example output, take a look at the [vtz
TLDR][tldr]. Or, for an extended introduction, or read [vtz: A Guided
Tour][tour] (also in `docs/`).

Source code for these can be found in `examples/src`.

[format.h]: include/api/vtz/format.h
[parse.h]: include/api/vtz/parse.h
[docs/vtz_tldr.md]: docs/vtz_tldr.md
[tldr]: docs/vtz_tldr.md
[tour]: docs/vtz_a_guided_tour.md
[`std::chrono::time_zone`]:
  https://en.cppreference.com/w/cpp/chrono/time_zone.html

### Using vtz in your project

If you use CMake, the fastest way to use `vtz` in your own project is with
[CPM]:

```cmake
CPMAddPackage("gh:voladynamics/vtz@1.0.0")

target_link_libraries(your_app PRIVATE vtz::vtz)
```

This will automatically download and configure `vtz` as part of your project's
configuration.

<details>
<summary>Tip: When Using CPM, enable offline builds with CPM_SOURCE_CACHE</summary>

To avoid repeat downloads, and to enable offline builds, I recommend configuring
the `CPM_SOURCE_CACHE` env variable (or set it in your CMakeLists.txt):

```sh
export CPM_SOURCE_CACHE=$HOME/.local/.cpm
```

This should be placed in your `.bashrc` or `.zshrc`.

</details>

If you install `vtz`, you can also use it with [`find_package`]:

```cmake
find_package(vtz)

target_link_libraries(your_app PRIVATE vtz::vtz)
```

[`find_package`]: https://cmake.org/cmake/help/latest/command/find_package.html
[CPM]: https://github.com/cpm-cmake/CPM.cmake

## Building, Running, and Installing

To build everything (the library, tests, and benchmarks), use:

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
```

To build _only_ the library, add `-DVTZ_ONLY=ON`:

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DVTZ_ONLY=ON && cmake --build build
```

### Dependencies

You do not need to install any packages on your system in order to build vtz.
The library itself has only one dependency, [`ankerl::unordered_dense`][ankerl].
This is a very fast hash map implementation which is used as the KV store for
`vtz::locate_zone( tzname )`.

[ankerl]: https://github.com/martinus/unordered_dense

Because `ankerl::unordered_dense` is header-only, a copy of the headers have
been provisioned at `etc/3rd/ankerl`.

If `vtz` is used as a sub-component of another library (eg, with
`add_subdirectory`, or when included with [CPM]), vtz will not build tests; it
will not build benchmarks; and it will not download anything: the build becomes
hermetic.

If you _do_ want to build tests and benchmarks, vtz will use [CPM] to download
test- and benchmark-related dependencies for ease of use.

### Installation

Install with:

```sh
cmake --install build --prefix [path]
```

If installed on a directory searched by CMake, `vtz` can be discovered with
`find_package`.

## Benchmarks

You can run benchmarks with `build/bench_vtz`:

```sh
build/bench_vtz
```

vtz uses google benchmark, and any of Google Benchmark`s cli arguments can be
used here.

<details>
<summary>Benchmark Arguments</summary>

```
$ build/bench_vtz --help
benchmark [--benchmark_list_tests={true|false}]
          [--benchmark_filter=<regex>]
          [--benchmark_min_time=`<integer>x` OR `<float>s` ]
          [--benchmark_min_warmup_time=<min_warmup_time>]
          [--benchmark_repetitions=<num_repetitions>]
          [--benchmark_dry_run={true|false}]
          [--benchmark_enable_random_interleaving={true|false}]
          [--benchmark_report_aggregates_only={true|false}]
          [--benchmark_display_aggregates_only={true|false}]
          [--benchmark_format=<console|json|csv>]
          [--benchmark_out=<filename>]
          [--benchmark_out_format=<json|console|csv>]
          [--benchmark_color={auto|true|false}]
          [--benchmark_counters_tabular={true|false}]
          [--benchmark_context=<key>=<value>,...]
          [--benchmark_time_unit={ns|us|ms|s}]
          [--v=<verbosity>]
```

</details>

## Acknowledgements

I would like to acknowledge Arthur David Olson, Paul Eggert, and the many
volunteers and contributors to the [tz database][tz-database-wiki]. Their
efforts to standardize and document the chaos that is global timekeeping has
proven invaluable for allowing innumerable other projects to properly handle
timezones.

[_Theory and pragmatics of the `tz` code and data_][tz-theory] is itself an
excellent resource for understanding how timezones work, and [_How to Read the
tz Database Source Files_][tz-how-to] was my primary reference when implementing
vtz's parsing logic.

[`ankerl::unordered_dense`][ankerl] is an excellent and performant
implementation of a flat hash-map implementation, and the
[benchmarks][ankerl-bench] carried out by Martin Leitner-Ankerl are solid and
well-documented.

[CPM: the CMake Package Manager][CPM] is invaluable for fast prototyping, and
for quickly adding dependencies to a project.

And finally, the [Hinnant Date Library][hinnant-date] exists as the source of
inspiration for the timezone library incorporated into the C++ standard, and
despite the performance issues it exposes with regard to timezone conversions in
particular, it has proven reliable. (The `date` side of things is otherwise very
performant.)

Hinnant's work, [_`chrono`-Compatible Low-Level Date Algorithms_][ll-date], is
also a fantastic read, and the date algorithms described there form the basis of
many of the date algorithms which vtz uses for formatting, parsing, and for
ingestion of the tz database itself.

The work goes out of its way to explain and document the algorithms used, and
these algorithms allow conversions between Unix Time and human-readable dates to
be performed _without_ lookup tables.

[tz-how-to]: https://data.iana.org/time-zones/tzdb-2022g/tz-how-to.html
[tz-theory]: https://data.iana.org/time-zones/tzdb-2022b/theory.html
[tz-database]: https://www.iana.org/time-zones
[tz-database-wiki]: https://en.wikipedia.org/wiki/Tz_database
[ankerl-bench]: https://martin.ankerl.com/2022/08/27/hashmap-bench-01/
[hinnant-date]: https://github.com/HowardHinnant/date
[ll-date]: https://howardhinnant.github.io/date_algorithms.html
