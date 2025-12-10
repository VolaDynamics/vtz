#include <vtz/endian.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>
#include <vtz/tz_reader/FromUTC.h>

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
        ASSERT_EQ( tz.r2.time, 3600 * 2 );

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


TEST( vtz, tz_string_get_states ) {
    auto utc = time_zone::utc();

    {
        ADD_CONTEXT( "Testing TZString::get_states with no daylight rules" );
        auto tz     = parse_tz_string( "KST-9" );
        auto states = tz.get_states( _ct( 2020, 1, 1, 0, 0, 0 ), _ct( 2025, 1, 1, 0, 0, 0 ) );
        ASSERT_EQ( states.size(), 0 ); // No transitions for zones without DST
    }

    {
        ADD_CONTEXT( "Testing TZString::get_states for America/New_York" );
        auto tz = parse_tz_string( "EST5EDT,M3.2.0,M11.1.0" );

        // Get transitions from 2024-01-01 to 2026-01-01
        auto states = tz.get_states( _ct( 2024, 1, 1, 0, 0, 0 ), _ct( 2026, 1, 1, 0, 0, 0 ) );

        // Should have 4 transitions: 2024 spring, 2024 fall, 2025 spring, 2025 fall
        ASSERT_EQ( states.size(), 4 );

        // Verify the 2024 transitions
        ASSERT_EQ( utc.format_s( states[0].when ), "2024-03-10 07:00:00 UTC" ); // Spring forward
        ASSERT_EQ( states[0].state.abbr.sv(), "EDT" );
        ASSERT_EQ( states[0].state.walloff, FromUTC::hhmmss( -4 ) );

        ASSERT_EQ( utc.format_s( states[1].when ), "2024-11-03 06:00:00 UTC" ); // Fall back
        ASSERT_EQ( states[1].state.abbr.sv(), "EST" );
        ASSERT_EQ( states[1].state.walloff, FromUTC::hhmmss( -5 ) );

        // Verify the 2025 transitions
        ASSERT_EQ( utc.format_s( states[2].when ), "2025-03-09 07:00:00 UTC" ); // Spring forward
        ASSERT_EQ( states[2].state.abbr.sv(), "EDT" );

        ASSERT_EQ( utc.format_s( states[3].when ), "2025-11-02 06:00:00 UTC" ); // Fall back
        ASSERT_EQ( states[3].state.abbr.sv(), "EST" );

        for( size_t i = 0; i < states.size(); ++i )
        {
            // stdoff is always -05
            ASSERT_EQ( states[i].state.stdoff, FromUTC::hhmmss( -5 ) );
            bool inside_standard_time = bool( i % 2 );
            ASSERT_EQ( states[i].state.walloff,
                inside_standard_time ? FromUTC::hhmmss( -5 ) : FromUTC::hhmmss( -4 ) );
        }
    }

    {
        ADD_CONTEXT( "Testing TZString::get_states with limited range" );
        auto tz = parse_tz_string( "EST5EDT,M3.2.0,M11.1.0" );

        // Get transitions from mid-2024 to end of 2024 (should only get fall transition)
        auto states = tz.get_states( _ct( 2024, 7, 1, 0, 0, 0 ), _ct( 2025, 1, 1, 0, 0, 0 ) );

        ASSERT_EQ( states.size(), 1 );
        ASSERT_EQ( utc.format_s( states[0].when ), "2024-11-03 06:00:00 UTC" );
        ASSERT_EQ( states[0].state.abbr.sv(), "EST" );
    }

    {
        ADD_CONTEXT( "Testing TZString::get_states for always-DST zone (Africa/Casablanca style)" );
        // This represents a zone that's "always in DST" - transitions happen at the same time
        auto tz = parse_tz_string( "XXX-2<+01>-1,0/0,J365/23" );

        // According to the implementation, this should return no transitions
        auto states = tz.get_states( _ct( 2024, 1, 1, 0, 0, 0 ), _ct( 2100, 1, 1, 0, 0, 0 ) );
        ASSERT_EQ( states.size(), 0 );
    }

    {
        ADD_CONTEXT( "Testing TZString::get_states for Southern Hemisphere (Australia/Lord_Howe)" );
        auto tz = parse_tz_string( "<+1030>-10:30<+11>-11,M10.1.0,M4.1.0" );

        // Get transitions for 2024-2025
        auto states = tz.get_states( _ct( 2024, 1, 1, 0, 0, 0 ), _ct( 2025, 7, 1, 0, 0, 0 ) );

        // There should be exactly 3 transitions:
        // - Apr 2024 transition
        // - Oct 2024 transition
        // - Apr 2025 transition
        //
        // The others are cutoff due to the cutoff provided to get_states
        ASSERT_EQ( states.size(), 3 );

        ASSERT_EQ( utc.format_s( states[0].when ), "2024-04-06 15:00:00 UTC" );
        ASSERT_EQ( utc.format_s( states[1].when ), "2024-10-05 15:30:00 UTC" );
        ASSERT_EQ( utc.format_s( states[2].when ), "2025-04-05 15:00:00 UTC" );

        // All of them should have the same stdoff
        ASSERT_EQ( states[0].state.stdoff, FromUTC::hhmmss( 10, 30 ) );
        ASSERT_EQ( states[1].state.stdoff, FromUTC::hhmmss( 10, 30 ) );
        ASSERT_EQ( states[2].state.stdoff, FromUTC::hhmmss( 10, 30 ) );

        // When we enter dst in october, the walloff should be +11, and then we go back to +1030
        // when we enter standard time in the spring
        ASSERT_EQ( states[0].state.walloff, FromUTC::hhmmss( 10, 30 ) );
        ASSERT_EQ( states[1].state.walloff, FromUTC::hhmmss( 11 ) );
        ASSERT_EQ( states[2].state.walloff, FromUTC::hhmmss( 10, 30 ) );

        // Check first transition is to standard time (April in Southern Hemisphere)
        ASSERT_EQ( states[0].state.abbr.sv(), "+1030" );
        ASSERT_EQ( states[1].state.abbr.sv(), "+11" );
        ASSERT_EQ( states[2].state.abbr.sv(), "+1030" );
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
