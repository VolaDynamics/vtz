#include "test_helper.h"

#include <gtest/gtest.h>
#include <vtz/format.h>
#include <vtz/parse.h>

using namespace vtz;


TEST( vtz_parse, sanity ) {
    // %F — ISO date
    EXPECT_EQ(
        parse_sys_seconds( "%F", "2024-01-01" ), _get_time( 2024, 1, 1 ) );
    EXPECT_EQ(
        parse_sys_seconds( "%F", "2024-02-29" ), _get_time( 2024, 2, 29 ) );
    EXPECT_EQ(
        parse_sys_seconds( "%F", "2024-12-31" ), _get_time( 2024, 12, 31 ) );
    EXPECT_EQ(
        parse_sys_seconds( "%F", "1970-01-01" ), _get_time( 1970, 1, 1 ) );
    EXPECT_EQ(
        parse_sys_seconds( "%F", "2000-01-01" ), _get_time( 2000, 1, 1 ) );
    EXPECT_EQ( parse_sys_seconds( "%F", "0001-01-01" ), _get_time( 1, 1, 1 ) );

    // %F %T — ISO date + time
    EXPECT_EQ( parse_sys_seconds( "%F %T", "2024-02-29 14:30:00" ),
        _get_time( 2024, 2, 29, 14, 30, 0 ) );
    EXPECT_EQ( parse_sys_seconds( "%F %T", "2024-12-31 23:59:59" ),
        _get_time( 2024, 12, 31, 23, 59, 59 ) );
    EXPECT_EQ( parse_sys_seconds( "%F %T", "1970-01-01 00:00:00" ),
        _get_time( 1970, 1, 1, 0, 0, 0 ) );
    EXPECT_EQ( parse_sys_seconds( "%F %T", "2025-02-19 21:20:00" ),
        _get_time( 2025, 2, 19, 21, 20, 0 ) );

    // %Y-%m-%d %H:%M:%S — same as above, spelled out
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%d %H:%M:%S", "2024-07-04 15:30:45" ),
        _get_time( 2024, 7, 4, 15, 30, 45 ) );

    // %Y%m%d — compact date
    EXPECT_EQ(
        parse_sys_seconds( "%Y%m%d", "20240101" ), _get_time( 2024, 1, 1 ) );
    EXPECT_EQ(
        parse_sys_seconds( "%Y%m%d", "20241231" ), _get_time( 2024, 12, 31 ) );

    // %d/%m/%Y — day/month/year
    EXPECT_EQ( parse_sys_seconds( "%d/%m/%Y", "29/02/2024" ),
        _get_time( 2024, 2, 29 ) );
    EXPECT_EQ( parse_sys_seconds( "%d/%m/%Y", "25/12/2024" ),
        _get_time( 2024, 12, 25 ) );

    // %R — HH:MM
    EXPECT_EQ( parse_sys_seconds( "%F %R", "2024-03-15 08:05" ),
        _get_time( 2024, 3, 15, 8, 5, 0 ) );
    EXPECT_EQ( parse_sys_seconds( "%F %R", "2024-03-15 23:59" ),
        _get_time( 2024, 3, 15, 23, 59, 0 ) );

    // %y — 2-digit year
    EXPECT_EQ(
        parse_sys_seconds( "%y-%m-%d", "24-06-15" ), _get_time( 2024, 6, 15 ) );
    EXPECT_EQ(
        parse_sys_seconds( "%y-%m-%d", "00-01-01" ), _get_time( 2000, 1, 1 ) );
    EXPECT_EQ(
        parse_sys_seconds( "%y-%m-%d", "69-01-01" ), _get_time( 1969, 1, 1 ) );
    EXPECT_EQ( parse_sys_seconds( "%y-%m-%d", "99-12-31" ),
        _get_time( 1999, 12, 31 ) );

    // %C%y — century + 2-digit year
    EXPECT_EQ( parse_sys_seconds( "%C%y-%m-%d", "2024-06-15" ),
        _get_time( 2024, 6, 15 ) );
    EXPECT_EQ( parse_sys_seconds( "%C%y-%m-%d", "1900-01-01" ),
        _get_time( 1900, 1, 1 ) );

    // %j — day of year (ordinal date)
    EXPECT_EQ(
        parse_sys_seconds( "%Y-%j", "2024-001" ), _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse_sys_seconds( "%Y-%j", "2024-060" ),
        _get_time( 2024, 2, 29 ) ); // 2024 leap: day 60 = Feb 29
    EXPECT_EQ(
        parse_sys_seconds( "%Y-%j", "2024-366" ), _get_time( 2024, 12, 31 ) );

    // %e — synonym of %d
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%e", "2024-01-01" ),
        _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%e", "2024-01-15" ),
        _get_time( 2024, 1, 15 ) );

    // %w and %u — weekday parsing (consumed but not used for date resolution)
    EXPECT_EQ(
        parse_sys_seconds( "%w %F", "1 2024-01-01" ), _get_time( 2024, 1, 1 ) );
    EXPECT_EQ(
        parse_sys_seconds( "%u %F", "1 2024-01-01" ), _get_time( 2024, 1, 1 ) );

    // %Z — timezone abbreviation (consumed but not used for offset)
    EXPECT_EQ( parse_sys_seconds( "%F %T %Z", "2024-01-01 00:00:00 UTC" ),
        _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse_sys_seconds( "%F %T %Z", "2024-06-15 12:00:00 EST" ),
        _get_time( 2024, 6, 15, 12, 0, 0 ) );

    // %n and %t — whitespace matching
    EXPECT_EQ( parse_sys_seconds( "%F%n%T", "2024-01-01 12:30:00" ),
        _get_time( 2024, 1, 1, 12, 30, 0 ) );
    EXPECT_EQ( parse_sys_seconds( "%F%t%T", "2024-01-01\t12:30:00" ),
        _get_time( 2024, 1, 1, 12, 30, 0 ) );

    // %% — literal percent
    EXPECT_EQ( parse_sys_seconds( "%F %% %T", "2024-01-01 % 12:00:00" ),
        _get_time( 2024, 1, 1, 12, 0, 0 ) );
}


