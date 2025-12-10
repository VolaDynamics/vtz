#include <vtz/endian.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>

#include "test_utils.h"
#include "test_zones.h"
#include "vtz_testing.h"

using namespace vtz;

TEST( vtz, tz_string ) {
    auto utc = time_zone::utc();

    {
        ADD_CONTEXT( "Testing Asia/Seoul" );
        auto tz = parse_tz_string( "KST-9" );

        ASSERT_EQ( tz.abbr1.sv(), "KST" );
        ASSERT_EQ( tz.off1, FromUTC::hhmmss( 9 ) );
        ASSERT_FALSE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing America/Panama" );
        auto tz = parse_tz_string( "EST5" );
        ASSERT_EQ( tz.abbr1.sv(), "EST" );
        ASSERT_EQ( tz.off1, FromUTC::hhmmss( -5 ) );
        ASSERT_FALSE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing America/Belem" );
        auto tz = parse_tz_string( "<-03>3" );
        ASSERT_EQ( tz.abbr1.sv(), "-03" );
        ASSERT_EQ( tz.off1, FromUTC::hhmmss( -3 ) );
        ASSERT_FALSE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing America/New_York" );
        auto tz = parse_tz_string( "EST5EDT,M3.2.0,M11.1.0" );
        ASSERT_EQ( tz.abbr1.sv(), "EST" );
        ASSERT_EQ( tz.abbr2.sv(), "EDT" );
        ASSERT_EQ( tz.off1, FromUTC::hhmmss( -5 ) );
        ASSERT_EQ( tz.off2, FromUTC::hhmmss( -4 ) );
        ASSERT_EQ( tz.r1.time, 3600 * 2 );
        ASSERT_EQ( tz.r2.time, 3600 * 3 );

        ASSERT_EQ( int( tz.r1.kind() ), int( TZDate::DayOfMonth ) );
        ASSERT_EQ( int( tz.r2.kind() ), int( TZDate::DayOfMonth ) );

        ASSERT_EQ( utc.format_s( tz.resolve_dst( 2025 ) ), "2025-03-09 07:00:00 UTC" );
        ASSERT_EQ( utc.format_s( tz.resolve_std( 2025 ) ), "2025-11-02 06:00:00 UTC" );

        ASSERT_TRUE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing America/Godthab" );
        auto tz = parse_tz_string( "<-02>2<-01>,M3.5.0/-1,M10.5.0/0" );
        ASSERT_EQ( tz.abbr1.sv(), "-02" );
        ASSERT_EQ( tz.abbr2.sv(), "-01" );
        ASSERT_EQ( tz.off1, FromUTC::hhmmss( -2 ) );
        ASSERT_EQ( tz.off2, FromUTC::hhmmss( -1 ) );
        ASSERT_EQ( tz.r1.time, -3600 );
        ASSERT_EQ( tz.r2.time, 0 );

        ASSERT_EQ( int( tz.r1.kind() ), int( TZDate::DayOfMonth ) );
        ASSERT_EQ( int( tz.r2.kind() ), int( TZDate::DayOfMonth ) );

        ASSERT_EQ( utc.format_s( tz.resolve_dst( 2025 ) ), "2025-03-30 01:00:00 UTC" );
        ASSERT_EQ( utc.format_s( tz.resolve_std( 2025 ) ), "2025-10-26 01:00:00 UTC" );

        ASSERT_TRUE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing Asia/Gaza" );
        auto tz = parse_tz_string( "EET-2EEST,M3.4.4/50,M10.4.4/50" );
        ASSERT_EQ( tz.abbr1.sv(), "EET" );
        ASSERT_EQ( tz.abbr2.sv(), "EEST" );
        ASSERT_EQ( tz.off1, FromUTC::hhmmss( 2 ) );
        ASSERT_EQ( tz.off2, FromUTC::hhmmss( 3 ) );
        ASSERT_EQ( tz.r1.time, 50 * 3600 );
        ASSERT_EQ( tz.r2.time, 50 * 3600 );

        ASSERT_EQ( int( tz.r1.kind() ), int( TZDate::DayOfMonth ) );
        ASSERT_EQ( int( tz.r2.kind() ), int( TZDate::DayOfMonth ) );

        ASSERT_EQ( utc.format_s( tz.resolve_dst( 2100 ) ), "2100-03-27 00:00:00 UTC" );
        ASSERT_EQ( utc.format_s( tz.resolve_std( 2100 ) ), "2100-10-29 23:00:00 UTC" );

        ASSERT_TRUE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing America/Miquelon" );
        auto tz = parse_tz_string( "<-03>3<-02>,M3.2.0,M11.1.0" );
        ASSERT_EQ( tz.abbr1.sv(), "-03" );
        ASSERT_EQ( tz.abbr2.sv(), "-02" );
        ASSERT_EQ( tz.off1, FromUTC::hhmmss( -3 ) );
        ASSERT_EQ( tz.off2, FromUTC::hhmmss( -2 ) );
        ASSERT_EQ( tz.r1.time, 3600 * 2 );
        ASSERT_EQ( tz.r2.time, 3600 * 2 );

        ASSERT_EQ( int( tz.r1.kind() ), int( TZDate::DayOfMonth ) );
        ASSERT_EQ( int( tz.r2.kind() ), int( TZDate::DayOfMonth ) );

        ASSERT_EQ( utc.format_s( tz.resolve_dst( 2025 ) ), "2025-03-09 05:00:00 UTC" );
        ASSERT_EQ( utc.format_s( tz.resolve_std( 2025 ) ), "2025-11-02 04:00:00 UTC" );
        ASSERT_TRUE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Australia/Lord_Howe" );
        auto tz = parse_tz_string( "<+1030>-10:30<+11>-11,M10.1.0,M4.1.0" );
        ASSERT_EQ( tz.abbr1.sv(), "+1030" );
        ASSERT_EQ( tz.abbr2.sv(), "+11" );
        ASSERT_EQ( tz.off1, FromUTC::hhmmss( 10, 30 ) );
        ASSERT_EQ( tz.off2, FromUTC::hhmmss( 11 ) );
        ASSERT_EQ( tz.r1.time, 3600 * 2 );
        ASSERT_EQ( tz.r2.time, 3600 * 2 );

        ASSERT_EQ( int( tz.r1.kind() ), int( TZDate::DayOfMonth ) );
        ASSERT_EQ( int( tz.r2.kind() ), int( TZDate::DayOfMonth ) );

        ASSERT_EQ( utc.format_s( tz.resolve_dst( 2025 ) ), "2025-10-04 15:30:00 UTC" );
        ASSERT_EQ( utc.format_s( tz.resolve_std( 2025 ) ), "2025-04-05 15:00:00 UTC" );
        ASSERT_TRUE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing Africa/Casablanca" );
        auto tz = parse_tz_string( "XXX-2<+01>-1,0/0,J365/23" );
        ASSERT_EQ( tz.abbr1.sv(), "XXX" );
        ASSERT_EQ( tz.abbr2.sv(), "+01" );
        ASSERT_EQ( tz.off1, FromUTC::hhmmss( +2 ) );
        ASSERT_EQ( tz.off2, FromUTC::hhmmss( +1 ) );
        ASSERT_EQ( tz.r1.time, 0 );
        ASSERT_EQ( tz.r2.time, 23 * 3600 );

        ASSERT_EQ( utc.format_s( tz.resolve_dst( 2100 ) ), "2099-12-31 22:00:00 UTC" );
        ASSERT_EQ( utc.format_s( tz.resolve_std( 2100 ) ), "2100-12-31 22:00:00 UTC" );
        ASSERT_TRUE( tz.has_daylight_rules() );
    }
}


