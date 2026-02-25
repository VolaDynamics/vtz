#include "test_helper.h"

#include <gtest/gtest.h>
#include <vtz/format.h>
#include <vtz/parse.h>

using namespace vtz;


TEST( vtz_parse, sanity ) {
    // %F — ISO date
    EXPECT_EQ(
        parse<seconds>( "%F", "2024-01-01" ), _get_time( 2024, 1, 1 ) );
    EXPECT_EQ(
        parse<seconds>( "%F", "2024-02-29" ), _get_time( 2024, 2, 29 ) );
    EXPECT_EQ(
        parse<seconds>( "%F", "2024-12-31" ), _get_time( 2024, 12, 31 ) );
    EXPECT_EQ(
        parse<seconds>( "%F", "1970-01-01" ), _get_time( 1970, 1, 1 ) );
    EXPECT_EQ(
        parse<seconds>( "%F", "2000-01-01" ), _get_time( 2000, 1, 1 ) );
    EXPECT_EQ( parse<seconds>( "%F", "0001-01-01" ), _get_time( 1, 1, 1 ) );

    // %F %T — ISO date + time
    EXPECT_EQ( parse<seconds>( "%F %T", "2024-02-29 14:30:00" ),
        _get_time( 2024, 2, 29, 14, 30, 0 ) );
    EXPECT_EQ( parse<seconds>( "%F %T", "2024-12-31 23:59:59" ),
        _get_time( 2024, 12, 31, 23, 59, 59 ) );
    EXPECT_EQ( parse<seconds>( "%F %T", "1970-01-01 00:00:00" ),
        _get_time( 1970, 1, 1, 0, 0, 0 ) );
    EXPECT_EQ( parse<seconds>( "%F %T", "2025-02-19 21:20:00" ),
        _get_time( 2025, 2, 19, 21, 20, 0 ) );

    // %Y-%m-%d %H:%M:%S — same as above, spelled out
    EXPECT_EQ( parse<seconds>( "%Y-%m-%d %H:%M:%S", "2024-07-04 15:30:45" ),
        _get_time( 2024, 7, 4, 15, 30, 45 ) );

    // %Y%m%d — compact date
    EXPECT_EQ(
        parse<seconds>( "%Y%m%d", "20240101" ), _get_time( 2024, 1, 1 ) );
    EXPECT_EQ(
        parse<seconds>( "%Y%m%d", "20241231" ), _get_time( 2024, 12, 31 ) );

    // %d/%m/%Y — day/month/year
    EXPECT_EQ( parse<seconds>( "%d/%m/%Y", "29/02/2024" ),
        _get_time( 2024, 2, 29 ) );
    EXPECT_EQ( parse<seconds>( "%d/%m/%Y", "25/12/2024" ),
        _get_time( 2024, 12, 25 ) );

    // %R — HH:MM
    EXPECT_EQ( parse<seconds>( "%F %R", "2024-03-15 08:05" ),
        _get_time( 2024, 3, 15, 8, 5, 0 ) );
    EXPECT_EQ( parse<seconds>( "%F %R", "2024-03-15 23:59" ),
        _get_time( 2024, 3, 15, 23, 59, 0 ) );

    // %y — 2-digit year
    EXPECT_EQ(
        parse<seconds>( "%y-%m-%d", "24-06-15" ), _get_time( 2024, 6, 15 ) );
    EXPECT_EQ(
        parse<seconds>( "%y-%m-%d", "00-01-01" ), _get_time( 2000, 1, 1 ) );
    EXPECT_EQ(
        parse<seconds>( "%y-%m-%d", "69-01-01" ), _get_time( 1969, 1, 1 ) );
    EXPECT_EQ( parse<seconds>( "%y-%m-%d", "99-12-31" ),
        _get_time( 1999, 12, 31 ) );

    // %C%y — century + 2-digit year
    EXPECT_EQ( parse<seconds>( "%C%y-%m-%d", "2024-06-15" ),
        _get_time( 2024, 6, 15 ) );
    EXPECT_EQ( parse<seconds>( "%C%y-%m-%d", "1900-01-01" ),
        _get_time( 1900, 1, 1 ) );

    // %j — day of year (ordinal date)
    EXPECT_EQ(
        parse<seconds>( "%Y-%j", "2024-001" ), _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse<seconds>( "%Y-%j", "2024-060" ),
        _get_time( 2024, 2, 29 ) ); // 2024 leap: day 60 = Feb 29
    EXPECT_EQ(
        parse<seconds>( "%Y-%j", "2024-366" ), _get_time( 2024, 12, 31 ) );

    // %e — synonym of %d
    EXPECT_EQ( parse<seconds>( "%Y-%m-%e", "2024-01-01" ),
        _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse<seconds>( "%Y-%m-%e", "2024-01-15" ),
        _get_time( 2024, 1, 15 ) );

    // %w and %u — weekday parsing (consumed but not used for date resolution)
    EXPECT_EQ(
        parse<seconds>( "%w %F", "1 2024-01-01" ), _get_time( 2024, 1, 1 ) );
    EXPECT_EQ(
        parse<seconds>( "%u %F", "1 2024-01-01" ), _get_time( 2024, 1, 1 ) );

    // %Z — timezone abbreviation (consumed but not used for offset)
    EXPECT_EQ( parse<seconds>( "%F %T %Z", "2024-01-01 00:00:00 UTC" ),
        _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse<seconds>( "%F %T %Z", "2024-06-15 12:00:00 EST" ),
        _get_time( 2024, 6, 15, 12, 0, 0 ) );

    // %n and %t — whitespace matching
    EXPECT_EQ( parse<seconds>( "%F%n%T", "2024-01-01 12:30:00" ),
        _get_time( 2024, 1, 1, 12, 30, 0 ) );
    EXPECT_EQ( parse<seconds>( "%F%t%T", "2024-01-01\t12:30:00" ),
        _get_time( 2024, 1, 1, 12, 30, 0 ) );

    // %% — literal percent
    EXPECT_EQ( parse<seconds>( "%F %% %T", "2024-01-01 % 12:00:00" ),
        _get_time( 2024, 1, 1, 12, 0, 0 ) );

    // %I %p — 12-hour clock with AM/PM
    // 12:00 AM = 00:00
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 12:00:00 AM" ),
        _get_time( 2024, 1, 1, 0, 0, 0 ) );
    // 12:30 AM = 00:30
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 12:30:00 AM" ),
        _get_time( 2024, 1, 1, 0, 30, 0 ) );
    // 1:00 AM = 01:00
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 01:00:00 AM" ),
        _get_time( 2024, 1, 1, 1, 0, 0 ) );
    // 11:59 AM = 11:59
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 11:59:59 AM" ),
        _get_time( 2024, 1, 1, 11, 59, 59 ) );
    // 12:00 PM = 12:00
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 12:00:00 PM" ),
        _get_time( 2024, 1, 1, 12, 0, 0 ) );
    // 1:00 PM = 13:00
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 01:00:00 PM" ),
        _get_time( 2024, 1, 1, 13, 0, 0 ) );
    // 11:59 PM = 23:59
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 11:59:59 PM" ),
        _get_time( 2024, 1, 1, 23, 59, 59 ) );

    // %p is case-insensitive
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 03:00:00 am" ),
        _get_time( 2024, 1, 1, 3, 0, 0 ) );
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 03:00:00 pm" ),
        _get_time( 2024, 1, 1, 15, 0, 0 ) );
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 03:00:00 Am" ),
        _get_time( 2024, 1, 1, 3, 0, 0 ) );
    EXPECT_EQ( parse<seconds>( "%F %I:%M:%S %p", "2024-01-01 03:00:00 pM" ),
        _get_time( 2024, 1, 1, 15, 0, 0 ) );

    // %p before %I (order-independent)
    EXPECT_EQ( parse<seconds>( "%F %p %I:%M:%S", "2024-01-01 PM 02:30:00" ),
        _get_time( 2024, 1, 1, 14, 30, 0 ) );
    EXPECT_EQ( parse<seconds>( "%F %p %I:%M:%S", "2024-01-01 AM 12:00:00" ),
        _get_time( 2024, 1, 1, 0, 0, 0 ) );
}


