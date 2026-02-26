# Hello VTZ

This document serves as a guided tour of vtz. For a full understanding of the 
library API, please refer to `vtz/tz.h`, which does it's best to provide 
thorough documentation of the API and it's constraints.

---

vtz provides two main functions for getting a timezone. These are:

- `vtz::locate_zone( name )`, which finds a timezone based on its
  name in the IANA timezone database, and
- `vtz::current_zone()`, which returns the current timezone for the
  application. This is the system's timezone by default.

Once loaded, timezones are stored in an atomic global cache, so that
subsequent accesses are very fast.

```cpp
auto tz = vtz::locate_zone( "America/New_York" ); // Eastern Time
```

Parse a timestamp. Uses format specifiers for strftime / strptime.

Here,
- `%a` represents a day of the week name
- `%b` represents the month name
- `%d` is the day of the month, 1-31
- `%R` is shorthand for `%H:%M` (`<hours>:<minutes>`) on a 24-hour clock
- `%Y` is the year

If you wanted second precision, you could use `%T` instead of `%R`.

See the docs for [`strftime`] for a full list of format specifiers.

Specifiers `%a` and `%b` also accept full names instead of abbreviations.
Eg, 'Thursday February 26 ...' would also be accepted here.

For performance, vtz handles locale-specific format specifiers in the C
locale. If you need to use an alternative locale, I recommend falling
back to the standard library.

[`strftime`]: https://man7.org/linux/man-pages/man3/strftime.3.html

```cpp
// `sys_seconds` is a type alias for `time_point<system_clock, seconds>`
using std::chrono::sys_seconds;
using sec = std::chrono::seconds;

sys_seconds T
    = vtz::parse<sec>( "%a %b %d %R %Y", "Thu Feb 26 10:30 2026" );
```

Our input time is 10:30 UTC time. Because America/New_York is within
Standard Time, this corresponds to 5:30 am on the Eastern Seaboard.

vtz formatting functions are standalone, and vtz does not require libfmt
as a dependency. tz->format() returns a std::string.

If you wanted to write to a buffer, you could instead use `tz->format_to`

```cpp
fmt::println( "UTC time vs Local Time (America/New_York):\n"
              "  {} <-> {}\n",
    vtz::format( "%T %Z", T ),
    tz->format( "%T %Z", T ) );
```

<details>
<summary>output</summary>

```
UTC time vs Local Time (America/New_York):
  10:30:00 UTC <-> 05:30:00 EST
```

</details>
<br>

Let's display the time in a couple other timezones:

```cpp
auto cet = vtz::locate_zone( "Europe/Amsterdam" );
auto pst = vtz::locate_zone( "America/Los_Angeles" );
auto mst = vtz::locate_zone( "America/Denver" );
fmt::println( "Time in other zones:\n"
              "  Amsterdam:      {}\n"
              "  Denver:         {}\n"
              "  Los Angeles/SF: {} (Google Imperial Time)\n",
    cet->format( "%T %Z", T ),
    mst->format( "%T %Z", T ),
    pst->format( "%T %Z", T ) );
```

<details>
<summary>output</summary>

```
Time in other zones:
  Amsterdam:      11:30:00 CET
  Denver:         03:30:00 MST
  Los Angeles/SF: 02:30:00 PST (Google Imperial Time)
```

</details>
<br>

We can use `tz->offset()` to get the offset from UTC at a particular
time. This function returns an offset in seconds. In the format string,
`%z` represents an ISO 8601 standard timezone specification.

During Standard Time, America/New_York is 5 hours behind UTC, so this
prints `UTC offset at T: -18000s (UTC-05)`

```cpp
fmt::println( "UTC offset at T: {} ({})\n",
    tz->offset( T ),
    tz->format( "UTC%z", T ) );
```

<details>
<summary>output</summary>

```
UTC offset at T: -18000s (UTC-05)
```

</details>
<br>

Our input time, T, falls during Eastern Standard Time (AKA, not Daylight
Savings Time).

When is Eastern Standard Time?

The answer to this question depends on the timezone, and the rules for
that timezone. Different timezones have different rules, and even for a
specific zone, legislation can change when daylight savings time starts
and ends; the offset(s) and abbreviation(s) for the zone; and even if
daylight savings time occurs at all.

But for a *specific* time, this is easy to answer with
`time_zone::get_info()`. This will return the UTC offset, the
abbreviation, and the save (if during Daylight Savings Time).

It will also return the _range_ of time over which those settings can be
guaranteed to apply (absent changes by future legislation).

```cpp
vtz::sys_info info = tz->get_info( T );

string_view fmt_ = "%a %b %d %T %Y %Z";
fmt::println( "Offset, Abbreviation, and Save @ {} ({})\n"
              "- start:  {}\n"
              "- end:    {}\n"
              "- offset: {}\n"
              "- save:   {}\n"
              "- abbrev: {}\n",
    tz->format( fmt_, T ),
    tz->name(),
    tz->format( fmt_, info.begin ),
    tz->format( fmt_, info.end - 1s ),
    info.offset,
    info.save,
    info.abbrev );
```

