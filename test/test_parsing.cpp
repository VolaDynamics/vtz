#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/parse.cpp>

#include "vtz_testing.h"
#include <gtest/gtest.h>

using namespace vtz;

TEST( vtz_parsing, parse_date_d_simple ) {
    // Test basic date formats

    // ISO date format
    auto d1 = parse_date_d( "%Y-%m-%d", "2024-03-15" );
    ASSERT_EQ( d1, resolveCivil( 2024, 3, 15 ) );

    // Date with different separators
    auto d2 = parse_date_d( "%Y/%m/%d", "2023/12/25" );
    ASSERT_EQ( d2, resolveCivil( 2023, 12, 25 ) );

    // Compact date format using %F
    auto d3 = parse_date_d( "%F", "2022-01-01" );
    ASSERT_EQ( d3, resolveCivil( 2022, 1, 1 ) );

    // Edge case: epoch date
    auto d4 = parse_date_d( "%Y-%m-%d", "1970-01-01" );
    ASSERT_EQ( d4, 0 );

    // Edge case: date with no separators
    {
        auto d5 = parse_date_d( "%Y%m%d", "20231225" );
        ASSERT_EQ( d5, resolveCivil( 2023, 12, 25 ) );
    }
    {
        auto d5 = parse_date_d( "%Y%m%d", "20240229" );
        ASSERT_EQ( d5, resolveCivil( 2024, 2, 29 ) );
    }
}

TEST( vtz_parsing, parse_time_s_simple ) {
    // Test basic time formats returning seconds since epoch

    // Full datetime
    auto t1        = parse_time_s( "%Y-%m-%d %H:%M:%S", "2024-03-15 14:30:45" );
    auto expected1 = resolveCivil( 2024, 3, 15 ) * 86400ll + 14 * 3600 + 30 * 60 + 45;
    ASSERT_EQ( t1, expected1 );

    // Using compact format specifiers
    auto t2        = parse_time_s( "%F %T", "2023-12-25 09:15:30" );
    auto expected2 = resolveCivil( 2023, 12, 25 ) * 86400ll + 9 * 3600 + 15 * 60 + 30;
    ASSERT_EQ( t2, expected2 );

    // Just time (assumes epoch date)
    auto t3 = parse_time_s( "%H:%M:%S", "12:00:00" );
    ASSERT_EQ( t3, 12 * 3600 );

    // Midnight
    auto t4        = parse_time_s( "%Y-%m-%d %H:%M:%S", "2024-01-01 00:00:00" );
    auto expected4 = resolveCivil( 2024, 1, 1 ) * 86400ll;
    ASSERT_EQ( t4, expected4 );
}

TEST( vtz_parsing, parse_time_ns_simple ) {
    // Test nanosecond precision time parsing

    // Without fractional seconds
    auto t1 = parse_time_ns( "%Y-%m-%d %H:%M:%S", "2024-03-15 14:30:45" );
    auto expected1
        = ( resolveCivil( 2024, 3, 15 ) * 86400ll + 14 * 3600 + 30 * 60 + 45 ) * 1000000000ll;
    ASSERT_EQ( t1, expected1 );

    // With fractional seconds (milliseconds)
    auto t2 = parse_time_ns( "%Y-%m-%d %H:%M:%S", "2024-03-15 14:30:45.123" );
    auto expected2
        = ( resolveCivil( 2024, 3, 15 ) * 86400ll + 14 * 3600 + 30 * 60 + 45 ) * 1000000000ll
          + 123000000ll;
    ASSERT_EQ( t2, expected2 );

    // With fractional seconds (microseconds)
    auto t3 = parse_time_ns( "%Y-%m-%d %H:%M:%S", "2024-03-15 14:30:45.123456" );
    auto expected3
        = ( resolveCivil( 2024, 3, 15 ) * 86400ll + 14 * 3600 + 30 * 60 + 45 ) * 1000000000ll
          + 123456000ll;
    ASSERT_EQ( t3, expected3 );

    // With full nanosecond precision
    auto t4 = parse_time_ns( "%Y-%m-%d %H:%M:%S", "2024-03-15 14:30:45.123456789" );
    auto expected4
        = ( resolveCivil( 2024, 3, 15 ) * 86400ll + 14 * 3600 + 30 * 60 + 45 ) * 1000000000ll
          + 123456789ll;
    ASSERT_EQ( t4, expected4 );
}

TEST( vtz_parsing, parse_year_formats ) {
    // Test different year format specifiers

    // Two-digit year (%y)
    auto d1 = parse_date_d( "%y-%m-%d", "24-03-15" );
    ASSERT_EQ( d1, resolveCivil( 2024, 3, 15 ) );

    // Two-digit year in 20th century range
    auto d2 = parse_date_d( "%y-%m-%d", "99-12-31" );
    ASSERT_EQ( d2, resolveCivil( 1999, 12, 31 ) );

    // Century + year (%C%y) - %C expects two digits for century (20), %y expects two digits for
    // year (24)
    auto d3 = parse_date_d( "%C%y-%m-%d", "2024-03-15" );
    ASSERT_EQ( d3, resolveCivil( 2024, 3, 15 ) );
}

TEST( vtz_parsing, parse_ordinal_date ) {
    // Test day of year parsing (%j)

    // First day of year
    auto d1 = parse_date_d( "%Y-%j", "2024-001" );
    ASSERT_EQ( d1, resolveCivil( 2024, 1, 1 ) );

    // Middle of year
    auto d2 = parse_date_d( "%Y-%j", "2024-100" );
    ASSERT_EQ( d2, resolveCivilOrdinal( 2024, 100 ) );

    // Last day of non-leap year
    auto d3 = parse_date_d( "%Y-%j", "2023-365" );
    ASSERT_EQ( d3, resolveCivil( 2023, 12, 31 ) );
}
