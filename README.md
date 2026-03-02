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

## A Guided Tour of VTZ

### Locating a Timezone

vtz provides two main functions for getting a timezone. These are:

- `vtz::locate_zone( name )`, which finds a timezone based on its name in the
  IANA timezone database, and
- `vtz::current_zone()`, which returns the current timezone for the application.
  This is the system's timezone by default.

Once loaded, timezones are stored in an atomic global cache, so that subsequent
accesses are very fast.

```cpp
auto tz      = vtz::locate_zone( "America/New_York" ); // Eastern Time
auto tz_here = vtz::current_zone();
```

### Parsing and Formatting Times

Let's display the current time, both in America/New_York, and in the current
timezone.

Here, `tz->format()` converts a sys_time (measuring Unix time) to a local time
in the given zone. It uses the format specifiers for [`strftime`].

Here, `%c` displays the date and time (eg, `Fri Feb 27 12:09:46 2026`), while
`%Z` displays the current timezone abbreviation (eg, `EST` or `EDT` for Eastern
Time, depending on the time of year).

I'm in US Eastern Time, so for me, the output is the same in both cases.

If you just want to display UTC time, you can use `vtz::format()`, which formats
in terms of UTC.

[`strftime`]: https://man7.org/linux/man-pages/man3/strftime.3.html

```cpp
using sec = std::chrono::seconds;
using std::chrono::floor;
using std::chrono::system_clock;

auto now = floor<sec>( system_clock::now() );

fmt::println( "Current Time:\n"
              "  {}\n"
              "  {}\n"
              "  {}     (UTC Timestamp)\n",
    tz->format( "%c %Z", now ),
    tz_here->format( "%c %Z", now ),
    vtz::format( "%FT%TZ", now ) );
```

<details>
<summary>output</summary>

```
Current Time:
  Sun Mar  1 23:47:13 2026 EST
  Sun Mar  1 23:47:13 2026 EST
  2026-03-02T04:47:13Z     (UTC Timestamp)
```

</details>
<br>

`system_clock::now()` usually returns time in milliseconds or microseconds
(depending on the system), se we used `std::chrono::floor()` to get a time in
seconds in the above snippet.

The format function will display up to 9 digits, but if the input time is less
precise, it uses fewer digits.

[`std::chrono::high_resolution_clock`] can sometimes provide even more
precision, however it often uses [`steady_clock`] under the hood, which does not
necessarily return Unix Time.

For this purpose, you should use [`clock_cast`] to convert it to a `sys_time`.

[`clock_cast`]: https://en.cppreference.com/w/cpp/chrono/clock_cast.html
[`steady_clock`]: https://en.cppreference.com/w/cpp/chrono/steady_clock.html
[`std::chrono::high_resolution_clock`]:
  https://en.cppreference.com/w/cpp/chrono/high_resolution_clock.html

```cpp
auto now_precise = system_clock::now();
fmt::println( "Current Time (full available precision):\n"
              "  {}\n"
              "  {}\n"
              "  {}     (UTC Timestamp)\n",
    tz->format( "%c %Z", now_precise ),
    tz_here->format( "%c %Z", now_precise ),
    vtz::format( "%FT%TZ", now_precise ) );
```

<details>
<summary>output</summary>

```
Current Time (full available precision):
  Sun Mar  1 23:47:13.675987 2026 EST
  Sun Mar  1 23:47:13.675987 2026 EST
  2026-03-02T04:47:13.675987Z     (UTC Timestamp)
```

</details>
<br>

#### Parsing Timestamps

Let's parse a timestamp. vtz aims to be able to round-trip any format specifiers
supported by `vtz::format()` and `tz->format()`.

```cpp
// `sys_seconds` is a type alias for `time_point<system_clock, seconds>`
using std::chrono::sys_seconds;

sys_seconds T
    = vtz::parse<sec>( "%a %b %e %R %Y", "Thu Feb 26 10:30 2026" );
```

Here,

- `%a` represents a day of the week name
- `%b` represents the month name
- `%e` is the day of the month, 1-31, permitting a leading space
- `%R` is shorthand for `%H:%M` (`<hours>:<minutes>`) on a 24-hour clock
- `%Y` is the year

If you wanted second precision, you could use `%T` instead of `%R`, and then
this would be equivalent to `%c`:

```cpp
sys_seconds T2 = vtz::parse<sec>( "%c", "Thu Feb 26 10:30:00 2026" );
assert( T == T2 );
```

`vtz::parse` also accepts fractions of a second for any format specifiers which
handle seconds:

```cpp
using micros = std::chrono::microseconds;
auto T_precise
    = vtz::parse<micros>( "%c", "Thu Feb 26 10:30:00.192873 2026" );
fmt::println(
    "Parsed timestamp: {}\n", vtz::format( "%FT%TZ", T_precise ) );
assert( T_precise == T + micros( 192873 ) );
```

<details>
<summary>output</summary>

```
Parsed timestamp: 2026-02-26T10:30:00.192873Z
```

</details>
<br>

See the docs for [`strftime`] for a full list of format specifiers.

Specifiers `%a` and `%b` also accept full names instead of abbreviations. Eg,
'Thursday February 26 ...' would also be accepted here.

For performance, vtz handles locale-specific format specifiers in the C locale.
If you need to use an alternative locale, I recommend falling back to the
standard library.

Our input time is 10:30 UTC time. Because America/New_York is within Standard
Time, this corresponds to 5:30 am on the Eastern Seaboard.

vtz formatting functions are standalone, and vtz does not require libfmt as a
dependency. tz->format() returns a std::string.

If you wanted to write to a buffer, you could instead use `tz->format_to()`
instead.

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

### UTC Offsets & Daylight Savings Time

We can use `tz->offset()` to get the offset from UTC at a particular time. This
function returns an offset in seconds. In the format string, `%z` represents an
ISO 8601 standard timezone specification - this is the offset expressed as
`±hh[mm]`

Because `T` does not fall during daylight savings time, the zone is 5 hours
behind UTC, so this prints `UTC offset at T: -18000s (UTC-05)`

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

Say, when does Daylight Savings Time start, anyways?

The answer depends on the timezone, and the rules for that timezone at the given
input time. And it _also_ determines the current time within the zone, because
for many time zones, the offset from UTC changes throughout the year. When a
given input time falls is therefore critical for determining the local time.

The rules for a timezone are also subject to change due to legislation, and this
dependence on social convention and governance is the reason the tz database
(and timezone libraries like vtz) exist at all - you want to be able to answer
this question reliably, for any time zone, based on a standardized and publicly
available source of truth.

Yeah, ok. So when does it start? And how far off are we from UTC time?

For a _specific_ time, in a _specific_ zone, this is easy to answer with
`time_zone::get_info()`. This will return the UTC offset, the abbreviation, and
the save (if during Daylight Savings Time).

It will also return the _range_ of time over which those settings can be
guaranteed to apply (absent changes by future legislation).

```cpp
vtz::sys_info info = tz->get_info( T );

string_view fmt_ = "%a %b %d %T %Y %Z";
fmt::println( "Offset, Abbreviation, and Save @ {} ({})\n"
              "- start:        {}\n"
              "- end:          {}\n"
              "- start of dst: {}\n"
              "- offset:       {}\n"
              "- save:         {}\n"
              "- abbrev:       {}\n",
    tz->format( fmt_, T ),
    tz->name(),
    tz->format( fmt_, info.begin ),
    tz->format( fmt_, info.end - 1s ),
    tz->format( fmt_, info.end ),
    info.offset,
    info.save,
    info.abbrev );
```

<details>
<summary>output</summary>

```
Offset, Abbreviation, and Save @ Thu Feb 26 05:30:00 2026 EST (America/New_York)
- start:        Sun Nov 02 01:00:00 2025 EST
- end:          Sun Mar 08 01:59:59 2026 EST
- start of dst: Sun Mar 08 03:00:00 2026 EDT
- offset:       -18000s
- save:         0min
- abbrev:       EST
```

</details>
<br>

