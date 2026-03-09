#include <vtz/civil.h>
#include <vtz/endian.h>
#include <vtz/format.h>
#include <vtz/tz.h>
#include <vtz/tz_cache.h>
#include <vtz/tz_reader.h>
#include <vtz/tz_reader/from_utc.h>

#include <vtz/libfmt_compat.h>

#include "test_utils.h"
#include "test_zones.h"
#include "vtz_debug.h"
#include "vtz_testing.h"

using namespace vtz;

TEST( vtz, tz_string ) {
    {
        ADD_CONTEXT( "Testing Asia/Seoul" );
        auto tz = parse_tz_string( "KST-9" );

        ASSERT_EQ( tz.abbr1.sv(), "KST" );
        ASSERT_EQ( tz.off1, from_utc::hhmmss( 9 ) );
        ASSERT_FALSE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing America/Panama" );
        auto tz = parse_tz_string( "EST5" );
        ASSERT_EQ( tz.abbr1.sv(), "EST" );
        ASSERT_EQ( tz.off1, from_utc::hhmmss( -5 ) );
        ASSERT_FALSE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing America/Belem" );
        auto tz = parse_tz_string( "<-03>3" );
        ASSERT_EQ( tz.abbr1.sv(), "-03" );
        ASSERT_EQ( tz.off1, from_utc::hhmmss( -3 ) );
        ASSERT_FALSE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing America/New_York" );
        auto tz = parse_tz_string( "EST5EDT,M3.2.0,M11.1.0" );
        ASSERT_EQ( tz.abbr1.sv(), "EST" );
        ASSERT_EQ( tz.abbr2.sv(), "EDT" );
        ASSERT_EQ( tz.off1, from_utc::hhmmss( -5 ) );
        ASSERT_EQ( tz.off2, from_utc::hhmmss( -4 ) );
        ASSERT_EQ( tz.r1.time, 3600 * 2 );
        ASSERT_EQ( tz.r2.time, 3600 * 2 );

        ASSERT_EQ( int( tz.r1.kind() ), int( tz_date::DayOfMonth ) );
        ASSERT_EQ( int( tz.r2.kind() ), int( tz_date::DayOfMonth ) );

        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_dst( 2025 ) ), "2025-03-09 07:00:00 UTC" );
        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_std( 2025 ) ), "2025-11-02 06:00:00 UTC" );

        ASSERT_TRUE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing America/Godthab" );
        auto tz = parse_tz_string( "<-02>2<-01>,M3.5.0/-1,M10.5.0/0" );
        ASSERT_EQ( tz.abbr1.sv(), "-02" );
        ASSERT_EQ( tz.abbr2.sv(), "-01" );
        ASSERT_EQ( tz.off1, from_utc::hhmmss( -2 ) );
        ASSERT_EQ( tz.off2, from_utc::hhmmss( -1 ) );
        ASSERT_EQ( tz.r1.time, -3600 );
        ASSERT_EQ( tz.r2.time, 0 );

        ASSERT_EQ( int( tz.r1.kind() ), int( tz_date::DayOfMonth ) );
        ASSERT_EQ( int( tz.r2.kind() ), int( tz_date::DayOfMonth ) );

        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_dst( 2025 ) ), "2025-03-30 01:00:00 UTC" );
        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_std( 2025 ) ), "2025-10-26 01:00:00 UTC" );

        ASSERT_TRUE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing Asia/Gaza" );
        auto tz = parse_tz_string( "EET-2EEST,M3.4.4/50,M10.4.4/50" );
        ASSERT_EQ( tz.abbr1.sv(), "EET" );
        ASSERT_EQ( tz.abbr2.sv(), "EEST" );
        ASSERT_EQ( tz.off1, from_utc::hhmmss( 2 ) );
        ASSERT_EQ( tz.off2, from_utc::hhmmss( 3 ) );
        ASSERT_EQ( tz.r1.time, 50 * 3600 );
        ASSERT_EQ( tz.r2.time, 50 * 3600 );

        ASSERT_EQ( int( tz.r1.kind() ), int( tz_date::DayOfMonth ) );
        ASSERT_EQ( int( tz.r2.kind() ), int( tz_date::DayOfMonth ) );

        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_dst( 2100 ) ), "2100-03-27 00:00:00 UTC" );
        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_std( 2100 ) ), "2100-10-29 23:00:00 UTC" );

        ASSERT_TRUE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing America/Miquelon" );
        auto tz = parse_tz_string( "<-03>3<-02>,M3.2.0,M11.1.0" );
        ASSERT_EQ( tz.abbr1.sv(), "-03" );
        ASSERT_EQ( tz.abbr2.sv(), "-02" );
        ASSERT_EQ( tz.off1, from_utc::hhmmss( -3 ) );
        ASSERT_EQ( tz.off2, from_utc::hhmmss( -2 ) );
        ASSERT_EQ( tz.r1.time, 3600 * 2 );
        ASSERT_EQ( tz.r2.time, 3600 * 2 );

        ASSERT_EQ( int( tz.r1.kind() ), int( tz_date::DayOfMonth ) );
        ASSERT_EQ( int( tz.r2.kind() ), int( tz_date::DayOfMonth ) );

        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_dst( 2025 ) ), "2025-03-09 05:00:00 UTC" );
        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_std( 2025 ) ), "2025-11-02 04:00:00 UTC" );
        ASSERT_TRUE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Australia/Lord_Howe" );
        auto tz = parse_tz_string( "<+1030>-10:30<+11>-11,M10.1.0,M4.1.0" );
        ASSERT_EQ( tz.abbr1.sv(), "+1030" );
        ASSERT_EQ( tz.abbr2.sv(), "+11" );
        ASSERT_EQ( tz.off1, from_utc::hhmmss( 10, 30 ) );
        ASSERT_EQ( tz.off2, from_utc::hhmmss( 11 ) );
        ASSERT_EQ( tz.r1.time, 3600 * 2 );
        ASSERT_EQ( tz.r2.time, 3600 * 2 );

        ASSERT_EQ( int( tz.r1.kind() ), int( tz_date::DayOfMonth ) );
        ASSERT_EQ( int( tz.r2.kind() ), int( tz_date::DayOfMonth ) );

        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_dst( 2025 ) ), "2025-10-04 15:30:00 UTC" );
        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_std( 2025 ) ), "2025-04-05 15:00:00 UTC" );
        ASSERT_TRUE( tz.has_daylight_rules() );
    }

    {
        ADD_CONTEXT( "Testing Africa/Casablanca" );
        auto tz = parse_tz_string( "XXX-2<+01>-1,0/0,J365/23" );
        ASSERT_EQ( tz.abbr1.sv(), "XXX" );
        ASSERT_EQ( tz.abbr2.sv(), "+01" );
        ASSERT_EQ( tz.off1, from_utc::hhmmss( +2 ) );
        ASSERT_EQ( tz.off2, from_utc::hhmmss( +1 ) );
        ASSERT_EQ( tz.r1.time, 0 );
        ASSERT_EQ( tz.r2.time, 23 * 3600 );

        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_dst( 2100 ) ), "2099-12-31 22:00:00 UTC" );
        ASSERT_EQ( format_s( "%F %T %Z", tz.resolve_std( 2100 ) ), "2100-12-31 22:00:00 UTC" );
        ASSERT_TRUE( tz.has_daylight_rules() );
    }
}