TEST( vtz_parse, leading_space ) {
    // Specifiers that accept a leading space via parse_d2_allow_space:
    //   %d, %e, %m, %H  (and their compound forms %F, %R, %T)

    // %d — day of month with leading space
    EXPECT_EQ( parse<seconds>( "%Y-%m-%d", "2024-01- 1" ),
        _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse<seconds>( "%Y-%m-%d", "2024-12- 9" ),
        _get_time( 2024, 12, 9 ) );

    // %e — synonym of %d, same behavior
    EXPECT_EQ( parse<seconds>( "%Y-%m-%e", "2024-01- 1" ),
        _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse<seconds>( "%Y-%m-%e", "2024-06- 5" ),
        _get_time( 2024, 6, 5 ) );

    // %m — month with leading space
    EXPECT_EQ( parse<seconds>( "%Y-%m-%d", "2024- 1-01" ),
        _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse<seconds>( "%Y-%m-%d", "2024- 8-15" ),
        _get_time( 2024, 8, 15 ) );

    // %m and %d both with leading spaces
    EXPECT_EQ( parse<seconds>( "%Y-%m-%d", "2024- 8- 9" ),
        _get_time( 2024, 8, 9 ) );

    // %H — hour with leading space
    EXPECT_EQ( parse<seconds>( "%F %H:%M:%S", "2024-01-01  5:30:00" ),
        _get_time( 2024, 1, 1, 5, 30, 0 ) );
    EXPECT_EQ( parse<seconds>( "%F %H:%M:%S", "2024-01-01  0:00:00" ),
        _get_time( 2024, 1, 1, 0, 0, 0 ) );

    // %F — compound; month and day parts accept leading space
    EXPECT_EQ(
        parse<seconds>( "%F", "2024- 1- 1" ), _get_time( 2024, 1, 1 ) );
    EXPECT_EQ(
        parse<seconds>( "%F", "2024- 3-15" ), _get_time( 2024, 3, 15 ) );

    // %R — compound; hour part accepts leading space
    EXPECT_EQ( parse<seconds>( "%F %R", "2024-01-01  8:05" ),
        _get_time( 2024, 1, 1, 8, 5, 0 ) );

    // %T — compound; hour part accepts leading space
    EXPECT_EQ( parse<seconds>( "%F %T", "2024-01-01  8:05:30" ),
        _get_time( 2024, 1, 1, 8, 5, 30 ) );

    // Two-digit values still work (no regression)
    EXPECT_EQ( parse<seconds>( "%Y-%m-%d", "2024-12-31" ),
        _get_time( 2024, 12, 31 ) );
    EXPECT_EQ( parse<seconds>( "%F %T", "2024-01-01 23:59:59" ),
        _get_time( 2024, 1, 1, 23, 59, 59 ) );

    // Specifiers that should NOT accept a leading space: %y, %C, %M, %S

    // %y requires digits only (2-digit year is always zero-padded)
    EXPECT_THROW( parse<seconds>( "%y-%m-%d", " 4-01-01" ), std::exception );
    // %C requires digits only (century is always zero-padded)
    EXPECT_THROW(
        parse<seconds>( "%C%y-%m-%d", " 024-01-01" ), std::exception );
    // %M requires digits only (minutes are always zero-padded)
    EXPECT_THROW( parse<seconds>( "%F %H:%M:%S", "2024-01-01 12: 5:00" ),
        std::exception );
    // %S requires digits only (seconds are always zero-padded)
    EXPECT_THROW( parse<seconds>( "%F %H:%M:%S", "2024-01-01 12:05: 0" ),
        std::exception );
}