There we go. The current offset at T is -5h (-18000 seconds), the abbreviation
is `EST`, the save is 0 minutes (we're not in daylight savings time), and at
01:59:59 AM EST we move the clocks forwards an hour, and it becomes 03:00:00 AM
EDT.

That being said, in most cases you probably want a specific piece of
information, such as the current local time; the offset; or (if you already have
a local time) the UTC time.

```cpp
auto T_local = tz->to_local( T );
auto offset  = tz->offset( T );
fmt::println( "UTC Time:   {}\n"
              "Local Time: {}\n"
              "Offset:     {}\n",
    vtz::format( "%c", T ),
    vtz::format( "%c", T_local ),
    offset );
```

<details>
<summary>output</summary>

```
UTC Time:   Thu Feb 26 10:30:00 2026
Local Time: Thu Feb 26 05:30:00 2026
Offset:     -18000s
```

</details>
<br>

Updating `T` to be the start of daylight savings time, we see that daylight
savings time lasts from 3AM EDT on the second Sunday in March, to 1:59:59 AM EDT
on the first Sunday in November.

If we wanted, we could repeat this process of `info = tz->get_info( info.end )`
to enumerate all future switches between daylight time and standard time (at
least until `T` becomes too large to fit in a 64-bit integer).

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

### Converting Back to UTC, and ambiguous local times

Converting a local time back to a sys time typically matches the input sys_time,
however local times are ambiguous whenever the clocks fall back. In zones where
this happens, this typically occurs for 1 hour, once a year.

Take a look at when daylight savings time ends in the above example. For the US,
this is currently the first Sunday in November, at what _would_ be 2AM EDT.

When 1:59:59AM EDT ends on that date, clocks fall back to 1AM EST, and times
between then are ambiguous: 1:30AM could refer to _either_ EDT or EST.

When converting to UTC time, this disambiguation should be made with
`vtz::choose` - `choose::earliest` returns the earliest system time, and
`choose::latest` returns the latest system time.

(Most of the time these give the exact same result, but the difference must be
acknowledged)

```cpp
auto T_ambig = vtz::parse_local<sec>( "%c", "Sun Nov 01 01:30:00 2026" );
```

Here, there are two possible offsets: UTC-04 (EDT), and UTC-05 (EST) These
correspond to 05:30 UTC and 06:30 UTC, respectively.

```cpp
using enum vtz::choose;
auto T_early = tz->to_sys( T_ambig, earliest );
auto T_late  = tz->to_sys( T_ambig, latest );
fmt::println( "Input:         {}\n"
              "earliest time: {}\n"
              "latest time:   {}\n",
    vtz::format( "%F %T", T_ambig ),
    vtz::format( "%F %T %Z", T_early ),
    vtz::format( "%F %T %Z", T_late ) );
```

<details>
<summary>output</summary>

```
Input:         2026-11-01 01:30:00
earliest time: 2026-11-01 05:30:00 UTC
latest time:   2026-11-01 06:30:00 UTC
```

</details>
<br>

Both of these map to the same local time, but the zone abbreviation and offset
are different:

```cpp
assert( tz->to_local( T_early ) == tz->to_local( T_late ) );
assert( tz->offset( T_early ) != tz->offset( T_late ) );
assert( tz->abbrev( T_early ) != tz->abbrev( T_late ) );

fmt::println( "T_early: {}\n"
              "T_late:  {}\n",
    tz->format( "%c %Z", T_early ),
    tz->format( "%c %Z", T_late ) );
```

<details>
<summary>output</summary>

```
T_early: Sun Nov  1 01:30:00 2026 EDT
T_late:  Sun Nov  1 01:30:00 2026 EST
```

</details>
<br>

### Conclusion / Historical Errata

Fun fact! During World War II, the US entered into 'War Time', which was a
permanent, year-round daylight savings time which was intended to help conserve
fuel.

War Time lasted from February 9th, 1942, to August 14th, 1945, when Japan
surrendered. At this point, the timezone database records the US as
transitioning from war time to peace time. Both have the same offset, however we
have different abbreviations 'EWT' for Eastern War Time, and 'EPT' for Eastern
Peace Time.

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

If this example gives the wrong dates, your system's timezone files exclude
historical dates. To get the correct dates, you can set `VTZ_TZDATA_PATH` to
point vtz to the location of the timezone database.

It's downloaded as part of a standard build, and should be in
`build/data/tzdata`, but you can place it wherever is convenient on your system.

```sh
env VTZ_TZDATA_PATH=build/data/tzdata build/etc/examples/hello_vtz
```

Japan communicated its surrender to the US on August 15th, 1945. At that time,
it was still August 14th in the US, and Truman announced the surrender in a
public broadcast at 7pm Eastern Time.

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

Eastern Peace Time lasted from August 14th until September 30th, when Eastern
Standard Time resumed as per the daylight savings time rules in place at the
time.

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