<details>
<summary>output</summary>

```
Offset, Abbreviation, and Save @ Thu Feb 26 05:30:00 2026 EST (America/New_York)
- start:  Sun Nov 02 01:00:00 2025 EST
- end:    Sun Mar 08 01:59:59 2026 EST
- offset: -18000s
- save:   0min
- abbrev: EST
```

</details>
<br>

Under the current US rules, Standard Time lasts until 2AM on the second
Sunday in March. At that point, we move the clocks forward an hour - 2AM
is skipped entirely, and we jump straight to 3AM local time.

The offset changes to UTC-04, the save changes to 1h (60 minutes), and
the abbreviation changes from 'EST' to 'EDT'.

```cpp
// Get next time period following Standard Time
T    = info.end;
info = tz->get_info( T );

// Put this in a lambda for reuse
auto print_info = [&] {
    fmt::println( "Offset, Abbreviation, and Save @ {} ({})\n"
                  "- start:  {}\n"
                  "- end:    {}\n"
                  "- offset: {}\n"
                  "- save:   {}\n"
                  "- abbrev: {}\n",
        tz->format( fmt_, T ),
        tz->name(),
        tz->format( fmt_, info.begin ),
        tz->format( fmt_, info.end - 1s ),
        info.offset,
        info.save,
        info.abbrev );
};
print_info();
```

<details>
<summary>output</summary>

```
Offset, Abbreviation, and Save @ Sun Mar 08 03:00:00 2026 EDT (America/New_York)
- start:  Sun Mar 08 03:00:00 2026 EDT
- end:    Sun Nov 01 01:59:59 2026 EDT
- offset: -14400s
- save:   60min
- abbrev: EDT
```

</details>
<br>

Fun fact! During World War II, the US entered into 'War Time', which was
a permanent, year-round daylight savings time which was intended to
help conserve fuel.

War Time lasted from February 9th, 1942, to August 14th, 1945, when Japan
surrendered. At this point, the timezone database records the US as
transitioning from war time to peace time. Both have the same offset,
however we have different abbreviations 'EWT' for Eastern War Time, and
'EPT' for Eastern Peace Time.

```cpp
// %F is shorthand for '%Y-%m-%d', YYYY-MM-DD
T    = vtz::parse<sec>( "%F", "1942-02-10" );
info = tz->get_info( T );

print_info();
```

<details>
<summary>output</summary>

```
Offset, Abbreviation, and Save @ Mon Feb 09 20:00:00 1942 EWT (America/New_York)
- start:  Mon Feb 09 03:00:00 1942 EWT
- end:    Tue Aug 14 18:59:59 1945 EWT
- offset: -14400s
- save:   60min
- abbrev: EWT
```

</details>
<br>

If this example gives the wrong dates, your system's timezone files
exclude historical dates. To get the correct dates, you can set
`VTZ_TZDATA_PATH` to point vtz to the location of the timezone database.

It's downloaded as part of a standard build, and should be in
`build/data/tzdata`, but you can place it wherever is convenient on your
system.

```sh
env VTZ_TZDATA_PATH=build/data/tzdata build/etc/examples/hello_vtz
```

Japan communicated its surrender to the US on August 15th, 1945. At that
time, it was still August 14th in the US, and Truman announced the
surrender in a public broadcast at 7pm Eastern Time.

```cpp
auto tokyo = vtz::locate_zone( "Asia/Tokyo" );
fmt::println( "Truman announces Japanese surrender:\n"
              "  japan: {}\n"
              "  us:    {}\n",
    tokyo->format( fmt_, info.end ),
    tz->format( fmt_, info.end ) );
```

<details>
<summary>output</summary>

```
Truman announces Japanese surrender:
  japan: Wed Aug 15 08:00:00 1945 JST
  us:    Tue Aug 14 19:00:00 1945 EPT
```

</details>
<br>

Eastern Peace Time lasted from August 14th until September 30th, when
Eastern Standard Time resumed as per the daylight savings time rules in
place at the time.

```cpp
T    = info.end; // set T to end of EWT;
info = tz->get_info( T );

print_info();
```

<details>
<summary>output</summary>

```
Offset, Abbreviation, and Save @ Tue Aug 14 19:00:00 1945 EPT (America/New_York)
- start:  Tue Aug 14 19:00:00 1945 EPT
- end:    Sun Sep 30 01:59:59 1945 EPT
- offset: -14400s
- save:   60min
- abbrev: EPT
```

</details>
<br>

---

Code for this example can be found at `etc/examples/src/vtz_a_guided_tour.cpp`.

This example can be run as:

```sh
env VTZ_TZDATA_PATH=build/data/tzdata build/etc/examples/vtz_a_guided_tour
```