TEST( vtz_parse, round_trip ) {
    // Format a timestamp to a string, then parse it back. The parsed result
    // must equal the original.

    // clang-format off
    sys_seconds time_values[] = {
        _get_time(    0,  1,  1,  0,  0,  0 ), // Year 0
        _get_time( 1970,  1,  1,  0,  0,  0 ),
        _get_time( 1970,  1,  1, 23, 59, 59 ),
        _get_time( 2000,  1,  1 ),
        _get_time( 2009,  2, 13, 23, 31, 30 ),
        _get_time( 2024,  3,  1, 12,  0,  0 ),
        _get_time( 2024, 12, 31, 23, 59, 59 ),
        _get_time( 2025,  2, 19, 21, 20,  0 ),
        _get_time( 2025, 11,  6, 15, 24, 45 ),
        _get_time( 2038,  1, 19,  3, 14,  7 ),
        _get_time( 2038,  1, 19,  3, 14,  8 ),
        _get_time( 2100,  1,  1, 12,  0,  0 ),
        _get_time( 9999, 12, 31, 23, 59, 59 ),
    };
    // clang-format on

    // Formats that carry enough information for a lossless round-trip
    const char* round_trip_fmts[] = {
        "%F %T",                  // ISO date + ISO time
        "%Y-%m-%d %H:%M:%S",      // equivalent, spelled out
        "%Y%m%d %H:%M:%S",        // compact date
        "%d/%m/%Y %T",            // day/month/year
        "%Y-%j %T",               // ordinal date + time
        "%Y-%j %H:%M:%S",         // ordinal date + time, spelled out
        "%C%y%m%d %T",            // century + 2-digit year + compact date
        "%C%y-%m-%d %H:%M:%S",    // century + 2-digit year + date
        "%Y-%m-%d %R:%S",         // %R for hour:minute, then %S
        "%d %b %Y %T",            // abbreviated month name
        "%d %B %Y %T",            // full month name
        "%a %F %T",               // abbreviated weekday + ISO date/time
        "%A %F %T",               // full weekday + ISO date/time
        "%F %I:%M:%S %p",         // 12-hour clock with AM/PM
        "%a %b %d %H:%M:%S %Y",   // date format, eg
                                  // `Thu Jan 01 22:46:40 1970`
        "%a %b %d %I:%M:%S%P %Y", // date format (am/pm), eg
                                  // `Thu Jan 01 10:46:40pm 1970`
        "%a %b %e %H:%M:%S %Y",   // date format, eg
                                  // `Thu Jan  1 22:46:40 1970`
        "%a %b %e %I:%M:%S%P %Y", // date format (am/pm), eg
                                  // `Thu Jan  1 10:46:40pm 1970`
        "%a %b %e %l:%M:%S%P %Y", // date format (am/pm), eg
                                  // `Thu Jan  1  7:46:40pm 1970`

    };

    for( auto tp : time_values )
    {
        auto t = tp.time_since_epoch().count();
        for( auto fmt : round_trip_fmts )
        {
            auto formatted = format_s( fmt, t );
            auto parsed    = parse_s( fmt, formatted );
            ASSERT_EQ( parsed, t )
                << "round-trip failed for t=" << t << " fmt=\"" << fmt
                << "\" formatted=\"" << formatted << "\"";
        }
    }

    // Date-only round-trips
    sys_seconds date_values[] = {
        _get_time( 1970, 1, 1 ),
        _get_time( 1971, 1, 1 ),
        _get_time( 1997, 5, 19 ),
        _get_time( 2024, 1, 1 ),
        _get_time( 2026, 1, 1 ),
        _get_time( 2243, 10, 17 ),
    };

    const char* date_fmts[] = {
        "%F",         // ISO date
        "%Y-%m-%d",   // equivalent, spelled out
        "%Y%m%d",     // compact
        "%d/%m/%Y",   // day/month/year
        "%Y-%j",      // ordinal date
        "%C%y-%m-%d", // century + 2-digit year
        "%C%y%m%d",   // century + 2-digit year, compact
        "%d %b %Y",   // abbreviated month name
        "%d %B %Y",   // full month name
        "%a %F",      // abbreviated weekday + ISO date
        "%A %F",      // full weekday + ISO date
    };

    for( auto dp : date_values )
    {
        auto d
            = i32( std::chrono::floor<days>( dp.time_since_epoch() ).count() );
        for( auto fmt : date_fmts )
        {
            auto formatted = format_d( fmt, d );
            auto parsed    = parse_d( fmt, formatted );
            ASSERT_EQ( parsed, d )
                << "round-trip failed for d=" << d << " fmt=\"" << fmt
                << "\" formatted=\"" << formatted << "\"";
        }
    }

    // Round-trip every day of 2024 with %F
    constexpr unsigned days_in_month[]
        = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    for( unsigned m = 1; m <= 12; m++ )
        for( unsigned d = 1; d <= days_in_month[m - 1]; d++ )
        {
            auto t         = _get_time( 2024, m, d );
            auto t_sec     = t.time_since_epoch().count();
            auto formatted = format_s( "%F", t_sec );
            auto parsed    = parse_s( "%F", formatted );
            ASSERT_EQ( parsed, t_sec )
                << "round-trip failed for 2024-" << m << "-" << d
                << " formatted=\"" << formatted << "\"";
        }
}


