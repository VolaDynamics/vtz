#include <chrono>
#include <vtz/format.h>
#include <vtz/parse.h>
#include <vtz/tz.h>

#include <fmt/chrono.h>
#include <fmt/core.h>

int main() {
    using sec = std::chrono::seconds;
    using namespace std::chrono_literals;

    // ## Time Zone Lookup
    //
    // Look up a timezone by IANA name, or get the system's current zone.
    // Timezones are cached globally after first lookup.

    auto ny   = vtz::locate_zone( "America/New_York" );
    auto here = vtz::current_zone();

    // ## Parsing Timestamps
    //
    // Parse a UTC timestamp. Format specifiers follow strftime conventions.
    // `%F` = `%Y-%m-%d`, `%T` = `%H:%M:%S`.

    auto t = vtz::parse<sec>( "%F %T", "2026-02-27 15:30:00" );

    // ## Formatting Timestamps
    //
    // `vtz::format()` formats in UTC. `tz->format()` converts to local time.

    fmt::println( "UTC:      {}\n"
                  "New York: {}\n"
                  "Here:     {}\n",
        vtz::format( "%c %Z", t ),
        ny->format( "%c %Z", t ),
        here->format( "%c %Z", t ) );

    // ## Convert to Local Time
    //
    // `to_local()` converts a UTC sys_time to a local time in the zone.

    auto local = ny->to_local( t );

    fmt::println( "Local time in NY: {}\n", vtz::format( "%c", local ) );

    // ## Covert to UTC
    //
    // `to_sys()` converts a local time back to UTC. Local times can be
    // ambiguous during DST transitions, so a `vtz::choose` policy is required.

    auto t_back = ny->to_sys( local, vtz::choose::earliest );

    fmt::println( "Round-trip: {} -> {} -> {}\n",
        vtz::format( "%T %Z", t ),
        vtz::format( "%T", local ),
        vtz::format( "%T %Z", t_back ) );

    // ## Get Offset from UTC
    //
    // `offset()` returns the UTC offset as a chrono duration. Adding the
    // offset to a sys_time gives the same wall-clock time as `to_local()`.

    auto off = ny->offset( t );

    fmt::println( "Offset:   {}\n"
                  "t + off:  {}\n"
                  "to_local: {}\n",
        off,
        vtz::format( "%c", t + off ),
        vtz::format( "%c", local ) );

    // ## Get info about current period
    //
    // `get_info()` returns the offset, abbreviation, DST save, and the time
    // range over which they apply.

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
}
