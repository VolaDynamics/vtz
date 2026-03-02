# vtz tldr

## Time Zone Lookup

Look up a timezone by IANA name, or get the system's current zone. Timezones are
cached globally after first lookup.

```cpp
auto ny   = vtz::locate_zone( "America/New_York" );
auto here = vtz::current_zone();
```

## Parsing Timestamps

Parse a UTC timestamp. Format specifiers follow strftime conventions. `%F` =
`%Y-%m-%d`, `%T` = `%H:%M:%S`.

```cpp
auto t = vtz::parse<sec>( "%F %T", "2026-02-27 15:30:00" );
```

## Formatting Timestamps

`vtz::format()` formats in UTC. `tz->format()` converts to local time.

```cpp
fmt::println( "UTC:      {}\n"
              "New York: {}\n"
              "Here:     {}\n",
    vtz::format( "%c %Z", t ),
    ny->format( "%c %Z", t ),
    here->format( "%c %Z", t ) );
```

<details>
<summary>Example Output</summary>

```
UTC:      Fri Feb 27 15:30:00 2026 UTC
New York: Fri Feb 27 10:30:00 2026 EST
Here:     Fri Feb 27 10:30:00 2026 EST
```

</details>

## Convert to Local Time

`to_local()` converts a UTC sys_time to a local time in the zone.

```cpp
auto local = ny->to_local( t );

fmt::println( "Local time in NY: {}\n", vtz::format( "%c", local ) );
```

<details>
<summary>Example Output</summary>

```
Local time in NY: Fri Feb 27 10:30:00 2026
```

</details>

## Covert to UTC

`to_sys()` converts a local time back to UTC. Local times can be ambiguous
during DST transitions, so a `vtz::choose` policy is required.

```cpp
auto t_back = ny->to_sys( local, vtz::choose::earliest );

fmt::println( "Round-trip: {} -> {} -> {}\n",
    vtz::format( "%T %Z", t ),
    vtz::format( "%T", local ),
    vtz::format( "%T %Z", t_back ) );
```

<details>
<summary>Example Output</summary>

```
Round-trip: 15:30:00 UTC -> 10:30:00 -> 15:30:00 UTC
```

</details>

## Get Offset from UTC

`offset()` returns the UTC offset as a chrono duration. Adding the offset to a
sys_time gives the same wall-clock time as `to_local()`.

```cpp
auto off = ny->offset( t );

fmt::println( "Offset:   {}\n"
              "t + off:  {}\n"
              "to_local: {}\n",
    off,
    vtz::format( "%c", t + off ),
    vtz::format( "%c", local ) );
```

<details>
<summary>Example Output</summary>

```
Offset:   -18000s
t + off:  Fri Feb 27 10:30:00 2026
to_local: Fri Feb 27 10:30:00 2026
```

</details>

## Get info about current period

`get_info()` returns the offset, abbreviation, DST save, and the time range over
which they apply.

```cpp
auto info = ny->get_info( t );

fmt::println( "Zone info @ {}:\n"
              "  abbrev: {}\n"
              "  offset: {}\n"
              "  save:   {}\n"
              "  valid:  {} to {}\n",
    ny->format( "%c %Z", t ),
    info.abbrev,
    info.offset,
    info.save,
    ny->format( "%c %Z", info.begin ),
    ny->format( "%c %Z", info.end - 1s ) );
```

<details>
<summary>Example Output</summary>

```
Zone info @ Fri Feb 27 10:30:00 2026 EST:
  abbrev: EST
  offset: -18000s
  save:   0min
  valid:  Sun Nov  2 01:00:00 2025 EST to Sun Mar  8 01:59:59 2026 EST
```

</details>

---

Code for this example can be found at `examples/src/vtz_tldr.cpp`.

This example can be run as:

```sh
env VTZ_TZDATA_PATH=build/data/tzdata build/examples/vtz_tldr
```