TEST( vtz_parse, utc_offset ) {
    // %z — parse UTC offset and adjust to UTC

    // -0500 means local time is 5 hours behind UTC
    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-24 10:40:00 -0500" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );

    // -0500 means local time is 5 hours 30 minutes behind UTC
    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-24 10:10:00 -0530" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );

    // -05 is handled the same way as -0500
    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-24 10:40:00 -05" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );

    // When we change the position of %z (meaning more of the string remains),
    // everything works as expected (ensures parser validates minutes)
    EXPECT_EQ( parse<seconds>( "%z %F %T", "-0530 2025-02-24 10:10:00" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );
    EXPECT_EQ( parse<seconds>( "%z %F %T", "-05 2025-02-24 10:40:00" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );

    // +0000 — already UTC
    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-24 15:40:00 +00" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );

    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-24 15:40:00 +0000" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );

    // +0530 — India Standard Time
    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-24 21:10:00 +0530" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );

    // -0000 — equivalent to +0000
    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-24 15:40:00 -0000" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );

    // +1200 — far ahead of UTC
    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-25 03:40:00 +1200" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );

    // -1100 — far behind UTC
    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-24 04:40:00 -1100" ),
        _get_time( 2025, 2, 24, 15, 40, 0 ) );

    // Offset that crosses midnight backwards
    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-24 02:00:00 +0800" ),
        _get_time( 2025, 2, 23, 18, 0, 0 ) );

    // Offset that crosses midnight forwards
    EXPECT_EQ( parse<seconds>( "%F %T %z", "2025-02-24 23:00:00 -0500" ),
        _get_time( 2025, 2, 25, 4, 0, 0 ) );
}