TEST( vtz, tz_string_get_states ) {

    {
        ADD_CONTEXT( "Testing tz_string::get_states with no daylight rules" );
        auto tz     = parse_tz_string( "KST-9" );
        auto states = tz.get_states( _ct( 2020, 1, 1, 0, 0, 0 ), _ct( 2025, 1, 1, 0, 0, 0 ) );
        ASSERT_EQ( states.size(), 0 ); // No transitions for zones without DST
    }

    {
        ADD_CONTEXT( "Testing tz_string::get_states for America/New_York" );
        auto tz = parse_tz_string( "EST5EDT,M3.2.0,M11.1.0" );

        // Get transitions from 2024-01-01 to 2026-01-01
        auto states = tz.get_states( _ct( 2024, 1, 1, 0, 0, 0 ), _ct( 2026, 1, 1, 0, 0, 0 ) );

        // Should have 4 transitions: 2024 spring, 2024 fall, 2025 spring, 2025 fall
        ASSERT_EQ( states.size(), 4 );

        // Verify the 2024 transitions
        ASSERT_EQ( format_s( "%F %T %Z", states[0].when ), "2024-03-10 07:00:00 UTC" ); // Spring forward
        ASSERT_EQ( states[0].state.abbr.sv(), "EDT" );
        ASSERT_EQ( states[0].state.walloff, from_utc::hhmmss( -4 ) );

        ASSERT_EQ( format_s( "%F %T %Z", states[1].when ), "2024-11-03 06:00:00 UTC" ); // Fall back
        ASSERT_EQ( states[1].state.abbr.sv(), "EST" );
        ASSERT_EQ( states[1].state.walloff, from_utc::hhmmss( -5 ) );

        // Verify the 2025 transitions
        ASSERT_EQ( format_s( "%F %T %Z", states[2].when ), "2025-03-09 07:00:00 UTC" ); // Spring forward
        ASSERT_EQ( states[2].state.abbr.sv(), "EDT" );

        ASSERT_EQ( format_s( "%F %T %Z", states[3].when ), "2025-11-02 06:00:00 UTC" ); // Fall back
        ASSERT_EQ( states[3].state.abbr.sv(), "EST" );

        for( size_t i = 0; i < states.size(); ++i )
        {
            // stdoff is always -05
            ASSERT_EQ( states[i].state.stdoff, from_utc::hhmmss( -5 ) );
            bool inside_standard_time = bool( i % 2 );
            ASSERT_EQ( states[i].state.walloff,
                inside_standard_time ? from_utc::hhmmss( -5 ) : from_utc::hhmmss( -4 ) );
        }
    }

    {
        ADD_CONTEXT( "Testing tz_string::get_states with limited range" );
        auto tz = parse_tz_string( "EST5EDT,M3.2.0,M11.1.0" );

        // Get transitions from mid-2024 to end of 2024 (should only get fall transition)
        auto states = tz.get_states( _ct( 2024, 7, 1, 0, 0, 0 ), _ct( 2025, 1, 1, 0, 0, 0 ) );

        ASSERT_EQ( states.size(), 1 );
        ASSERT_EQ( format_s( "%F %T %Z", states[0].when ), "2024-11-03 06:00:00 UTC" );
        ASSERT_EQ( states[0].state.abbr.sv(), "EST" );
    }

    {
        ADD_CONTEXT(
            "Testing tz_string::get_states for always-DST zone (Africa/Casablanca style)" );
        // This represents a zone that's "always in DST" - transitions happen at the same time
        auto tz = parse_tz_string( "XXX-2<+01>-1,0/0,J365/23" );

        // According to the implementation, this should return no transitions
        auto states = tz.get_states( _ct( 2024, 1, 1, 0, 0, 0 ), _ct( 2100, 1, 1, 0, 0, 0 ) );
        ASSERT_EQ( states.size(), 0 );
    }

    {
        ADD_CONTEXT(
            "Testing tz_string::get_states for Southern Hemisphere (Australia/Lord_Howe)" );
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

        ASSERT_EQ( format_s( "%F %T %Z", states[0].when ), "2024-04-06 15:00:00 UTC" );
        ASSERT_EQ( format_s( "%F %T %Z", states[1].when ), "2024-10-05 15:30:00 UTC" );
        ASSERT_EQ( format_s( "%F %T %Z", states[2].when ), "2025-04-05 15:00:00 UTC" );

        // All of them should have the same stdoff
        ASSERT_EQ( states[0].state.stdoff, from_utc::hhmmss( 10, 30 ) );
        ASSERT_EQ( states[1].state.stdoff, from_utc::hhmmss( 10, 30 ) );
        ASSERT_EQ( states[2].state.stdoff, from_utc::hhmmss( 10, 30 ) );

        // When we enter dst in october, the walloff should be +11, and then we go back to +1030
        // when we enter standard time in the spring
        ASSERT_EQ( states[0].state.walloff, from_utc::hhmmss( 10, 30 ) );
        ASSERT_EQ( states[1].state.walloff, from_utc::hhmmss( 11 ) );
        ASSERT_EQ( states[2].state.walloff, from_utc::hhmmss( 10, 30 ) );

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


TEST( vtz, tzdb_vs_tzfile_America_New_York ) {
    COUNT_ASSERTIONS();

    auto const& fp = "build/data/tzdata/tzdata.zi";


    constexpr sysseconds_t start_t = days_to_seconds( resolve_civil( 1800, 1, 1 ) );
    constexpr sysseconds_t end_t   = days_to_seconds( resolve_civil( 2900, 1, 1 ) );


    fmt::println( "Start time: {}", utc_to_string( start_t ) );
    fmt::println( "End time:   {}", utc_to_string( end_t ) );

    auto content = read_file( fp );
    auto file    = parse_tzdata( content, fp );


    auto zones = file.list_zones();

    ASSERT_GT( zones.size(), 0 );

    struct Info {
        bool extends_forever;
        int  end_year;
        // Name of zone being tested
        string_view zone_name;

        // This will usually be the same as the zone name but we may want to alternative versions of
        // a file for a particular zone
        string_view tzfile_name = zone_name;
    };

    for( auto zone : {
             Info{ 1, 2437, "America/New_York" },
             Info{ 1, 2437, "Australia/Lord_Howe" },
             Info{ 0, 2087, "Africa/Casablanca" },
             Info{ 1, 2407, "America/New_York", "America/New_York_leap" },
             Info{ 1, 2010, "Atlantic/Stanley" },
             Info{ 1, 2037, "Europe/Dublin" },
         } )
    {
        ADD_CONTEXT( "Comparing parsed zone against tzfile", zone.zone_name, zone.tzfile_name );

        auto ny_1 = file.get_zone_states( zone.zone_name, 2900 );
        auto tt1  = ny_1.get_transitions();


        auto ny_2 = load_zone_states_tzfile( join_path( "etc/testdata", zone.tzfile_name ) );
        auto tt2  = ny_2.get_transitions();

        fmt::println(
            "Checking transitions for zone {} (tzfile={})", zone.zone_name, zone.tzfile_name );

        fmt::println( "First time: {}", utc_to_string( tt2.front().when ) );
        fmt::println( "Last time:  {}", utc_to_string( tt2.back().when ) );

        // Check that the initial states match
        ASSERT_EQ_QUIET( ny_1.initial(), ny_2.initial() );

        ASSERT_GT( tt2.back().when, _ct( zone.end_year, 1, 1, 0, 0, 0 ) );

        // Number of transition times in zone states from tzfile should not exceed number of zone
        // states from tzdata.zi (which we computed out to 2900)
        //
        // If this assertion is wrong, then the likely cause is we are adding too many states

        ASSERT_LE( tt2.size(), tt1.size() );
        size_t count = std::min( tt1.size(), tt2.size() );

        if( !zone.extends_forever ) { ASSERT_EQ( tt1.size(), tt2.size() ); }

        for( size_t i = 0; i < count; ++i )
        {
            ADD_CONTEXT( "Checking that zone_state matches",
                i,
                format_s( "%F %T %Z", tt1[i].when ),
                format_s( "%F %T %Z", tt2[i].when ),
                tt1[i].state,
                tt2[i].state );

            ASSERT_EQ_QUIET( tt1[i].when, tt2[i].when );
            ASSERT_EQ_QUIET( tt1[i].state.abbr.sv(), tt2[i].state.abbr.sv() );
            ASSERT_EQ_QUIET( tt1[i].state.stdoff, tt2[i].state.stdoff );
            ASSERT_EQ_QUIET( tt1[i].state.walloff, tt2[i].state.walloff );
        }
    }
}


// Windows does not provide zoneinfo, so we skip these tests on Windows
#ifndef _WIN32

TEST( vtz, tzdb_vs_tzfile_coherence ) {
    COUNT_ASSERTIONS();


    auto tzcache = time_zone_cache( load_zone_info_from_dir( "build/data/tzdata" ) );
    auto zones   = tzcache.zones();

    auto tzcache_tzfile
        = time_zone_cache( "build/data/zoneinfo", tzcache.zones(), tzcache.links() );

    // For the tzcache loaded from source, we should NOT have a null set of zones, or a null set of
    // rules
    ASSERT_NE( tzcache.data.zones.size(), 0 );
    ASSERT_NE( tzcache.data.rules.size(), 0 );

    // For the tzcache loaded from OS tzfiles, zones and rules should be zero, because there is no
    // source files to load from
    ASSERT_EQ( tzcache_tzfile.data.zones.size(), 0 );
    ASSERT_EQ( tzcache_tzfile.data.rules.size(), 0 );

    fmt::println( "Found {} zones in tzdata.zi", tzcache.zone_cache.size() );

    constexpr auto T0   = _ct( 1850, 1, 1, 0, 0, 0 );
    constexpr auto TMax = _ct( 2100, 1, 1, 0, 0, 0 );
    // Count up in increments of 15 minutes
    constexpr auto interval = sysseconds_t( 15 * 60 );

    using C = vtz::choose;

    for( auto const& zone_name : zones )
    {
        ADD_CONTEXT( "Testing zone", zone_name );
        // Construct system tzfile path
        std::string tzfile_path = join_path( "build/data/zoneinfo", zone_name );
        fmt::println( "Testing {} (os version loaded from {})", zone_name, tzfile_path );

        /// Timezone loaded from the time_zone_cache - used as reference implementation
        auto const& tz_ref = tzcache.locate_zone( zone_name );

        /// Timezone loaded from the OS tzfiles
        auto const& tz_os = tzcache_tzfile.locate_zone( zone_name );

        for( auto T = T0; T < TMax; T += interval )
        {
            ADD_CONTEXT( "Testing time", _dt{ T } );
            ASSERT_EQ_QUIET( tz_ref.to_local_s( T ), tz_os.to_local_s( T ) );
            ASSERT_EQ_QUIET( tz_ref.to_sys_s( T, C::earliest ), tz_os.to_sys_s( T, C::earliest ) );
            ASSERT_EQ_QUIET( tz_ref.to_sys_s( T, C::latest ), tz_os.to_sys_s( T, C::latest ) );
            ASSERT_EQ_QUIET( tz_ref.offset_s( T ), tz_os.offset_s( T ) );
            ASSERT_EQ_QUIET( tz_ref.abbrev_s( T ), tz_os.abbrev_s( T ) );
        }
    }
}

TEST( vtz, tzdb_vs_tzfile_state_coherence ) {
    COUNT_ASSERTIONS();

    auto const& fp = "build/data/tzdata/tzdata.zi";


    fmt::println( "=== Comprehensive System Tzfile Test ===" );
    fmt::println( "Loading tzdata.zi..." );

    auto content = read_file( fp );
    auto file    = parse_tzdata( content, fp );
    auto zones   = file.list_zones();

    fmt::println( "Found {} zones in tzdata.zi", zones.size() );

    // Statistics tracking
    size_t total_zones        = 0;
    size_t zones_with_tzfiles = 0;
    size_t zones_tested       = 0;
    size_t zones_passed       = 0;
    size_t zones_failed       = 0;

    std::vector<std::string> missing_tzfiles;
    std::vector<std::string> failed_zones;

    // Test each zone
    for( auto const& zone_name : zones )
    {
        total_zones++;

        // Construct system tzfile path
        std::string tzfile_path = join_path( "build/data/zoneinfo", zone_name );

        // Try to load the system tzfile
        try
        {
            auto tzfile_states = load_zone_states_tzfile( tzfile_path );
            zones_with_tzfiles++;

            // Get zone states from tzdata.zi
            auto tzdata_states = file.get_zone_states( zone_name, 2900 );

            auto tt1 = tzdata_states.get_transitions();
            auto tt2 = tzfile_states.get_transitions();

            bool zone_passed = true;

            // Check initial states
            if( tzdata_states.initial() != tzfile_states.initial() )
            {
                fmt::println( "  {} - Initial state mismatch", zone_name );
                fmt::println(
                    "    tzdata: {}", _test_vtz::debug_to_string( tzdata_states.initial() ) );
                fmt::println(
                    "    tzfile: {}", _test_vtz::debug_to_string( tzfile_states.initial() ) );
                zone_passed = false;
            }

            // Compare transitions (up to the minimum of both)
            size_t count = std::min( tt1.size(), tt2.size() );
            for( size_t i = 0; i < count; ++i )
            {
                if( tt1[i].when != tt2[i].when
                    || tt1[i].state.abbr.sv() != tt2[i].state.abbr.sv()
                    // || tt1[i].state.stdoff != tt2[i].state.stdoff
                    || tt1[i].state.walloff != tt2[i].state.walloff )
                {
                    fmt::println( "  {} - Mismatch at transition {} ({})",
                        zone_name,
                        i,
                        format_s( "%F %T %Z", tt1[i].when ) );
                    fmt::println( "    tzdata: [when={}] {}",
                        format_s( "%F %T %Z", tt1[i].when ),
                        _test_vtz::debug_to_string( tt1[i].state ) );
                    fmt::println( "    tzfile: [when={}] {}",
                        format_s( "%F %T %Z", tt2[i].when ),
                        _test_vtz::debug_to_string( tt2[i].state ) );
                    zone_passed = false;
                    break;
                }
            }

            zones_tested++;
            if( zone_passed ) { zones_passed++; }
            else
            {
                zones_failed++;
                failed_zones.push_back( zone_name );
            }
        }
        catch( std::exception const& e )
        {
            // File doesn't exist or can't be read
            missing_tzfiles.push_back( zone_name );
        }
    }

    // Report summary
    fmt::println( "\n=== Test Summary ===" );
    fmt::println( "Total zones in tzdata.zi:     {}", total_zones );
    fmt::println( "Zones with system tzfiles:    {}", zones_with_tzfiles );
    fmt::println( "Zones successfully tested:    {}", zones_tested );
    fmt::println( "Zones passed:                 {}", zones_passed );
    fmt::println( "Zones failed:                 {}", zones_failed );
    fmt::println( "Missing system tzfiles:       {}", missing_tzfiles.size() );

    if( !failed_zones.empty() )
    {
        fmt::println( "\nFailed zones:" );
        for( auto const& zone : failed_zones ) { fmt::println( "  - {}", zone ); }
    }

    if( !missing_tzfiles.empty() && missing_tzfiles.size() <= 20 )
    {
        fmt::println( "\nMissing tzfiles (first 20):" );
        for( size_t i = 0; i < std::min( size_t( 20 ), missing_tzfiles.size() ); ++i )
        { fmt::println( "  - {}", missing_tzfiles[i] ); }
    }

    // The test passes as long as we can test some zones
    ASSERT_GT( zones_tested, 0 );

    // Report pass rate
    if( zones_tested > 0 )
    {
        double pass_rate = ( zones_passed * 100.0 ) / zones_tested;
        fmt::println( "\nPass rate: {:.2f}% ({}/{})", pass_rate, zones_passed, zones_tested );
    }
}

#endif