TEST( vtz, endian ) {
    using vtz::endian::load_be;
    using vtz::endian::span_be;
    // Test _load_be

    constexpr char buff1[1]{ '\xde' };
    constexpr char buff2[2]{ '\xde', '\xad' };
    constexpr char buff4[4]{ '\xde', '\xad', '\xbe', '\xef' };
    constexpr char buff8[8]{ '\xde', '\xad', '\xbe', '\xef', '\1', '\2', '\3', '\4' };

    ASSERT_EQ( load_be<u8>( buff1 ), 0xDEu );
    ASSERT_EQ( load_be<u16>( buff2 ), 0xDEADu );
    ASSERT_EQ( load_be<u32>( buff4 ), 0xDEADBEEFu );
    ASSERT_EQ( load_be<u64>( buff8 ), 0xDEADBEEF01020304u );
    ASSERT_EQ( load_be<i8>( buff1 ), i8( 0xDEu ) );
    ASSERT_EQ( load_be<i16>( buff2 ), i16( 0xDEADu ) );
    ASSERT_EQ( load_be<i32>( buff4 ), i32( 0xDEADBEEFu ) );
    ASSERT_EQ( load_be<i64>( buff8 ), i64( 0xDEADBEEF01020304u ) );

    auto s1 = span_be<u8>( buff1, 1 );
    auto s2 = span_be<u16>( buff2, 1 );
    auto s4 = span_be<u32>( buff4, 1 );
    auto s8 = span_be<u64>( buff8, 1 );

    ASSERT_EQ( s1.size(), 1 );
    ASSERT_EQ( s2.size(), 1 );
    ASSERT_EQ( s4.size(), 1 );
    ASSERT_EQ( s8.size(), 1 );

    ASSERT_EQ( s1.end() - s1.begin(), 1 );
    ASSERT_EQ( s2.end() - s2.begin(), 1 );
    ASSERT_EQ( s4.end() - s4.begin(), 1 );
    ASSERT_EQ( s8.end() - s8.begin(), 1 );

    ASSERT_EQ( s1.end().p - s1.begin().p, 1 );
    ASSERT_EQ( s2.end().p - s2.begin().p, 2 );
    ASSERT_EQ( s4.end().p - s4.begin().p, 4 );
    ASSERT_EQ( s8.end().p - s8.begin().p, 8 );

    ASSERT_EQ( s1[0], 0xDEu );
    ASSERT_EQ( s2[0], 0xDEADu );
    ASSERT_EQ( s4[0], 0xDEADBEEFu );
    ASSERT_EQ( s8[0], 0xDEADBEEF01020304u );

    ASSERT_EQ( s1.begin() - s1.end(), -1 );
    ASSERT_EQ( s2.begin() - s2.end(), -1 );
    ASSERT_EQ( s4.begin() - s4.end(), -1 );
    ASSERT_EQ( s8.begin() - s8.end(), -1 );

    ASSERT_EQ( s1.size_bytes(), 1 );
    ASSERT_EQ( s2.size_bytes(), 2 );
    ASSERT_EQ( s4.size_bytes(), 4 );
    ASSERT_EQ( s8.size_bytes(), 8 );

    auto ss = span_be<u16>( buff8, 4 );
    ASSERT_EQ( ss[0], 0xDEAD );
    ASSERT_EQ( ss[1], 0xBEEF );
    ASSERT_EQ( ss[2], 0x0102 );
    ASSERT_EQ( ss[3], 0x0304 );
    ASSERT_EQ( ss.begin()[0], 0xDEAD );
    ASSERT_EQ( ss.begin()[1], 0xBEEF );
    ASSERT_EQ( ss.begin()[2], 0x0102 );
    ASSERT_EQ( ss.begin()[3], 0x0304 );
    ASSERT_EQ( ss.end() - ss.begin(), 4 );
    ASSERT_EQ( ss.begin() - ss.end(), -4 );

    ASSERT_EQ( ( std::vector<u16>{ 0xdead, 0xbeef, 0x0102, 0x0304 } ),
        std::vector<u16>( ss.begin(), ss.end() ) );
}