TEST( vtz_parse, parse_generic_integral ) {
    using std::chrono::microseconds;
    using std::chrono::milliseconds;

    // 1. Days
    {
        EXPECT_EQ( parse<days>( "%F", "2024-03-15" ),
            std::chrono::floor<days>( _get_time( 2024, 3, 15 ) ) );
        EXPECT_EQ( parse<days>( "%F", "1970-01-01" ),
            std::chrono::floor<days>( _get_time( 1970, 1, 1 ) ) );
        EXPECT_EQ( parse<days>( "%F", "9999-12-31" ),
            std::chrono::floor<days>( _get_time( 9999, 12, 31 ) ) );
    }

    // 2. Hours
    {
        EXPECT_EQ( parse<hours>( "%F %T", "2024-03-15 14:00:00" ),
            std::chrono::floor<hours>( _get_time( 2024, 3, 15, 14, 0, 0 ) ) );
        // 14:30:45 floors to hour 14
        EXPECT_EQ( parse<hours>( "%F %T", "2024-03-15 14:30:45" ),
            std::chrono::floor<hours>( _get_time( 2024, 3, 15, 14, 30, 45 ) ) );
    }

    // 3. Minutes
    {
        EXPECT_EQ( parse<minutes>( "%F %T", "2024-03-15 14:30:00" ),
            std::chrono::floor<minutes>(
                _get_time( 2024, 3, 15, 14, 30, 0 ) ) );
        // 14:30:45 floors to 14:30
        EXPECT_EQ( parse<minutes>( "%F %T", "2024-03-15 14:30:45" ),
            std::chrono::floor<minutes>(
                _get_time( 2024, 3, 15, 14, 30, 45 ) ) );
    }

    // 4. Seconds — including years beyond 2292
    {
        EXPECT_EQ( parse<seconds>( "%F %T", "2024-03-15 14:30:45" ),
            _get_time( 2024, 3, 15, 14, 30, 45 ) );
        EXPECT_EQ( parse<seconds>( "%F %T", "2923-06-15 10:30:00" ),
            _get_time( 2923, 6, 15, 10, 30, 0 ) );
        EXPECT_EQ( parse<seconds>( "%F %T", "9999-12-31 23:59:59" ),
            _get_time( 9999, 12, 31, 23, 59, 59 ) );
    }

    // 5. Milliseconds — including years beyond 2292
    {
        using ms = milliseconds;
        EXPECT_EQ( parse<ms>( "%F %T", "2024-03-15 14:30:45.123" ),
            _get_time( 2024, 3, 15, 14, 30, 45 ) + ms( 123 ) );
        EXPECT_EQ( parse<ms>( "%F %T", "2024-03-15 14:30:45" ),
            _get_time( 2024, 3, 15, 14, 30, 45 ) + ms( 0 ) );
        EXPECT_EQ( parse<ms>( "%F %T", "2923-06-15 10:30:00.500" ),
            _get_time( 2923, 6, 15, 10, 30, 0 ) + ms( 500 ) );
        EXPECT_EQ( parse<ms>( "%F %T", "9999-12-31 23:59:59.999" ),
            _get_time( 9999, 12, 31, 23, 59, 59 ) + ms( 999 ) );
    }

    // 6. Microseconds — including years beyond 2292
    {
        using us = microseconds;
        EXPECT_EQ( parse<us>( "%F %T", "2024-03-15 14:30:45.123456" ),
            _get_time( 2024, 3, 15, 14, 30, 45 ) + us( 123456 ) );
        EXPECT_EQ( parse<us>( "%F %T", "2923-06-15 10:30:00.5" ),
            _get_time( 2923, 6, 15, 10, 30, 0 ) + us( 500000 ) );
        EXPECT_EQ( parse<us>( "%F %T", "9999-12-31 23:59:59.999999" ),
            _get_time( 9999, 12, 31, 23, 59, 59 ) + us( 999999 ) );
    }

    // 7. Nanoseconds (int64 nanoseconds overflow beyond ~year 2262,
    //    so we only test dates within that range)
    {
        using ns = nanoseconds;
        EXPECT_EQ( parse<ns>( "%F %T", "2024-03-15 14:30:45.123456789" ),
            _get_time( 2024, 3, 15, 14, 30, 45 ) + ns( 123456789 ) );
        EXPECT_EQ( parse<ns>( "%F %T", "2024-03-15 14:30:45" ),
            _get_time( 2024, 3, 15, 14, 30, 45 ) + ns( 0 ) );
    }

    // 8. Pathological: 1/7 of a second
    //    Period does not divide evenly into nanoseconds, so this exercises
    //    the floor<Dur>(nanoseconds(...)) fallback path.
    //    Date is near the epoch to avoid intermediate overflow in
    //    floor — the conversion multiplies the nanosecond count by 7.
    {
        using seventh = std::chrono::duration<i64, std::ratio<1, 7>>;

        // Integer seconds always land on a 1/7-tick boundary
        // (S seconds = S*7 ticks), so we can reason about
        // fractional-second offsets directly.
        auto base = floor<seventh>( _get_time( 1971, 6, 15, 12, 0, 0 ) );

        // 0.0s: exactly on a tick boundary
        EXPECT_EQ( parse<seventh>( "%F %T", "1971-06-15 12:00:00" ), base );

        // 0.1s: floor(0.1 * 7) = floor(0.7) = 0 ticks past the second
        EXPECT_EQ(
            parse<seventh>( "%F %T", "1971-06-15 12:00:00.1" ), base );

        // 0.15s: floor(0.15 * 7) = floor(1.05) = 1 tick (just past 1/7)
        EXPECT_EQ( parse<seventh>( "%F %T", "1971-06-15 12:00:00.15" ),
            base + seventh( 1 ) );

        // 0.2s: floor(0.2 * 7) = floor(1.4) = 1 tick
        EXPECT_EQ( parse<seventh>( "%F %T", "1971-06-15 12:00:00.2" ),
            base + seventh( 1 ) );

        // 0.5s: floor(0.5 * 7) = floor(3.5) = 3 ticks
        EXPECT_EQ( parse<seventh>( "%F %T", "1971-06-15 12:00:00.5" ),
            base + seventh( 3 ) );

        // 0.999999999s: floor(0.999999999 * 7) = floor(6.999999993) = 6
        EXPECT_EQ(
            parse<seventh>( "%F %T", "1971-06-15 12:00:00.999999999" ),
            base + seventh( 6 ) );
    }

    // 9. Pathological: 2/5 of a second
    //    Both n > 1 and d > 1, so this exercises the div_floor<n> path
    //    inside the parse_precise branch.
    {
        using two_fifths = std::chrono::duration<i64, std::ratio<2, 5>>;

        auto base = _get_time( 2024, 3, 15, 14, 30, 45 );

        // Tick boundaries (every 0.4s) don't necessarily align with
        // integer seconds, so we compute expected values via floor.
        EXPECT_EQ( parse<two_fifths>( "%F %T", "2024-03-15 14:30:45" ),
            floor<two_fifths>( base ) );

        // 0.123s falls within the same 0.4s tick or the next, depending
        // on alignment — floor handles this correctly
        EXPECT_EQ(
            parse<two_fifths>( "%F %T", "2024-03-15 14:30:45.123" ),
            floor<two_fifths>( base + milliseconds( 123 ) ) );

        // 0.5s: past the first 0.4s tick, before the second (0.8s)
        EXPECT_EQ( parse<two_fifths>( "%F %T", "2024-03-15 14:30:45.5" ),
            floor<two_fifths>( base + milliseconds( 500 ) ) );

        // 0.9s: past the second 0.4s tick (0.8s), before the third (1.2s)
        EXPECT_EQ( parse<two_fifths>( "%F %T", "2024-03-15 14:30:45.9" ),
            floor<two_fifths>( base + milliseconds( 900 ) ) );
    }
}