TEST( vtz_parse, leading_space ) {
    // Specifiers that accept a leading space via parse_d2_allow_space:
    //   %d, %e, %m, %H  (and their compound forms %F, %R, %T)

    // %d — day of month with leading space
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%d", "2024-01- 1" ),
        _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%d", "2024-12- 9" ),
        _get_time( 2024, 12, 9 ) );

    // %e — synonym of %d, same behavior
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%e", "2024-01- 1" ),
        _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%e", "2024-06- 5" ),
        _get_time( 2024, 6, 5 ) );

    // %m — month with leading space
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%d", "2024- 1-01" ),
        _get_time( 2024, 1, 1 ) );
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%d", "2024- 8-15" ),
        _get_time( 2024, 8, 15 ) );

    // %m and %d both with leading spaces
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%d", "2024- 8- 9" ),
        _get_time( 2024, 8, 9 ) );

    // %H — hour with leading space
    EXPECT_EQ( parse_sys_seconds( "%F %H:%M:%S", "2024-01-01  5:30:00" ),
        _get_time( 2024, 1, 1, 5, 30, 0 ) );
    EXPECT_EQ( parse_sys_seconds( "%F %H:%M:%S", "2024-01-01  0:00:00" ),
        _get_time( 2024, 1, 1, 0, 0, 0 ) );

    // %F — compound; month and day parts accept leading space
    EXPECT_EQ(
        parse_sys_seconds( "%F", "2024- 1- 1" ), _get_time( 2024, 1, 1 ) );
    EXPECT_EQ(
        parse_sys_seconds( "%F", "2024- 3-15" ), _get_time( 2024, 3, 15 ) );

    // %R — compound; hour part accepts leading space
    EXPECT_EQ( parse_sys_seconds( "%F %R", "2024-01-01  8:05" ),
        _get_time( 2024, 1, 1, 8, 5, 0 ) );

    // %T — compound; hour part accepts leading space
    EXPECT_EQ( parse_sys_seconds( "%F %T", "2024-01-01  8:05:30" ),
        _get_time( 2024, 1, 1, 8, 5, 30 ) );

    // Two-digit values still work (no regression)
    EXPECT_EQ( parse_sys_seconds( "%Y-%m-%d", "2024-12-31" ),
        _get_time( 2024, 12, 31 ) );
    EXPECT_EQ( parse_sys_seconds( "%F %T", "2024-01-01 23:59:59" ),
        _get_time( 2024, 1, 1, 23, 59, 59 ) );

    // Specifiers that should NOT accept a leading space: %y, %C, %M, %S

    // %y requires digits only (2-digit year is always zero-padded)
    EXPECT_THROW( parse_sys_seconds( "%y-%m-%d", " 4-01-01" ), std::exception );
    // %C requires digits only (century is always zero-padded)
    EXPECT_THROW(
        parse_sys_seconds( "%C%y-%m-%d", " 024-01-01" ), std::exception );
    // %M requires digits only (minutes are always zero-padded)
    EXPECT_THROW( parse_sys_seconds( "%F %H:%M:%S", "2024-01-01 12: 5:00" ),
        std::exception );
    // %S requires digits only (seconds are always zero-padded)
    EXPECT_THROW( parse_sys_seconds( "%F %H:%M:%S", "2024-01-01 12:05: 0" ),
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
        "%F %T",               // ISO date + ISO time
        "%Y-%m-%d %H:%M:%S",   // equivalent, spelled out
        "%Y%m%d %H:%M:%S",     // compact date
        "%d/%m/%Y %T",         // day/month/year
        "%Y-%j %T",            // ordinal date + time
        "%Y-%j %H:%M:%S",      // ordinal date + time, spelled out
        "%C%y%m%d %T",         // century + 2-digit year + compact date
        "%C%y-%m-%d %H:%M:%S", // century + 2-digit year + date
        "%Y-%m-%d %R:%S",      // %R for hour:minute, then %S
    };

    for( auto tp : time_values )
    {
        auto t = tp.time_since_epoch().count();
        for( auto fmt : round_trip_fmts )
        {
            auto formatted = format_time_s( fmt, t );
            auto parsed    = parse_time_s( fmt, formatted );
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
    };

    for( auto dp : date_values )
    {
        auto d
            = i32( std::chrono::floor<days>( dp.time_since_epoch() ).count() );
        for( auto fmt : date_fmts )
        {
            auto formatted = format_date_d( fmt, d );
            auto parsed    = parse_date_d( fmt, formatted );
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
            auto formatted = format_time_s( "%F", t_sec );
            auto parsed    = parse_time_s( "%F", formatted );
            ASSERT_EQ( parsed, t_sec )
                << "round-trip failed for 2024-" << m << "-" << d
                << " formatted=\"" << formatted << "\"";
        }
}
