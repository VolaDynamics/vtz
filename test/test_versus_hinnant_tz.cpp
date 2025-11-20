#include <gtest/gtest.h>
#include <random>
#include <string>
#include <vtz/date_types.h>
#include <vtz/files.h>
#include <vtz/strings.h>
#include <vtz/tz_impl.h>
#include <vtz/tz_reader.h>

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>

#include <vtz/civil.h>
#include <vtz/tz.h>
#include <vtz/tz_cache.h>

#include "test_utils.h"
#include "test_zones.h"
#include "vtz_testing.h"

#include <date/date.h>
#include <date/tz.h>

using namespace vtz;
using _test_vtz::TEST_LOG;

namespace {
    int _do_set_install_hinnant = ( date::set_install( "build/data/tzdata" ), 0 );
} // namespace

static sys_seconds toSys( sysseconds_t t ) { return sys_seconds( seconds( t ) ); }

static vector<sys_seconds> toSysVec( span<sysseconds_t> tt ) {
    vector<sys_seconds> result( tt.size() );
    for( size_t i = 0; i < tt.size(); ++i ) { result[i] = sys_seconds( seconds( tt[i] ) ); }
    return result;
}


STRUCT_INFO( vtz::sys_info,
    FIELD( vtz::sys_info, begin ),
    FIELD( vtz::sys_info, end ),
    FIELD( vtz::sys_info, offset ),
    FIELD( vtz::sys_info, save ),
    FIELD( vtz::sys_info, abbrev ) );

STRUCT_INFO( vtz::local_info,
    FIELD( vtz::local_info, result ),
    FIELD( vtz::local_info, first ),
    FIELD( vtz::local_info, second ) );


struct entry {
    string       abbr;
    sysseconds_t begin;
    sysseconds_t end;
    FromUTC      offset;
    RuleSave     save;

    template<size_t N>
    ZoneAbbr fixAbbr() const {
        FixStr<N> result{};
        if( abbr.size() > N ) { throw std::runtime_error( "fixAbbr(): abbreviation too big!" ); }
        memcpy( result.buff_, abbr.data(), abbr.size() );
        result.size_ = abbr.size();
        return { result };
    }

    FromUTC getStdoff() const { return offset.off - save.save; }

    ZoneState state() const { return ZoneState{ getStdoff(), save, fixAbbr<15>() }; }
};

std::vector<entry> getEntries( string_view name, sys_seconds start, sys_seconds end ) {
    auto const*        tz = date::locate_zone( name );
    std::vector<entry> entries;

    auto T = start;
    while( T < end )
    {
        auto info = tz->get_info( T );
        entries.push_back( entry{
            info.abbrev,
            info.begin.time_since_epoch().count(),
            info.end.time_since_epoch().count(),
            FromUTC( info.offset.count() ),
            RuleSave( info.save.count() * 60 ),
        } );
        T = info.end;
    }

    return entries;
}

std::vector<date::sys_info> getSysInfo( string_view name, sysseconds_t start, sysseconds_t end ) {
    auto const* tz      = date::locate_zone( name );
    auto        T       = sys_seconds( seconds( start ) );
    auto        Tend    = sys_seconds( seconds( end ) );
    auto        entries = std::vector<date::sys_info>();
    while( T < Tend )
    {
        auto info = tz->get_info( T );
        T         = info.end;
        entries.emplace_back( std::move( info ) );
    }
    return entries;
}

std::vector<entry> getEntries( string_view name, sysseconds_t start, sysseconds_t end ) {
    return getEntries( name, sys_seconds( seconds( start ) ), sys_seconds( seconds( end ) ) );
}


TEST( vtz, Buenos_Aires ) {
    // On October 3rd, 1999, two things happened simultaneously at midnight:
    // - Argentina entered daylight savings time with save=1:00
    // - Buenos Aires

    string_view input = R"(
Rule	Arg	1930	only	-	Dec	 1	0:00	1:00	-
Rule	Arg	1931	only	-	Apr	 1	0:00	0	-
Rule	Arg	1931	only	-	Oct	15	0:00	1:00	-
Rule	Arg	1932	1940	-	Mar	 1	0:00	0	-
Rule	Arg	1932	1939	-	Nov	 1	0:00	1:00	-
Rule	Arg	1940	only	-	Jul	 1	0:00	1:00	-
Rule	Arg	1941	only	-	Jun	15	0:00	0	-
Rule	Arg	1941	only	-	Oct	15	0:00	1:00	-
Rule	Arg	1943	only	-	Aug	 1	0:00	0	-
Rule	Arg	1943	only	-	Oct	15	0:00	1:00	-
Rule	Arg	1946	only	-	Mar	 1	0:00	0	-
Rule	Arg	1946	only	-	Oct	 1	0:00	1:00	-
Rule	Arg	1963	only	-	Oct	 1	0:00	0	-
Rule	Arg	1963	only	-	Dec	15	0:00	1:00	-
Rule	Arg	1964	1966	-	Mar	 1	0:00	0	-
Rule	Arg	1964	1966	-	Oct	15	0:00	1:00	-
Rule	Arg	1967	only	-	Apr	 2	0:00	0	-
Rule	Arg	1967	1968	-	Oct	Sun>=1	0:00	1:00	-
Rule	Arg	1968	1969	-	Apr	Sun>=1	0:00	0	-
Rule	Arg	1974	only	-	Jan	23	0:00	1:00	-
Rule	Arg	1974	only	-	May	 1	0:00	0	-
Rule	Arg	1988	only	-	Dec	 1	0:00	1:00	-
Rule	Arg	1989	1993	-	Mar	Sun>=1	0:00	0	-
Rule	Arg	1989	1992	-	Oct	Sun>=15	0:00	1:00	-
Rule	Arg	1999	only	-	Oct	Sun>=1	0:00	1:00	-
Rule	Arg	2000	only	-	Mar	3	0:00	0	-
Rule	Arg	2007	only	-	Dec	30	0:00	1:00	-
Rule	Arg	2008	2009	-	Mar	Sun>=15	0:00	0	-
Rule	Arg	2008	only	-	Oct	Sun>=15	0:00	1:00	-
Zone America/Argentina/Buenos_Aires -3:53:48 - LMT	1894 Oct 31
		#STDOFF	-4:16:48.25
			-4:16:48 -	CMT	1920 May    # Córdoba Mean Time
			-4:00	-	%z	1930 Dec
			-4:00	Arg	%z	1969 Oct  5
			-3:00	Arg	%z	1999 Oct  3
			-4:00	Arg	%z	2000 Mar  3
			-3:00	Arg	%z
)";

    auto content = parseTZData( input, "(test input)" );
}

constexpr auto E  = choose::earliest;
constexpr auto L  = choose::latest;
constexpr auto DE = date::choose::earliest;
constexpr auto DL = date::choose::latest;

TEST( vtz, America_NewYork ) {
    COUNT_ASSERTIONS();

    auto const& fp = "build/data/tzdata/tzdata.zi";

    auto content = readFile( fp );
    auto file    = parseTZData( content, fp );

    auto states = file.getZoneStates( "America/New_York", 2500 );

    auto tz = TimeZone( "America/New_York", states );


    // 2025-10-29 - the current offset from UTC is UTC-04
    // so, UTC is 4 hours ahead
    // clang-format off

    // The abbreviation should change from EDT to EST
    ASSERT_EQ( tz.abbrev_s( _ct( 2025, 11,  2,  5, 59, 59 ) ), "EDT" );
    ASSERT_EQ( tz.abbrev_s( _ct( 2025, 11,  2,  6,  0,  0 ) ), "EST" );

    ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025, 10, 29, 12, 0, 0 ), L ) }, DT::civil( 2025, 10, 29, 16, 0, 0 ) );

    // prior to daylight savings time transition, early and latest times match
    ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025, 11,  2,  0, 59, 59 ), E ) }, DT::civil( 2025, 11,  2,  4, 59, 59 ) );
    ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025, 11,  2,  0, 59, 59 ), L ) }, DT::civil( 2025, 11,  2,  4, 59, 59 ) );

    // at 2AM the clocks move back an hour. The offset becomes UTC-05, so 2AM uniquely refers to 7am UTC time
    ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025, 11,  2,  2,  0,  0 ), E ) }, DT::civil( 2025, 11,  2,  7,  0,  0 ) );
    ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025, 11,  2,  2,  0,  0 ), L ) }, DT::civil( 2025, 11,  2,  7,  0,  0 ) );

    // times from 1 AM to 1:59AM are ambiguous - they could refer to either 5AM UTC or 6AM UTC
    ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025, 11,  2,  1,  0,  0 ), E ) }, DT::civil( 2025, 11,  2,  5,  0,  0 ) );
    ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025, 11,  2,  1,  0,  0 ), L ) }, DT::civil( 2025, 11,  2,  6,  0,  0 ) );
    ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025, 11,  2,  1, 59, 59 ), E ) }, DT::civil( 2025, 11,  2,  5, 59, 59 ) );
    ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025, 11,  2,  1, 59, 59 ), L ) }, DT::civil( 2025, 11,  2,  6, 59, 59 ) );

    // clang-format on

    {
        auto t0 = _ct( 2025, 3, 9, 7, 0, 0 );
        auto t1 = _ct( 2025, 11, 2, 6, 0, 0 );
        auto t2 = _ct( 2026, 3, 8, 7, 0, 0 );

        ADD_CONTEXT( "Checking get_info",
            tz.format_s( t0 ),
            tz.format_s( t1 - 1 ),
            tz.format_s( t1 ),
            tz.format_s( t2 ) );

        auto s0 = tz.get_info_sys_s( t1 - 1 );
        auto s1 = tz.get_info_sys_s( t1 );
        ASSERT_EQ( s0,
            ( sys_info{
                toSys( t0 ),
                toSys( t1 ),
                hours( -4 ),
                minutes( 60 ),
                "EDT",
            } ) );
        ASSERT_EQ( s1,
            ( sys_info{
                toSys( t1 ),
                toSys( t2 ),
                hours( -5 ),
                minutes( 0 ),
                "EST",
            } ) );

        {
            // 1am is the first time that is ambiguous, 1:59:59am is the last time
            // that is ambiguous
            ASSERT_EQ( tz.get_info_local_s( _ct( 2025, 11, 2, 1, 0, 0 ) ),
                local_info( { local_info::ambiguous, s0, s1 } ) );
            ASSERT_EQ( tz.get_info_local_s( _ct( 2025, 11, 2, 1, 59, 59 ) ),
                local_info( { local_info::ambiguous, s0, s1 } ) );

            // 00:59am is not ambiguous
            ASSERT_EQ( tz.get_info_local_s( _ct( 2025, 11, 2, 0, 59, 59 ) ),
                local_info( { local_info::unique, s0 } ) );

            // 2am is no longer ambiguous
            ASSERT_EQ( tz.get_info_local_s( _ct( 2025, 11, 2, 2, 0, 0 ) ),
                local_info( { local_info::unique, s1 } ) );
        }
    }


    // In the spring, when we jump forward and skip an hour, whether we choose the earliest or
    // latest time makes no difference
    //
    // Also, non-existent times map to the same value
    for( auto choose : { choose::earliest, choose::latest } )
    {
        // clang-format off
        ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025,  3,  9,  1, 59, 59 ), choose ) }, DT::civil( 2025,  3,  9,  6, 59, 59 ) );
        ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025,  3,  9,  3,  0,  0 ), choose ) }, DT::civil( 2025,  3,  9,  7,  0,  0 ) );
        ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025,  3,  9,  3,  0,  1 ), choose ) }, DT::civil( 2025,  3,  9,  7,  0,  1 ) );

        // On 2025-03-09  2AM..3AM are all non-existant, and they should all map to 7am
        ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025,  3,  9,  2,  0,  0 ), choose ) }, DT::civil( 2025,  3,  9,  7,  0,  0 ) );
        ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025,  3,  9,  2, 30,  0 ), choose ) }, DT::civil( 2025,  3,  9,  7,  0,  0 ) );
        ASSERT_EQ( DT{ tz.to_sys_s( _ct( 2025,  3,  9,  2, 59, 59 ), choose ) }, DT::civil( 2025,  3,  9,  7,  0,  0 ) );
        // clang-format on
    }
}


TEST( vtz, FirstTransition ) {
    COUNT_ASSERTIONS();


    auto const& fp = "build/data/tzdata/tzdata.zi";

    auto content = readFile( fp );
    auto file    = parseTZData( content, fp );


    // Check that we give the correct times for inputs around the _first_ transition
    // in the zone.
    //
    // This is necessary for checking that we made our table large enough.
    //
    // We are going to test two zones here: America/New_York, and America/Phoenix
    //
    // These two zones were chosen because America/New_York moved the clocks _back_ when
    // switching to standard time, and America/Phoenix moved the clocks _forward_ when
    // switching to standard time.
    //
    // This means that America/New_York had some ambiguous local times, while
    // America/Phoenix had some nonexistent local times.

    // clang-format off
    /*
    America/New_York set the clocks back a few minutes
    at 5PM UT (what became 12:00 local time) on 1883 Nov 18:

    Zone America/New_York   -4:56:02   -    LMT    1883 Nov 18 17:00u
                               -5:00   US   E%sT   1920
                            ...

    From zdump:

    America/New_York  Sun Nov 18 16:59:59 1883 UT = Sun Nov 18 12:03:57 1883 LMT isdst=0 gmtoff=-17762
    America/New_York  Sun Nov 18 17:00:00 1883 UT = Sun Nov 18 12:00:00 1883 EST isdst=0 gmtoff=-18000
    */
    // clang-format on
    auto zoneStatenNY = file.getZoneStates( "America/New_York", 2500 );

    /// America/New_York
    auto NY = TimeZone( "America/New_York", zoneStatenNY );

    // clang-format off
    ASSERT_EQ( NY.abbrev_s( _ct( 1883, 11, 18, 16, 59, 59 ) ), "LMT" );
    ASSERT_EQ( NY.abbrev_s( _ct( 1883, 11, 18, 17,  0,  0 ) ), "EST" );
    ASSERT_EQ( NY.offset_s( _ct( 1883, 11, 18, 16, 59, 59 ) ), -17762 );
    ASSERT_EQ( NY.offset_s( _ct( 1883, 11, 18, 17,  0,  0 ) ), -18000 );

    ASSERT_EQ( DT{ NY.to_local_s( _ct( 1883, 11, 18, 16, 59, 59 ) ) }, DT::civil( 1883, 11, 18, 12,  3, 57 ) );
    ASSERT_EQ( DT{ NY.to_local_s( _ct( 1883, 11, 18, 17,  0,  0 ) ) }, DT::civil( 1883, 11, 18, 12,  0,  0 ) );

    // 11:59:59 AM is the last local time before America/New_York switched to standard railway time, in 1883
    ASSERT_EQ( DT{ NY.to_sys_s( _ct( 1883, 11, 18, 11, 59, 59 ), E ) }, DT::civil( 1883, 11, 18, 16, 56,  1 ) );
    ASSERT_EQ( DT{ NY.to_sys_s( _ct( 1883, 11, 18, 11, 59, 59 ), L ) }, DT::civil( 1883, 11, 18, 16, 56,  1 ) );

    // local times from 12pm to 12:03:57 are ambiguous
    ASSERT_EQ( DT{ NY.to_sys_s( _ct( 1883, 11, 18, 12,  0,  0 ), E ) }, DT::civil( 1883, 11, 18, 16, 56,  2 ) );
    ASSERT_EQ( DT{ NY.to_sys_s( _ct( 1883, 11, 18, 12,  0,  0 ), L ) }, DT::civil( 1883, 11, 18, 17,  0,  0 ) );
    ASSERT_EQ( DT{ NY.to_sys_s( _ct( 1883, 11, 18, 12,  3, 57 ), E ) }, DT::civil( 1883, 11, 18, 16, 59, 59 ) );
    ASSERT_EQ( DT{ NY.to_sys_s( _ct( 1883, 11, 18, 12,  3, 57 ), L ) }, DT::civil( 1883, 11, 18, 17,  3, 57 ) );

    // from 12:03:58 local time is unambiguous again
    ASSERT_EQ( DT{ NY.to_sys_s( _ct( 1883, 11, 18, 12,  3, 58 ), E ) }, DT::civil( 1883, 11, 18, 17,  3, 58 ) );
    ASSERT_EQ( DT{ NY.to_sys_s( _ct( 1883, 11, 18, 12,  3, 58 ), L ) }, DT::civil( 1883, 11, 18, 17,  3, 58 ) );
    // clang-format on

    // clang-format off
    /*
    America/Phoenix moved the clocks forward 28 minutes and 18 seconds
    at 7pm UTC (what became 12:00 local time) on 1883 Nov 18

    Zone America/Phoenix    -7:28:18   -    LMT    1883 Nov 18 19:00u
                            -7:00      US   M%sT   1944 Jan  1  0:01
                            ...

    From zdump:

    America/Phoenix  Sun Nov 18 18:59:59 1883 UT = Sun Nov 18 11:31:41 1883 LMT isdst=0 gmtoff=-26898
    America/Phoenix  Sun Nov 18 19:00:00 1883 UT = Sun Nov 18 12:00:00 1883 MST isdst=0 gmtoff=-25200
    */
    // clang-format on
    auto zoneStatePhoenix = file.getZoneStates( "America/Phoenix", 2500 );

    /// America/Phoenix
    auto PH = TimeZone( "America/Phoenix", zoneStatePhoenix );

    // clang-format off
    ASSERT_EQ( PH.offset_s( _ct( 1883, 11, 18, 18, 59, 59 ) ), -26898 );
    ASSERT_EQ( PH.offset_s( _ct( 1883, 11, 18, 19,  0,  0 ) ), -25200 );

    ASSERT_EQ( PH.abbrev_s( _ct( 1883, 11, 18, 18, 59, 59 ) ), "LMT" );
    ASSERT_EQ( PH.abbrev_s( _ct( 1883, 11, 18, 19,  0,  0 ) ), "MST" );

    ASSERT_EQ( DT{ PH.to_local_s( _ct( 1883, 11, 18, 18, 59, 59 ) ) }, DT::civil( 1883, 11, 18, 11, 31, 41 ) );
    ASSERT_EQ( DT{ PH.to_local_s( _ct( 1883, 11, 18, 19,  0,  0 ) ) }, DT::civil( 1883, 11, 18, 12,  0,  0 ) );

    ASSERT_EQ( DT{ PH.to_sys_s( _ct( 1883, 11, 18, 11, 31, 41 ), E ) }, DT::civil( 1883, 11, 18, 18, 59, 59 ) );
    ASSERT_EQ( DT{ PH.to_sys_s( _ct( 1883, 11, 18, 11, 31, 41 ), L ) }, DT::civil( 1883, 11, 18, 18, 59, 59 ) );

    // All of these times are non-existent local times, and all of them map to 7pm UTC
    ASSERT_EQ( DT{ PH.to_sys_s( _ct( 1883, 11, 18, 11, 31, 42 ), E ) }, DT::civil( 1883, 11, 18, 19,  0,  0 ) );
    ASSERT_EQ( DT{ PH.to_sys_s( _ct( 1883, 11, 18, 11, 31, 42 ), L ) }, DT::civil( 1883, 11, 18, 19,  0,  0 ) );
    ASSERT_EQ( DT{ PH.to_sys_s( _ct( 1883, 11, 18, 11, 59, 59 ), E ) }, DT::civil( 1883, 11, 18, 19,  0,  0 ) );
    ASSERT_EQ( DT{ PH.to_sys_s( _ct( 1883, 11, 18, 11, 59, 59 ), L ) }, DT::civil( 1883, 11, 18, 19,  0,  0 ) );

    ASSERT_EQ( DT{ PH.to_sys_s( _ct( 1883, 11, 18, 12,  0,  0 ), E ) }, DT::civil( 1883, 11, 18, 19,  0,  0 ) );
    ASSERT_EQ( DT{ PH.to_sys_s( _ct( 1883, 11, 18, 12,  0,  0 ), L ) }, DT::civil( 1883, 11, 18, 19,  0,  0 ) );
    // clang-format on
}

TEST( vtz, all_timezones ) {
    COUNT_ASSERTIONS();

    using sec      = std::chrono::seconds;
    auto const& fp = "build/data/tzdata/tzdata.zi";

    constexpr sysseconds_t startT = daysToSeconds( resolveCivil( 1600, 1, 1 ) );
    constexpr sysseconds_t endT   = daysToSeconds( resolveCivil( 2400, 1, 1 ) );


    fmt::println( "Start time: {}", utcToString( startT ) );
    fmt::println( "End time:   {}", utcToString( endT ) );

    auto content = readFile( fp );
    auto file    = parseTZData( content, fp );

    auto zones = file.listZones();

    ADD_CONTEXT( "Checking zones within file", fp );
    for( auto const& zone : zones )
    {
        ADD_CONTEXT( "Checking zone", zone );

        if( zone == "Factory" ) continue;

        TEST_LOG.logGood( "Testing", zone );

        auto entries = getEntries( zone, sys_seconds( sec( startT ) ), sys_seconds( sec( endT ) ) );

        auto zoneStates = file.getZoneStates( zone, 2400 );

        for( size_t i = 0; i < entries.size(); ++i )
        {
            auto const& e = entries[i];

            ADD_CONTEXT( "Checking time period within zone",
                i,
                localToString( e.begin, e.offset, e.fixAbbr<15>() ),
                localToString( e.end - 1, e.offset, e.fixAbbr<15>() ) );

            // Check that the zone matches from begin to end-1
            auto const& s1 = zoneStates.getState( e.begin );
            auto const& s2 = zoneStates.getState( e.end - 1 );

            auto const& off1    = zoneStates.walloff( e.begin );
            auto const& stdoff1 = zoneStates.stdoff( e.begin );
            auto const& abbr1   = zoneStates.abbr( e.begin );

            auto const& off2    = zoneStates.walloff( e.end - 1 );
            auto const& stdoff2 = zoneStates.stdoff( e.end - 1 );
            auto const& abbr2   = zoneStates.abbr( e.end - 1 );

            ASSERT_EQ_QUIET( &off1, &off2 );
            ASSERT_EQ_QUIET( &stdoff1, &stdoff2 );
            ASSERT_EQ_QUIET( &abbr1, &abbr2 );

            ASSERT_EQ_QUIET( s1.walloff, e.offset );
            ASSERT_EQ_QUIET( s1.save(), e.save );
            ASSERT_EQ_QUIET( s1.abbr.sv(), e.abbr );

            ASSERT_EQ_QUIET( s2.save(), e.save );
            ASSERT_EQ_QUIET( s2.walloff, e.offset );
            ASSERT_EQ_QUIET( s2.abbr.sv(), e.abbr );
        }
        // Load zones from Howard Hinnant TZ library
    }
}


TEST( vtz, TimeZone ) {
    COUNT_ASSERTIONS();

    auto const& fp = "build/data/tzdata/tzdata.zi";

    constexpr sysseconds_t startT = daysToSeconds( resolveCivil( 1800, 1, 1 ) );
    constexpr sysseconds_t endT   = daysToSeconds( resolveCivil( 2900, 1, 1 ) );


    fmt::println( "Start time: {}", utcToString( startT ) );
    fmt::println( "End time:   {}", utcToString( endT ) );

    auto content = readFile( fp );
    auto file    = parseTZData( content, fp );

    auto zones = file.listZones();

    ASSERT_GT( zones.size(), 0 );

    {
        auto itNY = std::find( zones.begin(), zones.end(), "America/New_York" );
        ASSERT_TRUE( itNY != zones.end() );

        // Move NY to the front, so we test it first
        std::swap( *itNY, *zones.begin() );
    }

    for( auto const& zone : zones )
    {
        fmt::println( "Checking transitions within zone {}", zone );

        ADD_CONTEXT( "Checking timezone", zone );
        // Get all the zone states
        auto zoneStates = file.getZoneStates( zone, 2900 );
        span tt         = zoneStates.walloffTrans_;
        span TTabbr     = zoneStates.abbrTrans_;

        auto tz = TimeZone( zone, zoneStates );

        // Check that all the abbreviations are correct
        {
            auto abbrPrev = zoneStates.abbrInitial_;
            for( size_t zi = 0; zi < TTabbr.size(); ++zi )
            {
                auto T    = TTabbr[zi];
                auto abbr = zoneStates.abbr_[zi];
                ADD_CONTEXT(
                    "Checking times", DT{ T }, DT{ T - 1 }, DT{ zoneStates.safeCycleTime } );
                ASSERT_EQ_QUIET( tz.abbrev_s( T - 1 ), zoneStates.abbrToSV( abbrPrev ) );
                ASSERT_EQ_QUIET( tz.abbrev_s( T ), zoneStates.abbrToSV( abbr ) );
                abbrPrev = abbr;
            }
        }

        // Check that translations to and from local time are correct, and that
        // calculated timezone offsets are correct
        i32 off0 = zoneStates.walloffInitial_;
        for( size_t zi = 0; zi < tt.size(); ++zi )
        {
            auto const& t             = zoneStates.walloffTrans_[zi];
            auto const& off           = zoneStates.walloff_[zi];
            auto        localTimePre  = t - 1 + off0;
            auto        localTimePost = t + off;
            // Quantity that we're changing the offset
            auto delta = off - off0;

            ADD_CONTEXT( "Checking transition",
                DT{ t - 1 },
                DT{ t },
                DT{ localTimePre },
                DT{ localTimePost },
                off0,
                off,
                delta,
                zi );


            ASSERT_EQ_QUIET( tz.offset_s( t - 1 ), off0 );
            ASSERT_EQ_QUIET( tz.offset_s( t ), off );

            ASSERT_EQ_QUIET( DT{ tz.to_local_s( t - 1 ) }, DT{ localTimePre } );
            ASSERT_EQ_QUIET( DT{ tz.to_local_s( t ) }, DT{ localTimePost } );

            // these should not be equal (if they are, we didn't filter transitions correctly)
            ASSERT_NE( localTimePre, localTimePost );
            if( localTimePre < localTimePost )
            {
                // We move the clocks forward, resulting in a set of nonexistent
                // local times between localTimePre and localTimePost

                ASSERT_EQ_QUIET( DT{ tz.to_sys_s( localTimePost, E ) }, DT{ t } );
                ASSERT_EQ_QUIET( DT{ tz.to_sys_s( localTimePost, L ) }, DT{ t } );

                // ASSERT_EQ_QUIET( DT{ tz.toUTC( localTimePre, E ) }, DT{ t - 1 } );
                ASSERT_EQ_QUIET( DT{ tz.to_sys_s( localTimePre, L ) }, DT{ t - 1 } );

                for( auto i = localTimePre + 1; i < localTimePost; ++i )
                {
                    // All of these times are nonexistent, and they should map to the same UTC time
                    ASSERT_EQ_QUIET( DT{ tz.to_sys_s( i, E ) }, DT{ t } );
                    ASSERT_EQ_QUIET( DT{ tz.to_sys_s( i, L ) }, DT{ t } );
                }
            }
            else
            {
                // This delta should be negative, since we moved the clocks back
                ASSERT_LT( delta, 0 );

                // The first ambiguous local time is whenever we move the clocks back to
                // (eg, at 1:59:59am, NY moves the clock back to 1am, when daylight savings time
                // ends)
                auto ambigStartLocal = localTimePost;
                // the last ambiguous local time is whenever we moved the clocks back from
                // (eg, this would be 1:59:59am)
                auto ambigEndLocal = localTimePre + 1;

                // The previous unambiguous time was 1 before the start of the ambiguous times
                auto prevUnambiguousTime = ambigStartLocal - 1;
                // The next unambiguous time is when ambiguity ends
                auto nextUnambiguousTime = ambigEndLocal;

                // We need to actually have ambiguous times
                ASSERT_LT( ambigStartLocal, ambigEndLocal );

                auto ambigStartUTC = t + delta;
                auto ambigEndUTC   = t - delta;

                // The previous time that was unambiguous was whenever we moved the clocks back to


                ADD_CONTEXT( "Checking ambiguous local times resulting from moving the clock back",
                    DT{ ambigStartLocal },
                    DT{ ambigEndLocal },
                    DT{ ambigStartUTC },
                    DT{ ambigEndUTC } );

                // We moved the clocks back, resulting in an ambiguous time

                // These times right before and after should not be ambiguous
                ASSERT_EQ_QUIET(
                    DT{ tz.to_sys_s( prevUnambiguousTime, E ) }, DT{ ambigStartUTC - 1 } );
                ASSERT_EQ_QUIET(
                    DT{ tz.to_sys_s( prevUnambiguousTime, L ) }, DT{ ambigStartUTC - 1 } );
                ASSERT_EQ_QUIET( DT{ tz.to_sys_s( nextUnambiguousTime, E ) }, DT{ ambigEndUTC } );
                ASSERT_EQ_QUIET( DT{ tz.to_sys_s( nextUnambiguousTime, L ) }, DT{ ambigEndUTC } );

                // Recall that we have established that localTimePost < localTimePre -
                // right now we're testing the scenario where we move the clocks back

                auto n = ambigEndLocal - ambigStartLocal;
                ASSERT_LT( 0, n );
                for( i64 i = 0; i < n; ++i )
                {
                    // Check local times, choosing latest
                    ASSERT_EQ_QUIET( DT{ tz.to_sys_s( ambigStartLocal + i, L ) }, DT{ t + i } );
                }
                for( i64 i = 0; i < n; ++i )
                {
                    auto localT    = ambigStartLocal + i;
                    auto prevBlock = TimeZone::Impl::local_block_s( tz, localT - 1 );
                    auto block     = TimeZone::Impl::local_block_s( tz, localT );

                    ADD_CONTEXT( "Check local -> UTC, choosing earliest",
                        DT{ localT },
                        block.t,
                        DT{ block.t },
                        prevBlock.t,
                        DT{ prevBlock.t },
                        block.hi(),
                        block.lo(),
                        localT,
                        TimeZone::Impl::getTT( tz ).blockSize(),
                        TimeZone::Impl::getTT( tz ).blockStartT( localT ),
                        TimeZone::Impl::getTT( tz ).blockEndT( localT ),
                        DT{ TimeZone::Impl::getTT( tz ).blockStartT( localT ) },
                        DT{ TimeZone::Impl::getTT( tz ).blockEndT( localT ) } );
                    ASSERT_EQ_QUIET( DT{ tz.to_sys_s( localT, E ) }, DT{ t + i + delta } );
                }
            }
            off0 = off;
        }

        if( tt.size() > 0 )
        {
            int64_t T0          = daysToSeconds( resolveCivil( 1800, 1, 1 ) );
            int64_t TMax        = daysToSeconds( resolveCivil( 2899, 1, 1 ) );
            auto    currentOff  = zoneStates.walloffInitial_;
            auto    currentAbbr = zoneStates.abbrInitial_;
            size_t  zi          = 0;
            size_t  ai          = 0;
            // loop through times in 1h intervals until we get to the end of the states we
            // calculated
            for( ; T0 < TMax; T0 += 3600 )
            {
                if( T0 >= tt.value_or( zi, INT64_MAX ) ) currentOff = zoneStates.walloff_[zi++];
                if( T0 >= TTabbr.value_or( ai, INT64_MAX ) ) currentAbbr = zoneStates.abbr_[ai++];

                ASSERT_EQ_QUIET( AbbrBlock{ TimeZone::Impl::getAbbrTable( tz ).abbr_block_s( T0 ) },
                    currentAbbr );
                ASSERT_EQ_QUIET( tz.offset_s( T0 ), currentOff );
            }
        }
    }
}

static std::string dateStr( sysseconds_t t ) {
    return fmt::to_string( sys_seconds( seconds( t ) ) );
}
namespace date {
    std::string format_as( date::local_seconds s ) {
        return dateStr( s.time_since_epoch().count() );
    }
} // namespace date

struct TimeT {
    sysseconds_t sec;
    TimeT( date::local_seconds t )
    : sec( t.time_since_epoch().count() ) {}
    TimeT( vtz::local_seconds t )
    : sec( t.time_since_epoch().count() ) {}
    TimeT( sys_seconds t )
    : sec( t.time_since_epoch().count() ) {}

    bool operator==( TimeT const& rhs ) const noexcept { return sec == rhs.sec; }
    bool operator!=( TimeT const& rhs ) const noexcept { return sec != rhs.sec; }
};

std::string format_as( TimeT t ) { return dateStr( t.sec ); }


TEST( vtz, SelfTest ) {
    COUNT_ASSERTIONS();
    constexpr sysseconds_t startT = daysToSeconds( resolveCivil( 1800, 1, 1 ) );
    constexpr sysseconds_t endT   = daysToSeconds( resolveCivil( 2900, 1, 1 ) );
    constexpr sysseconds_t _2370  = resolveCivilTime( 2370, 1, 1, 0, 0, 0 );

    auto const& fp = "build/data/tzdata/tzdata.zi";

    auto content = readFile( fp );
    auto file    = parseTZData( content, fp );

    auto const& cache = tzdb_cache();

    for( auto const& zone : ALL_ZONES )
    {
        ADD_CONTEXT( "Testing zone", zone );
        fmt::println( "Testing zone {}", zone );

        auto  s1  = file.getZoneStates( zone, 2901 );
        auto  tz  = TimeZone( zone, s1 );
        auto  s2  = cache.computeStates( zone );
        auto& tz2 = *locate_zone( zone );

        auto tt1 = s1.getTransitions();
        auto tt2 = s2.getTransitions();

        // if tt1.size() is not 0, tt2.size() must be greater than 0 as well
        ASSERT_TRUE( tt1.size() == 0 || tt2.size() > 0 );

        size_t count = std::min( tt1.size(), tt2.size() );

        for( size_t i = 0; i < count; ++i )
        {
            ASSERT_EQ_QUIET( tt1[i].when, tt2[i].when );
            ASSERT_EQ_QUIET( tt1[i].state.stdoff, tt2[i].state.stdoff );
            ASSERT_EQ_QUIET( tt1[i].state.walloff, tt2[i].state.walloff );
            ASSERT_EQ_QUIET( tt1[i].state.abbr.sv(), tt2[i].state.abbr.sv() );
        }

        if( !tt1.empty() )
        {
            ASSERT_TRUE( count > 0 );
            if( tt1.back().when >= _2370 )
            {
                // Both should end after 2370 - this implies that we have some sort of cyclic rule
                ASSERT_GT( tt2.back().when, _2370 );
            }
        }


        auto t0 = startT;
        while( t0 < endT )
        {
            ADD_CONTEXT( "Testing time",
                toSys( t0 ),
                toSys( tz.to_sys_s( t0, E ) ),
                toSys( tz2.to_sys_s( t0, E ) ),
                toSys( tz.to_sys_s( t0, L ) ),
                toSys( tz2.to_sys_s( t0, L ) ) );
            ASSERT_EQ_QUIET( tz.offset_s( t0 ), tz2.offset_s( t0 ) );
            ASSERT_EQ_QUIET( tz.to_local_s( t0 ), tz2.to_local_s( t0 ) );
            ASSERT_EQ_QUIET( tz.to_sys_s( t0, E ), tz2.to_sys_s( t0, E ) );
            ASSERT_EQ_QUIET( tz.to_sys_s( t0, L ), tz2.to_sys_s( t0, L ) );
            t0 += 3600;
        }
    }
}
TEST( vtz, TimeZoneFuzz ) {
    COUNT_ASSERTIONS();
    constexpr sysseconds_t startT = daysToSeconds( resolveCivil( 1800, 1, 1 ) );
    constexpr sysseconds_t endT   = daysToSeconds( resolveCivil( 2900, 1, 1 ) );
    auto const&            fp     = "build/data/tzdata/tzdata.zi";

    auto content = readFile( fp );
    auto file    = parseTZData( content, fp );
    auto zones   = file.listZones();
    auto rng     = std::mt19937_64{};
    auto dist    = std::uniform_int_distribution<sysseconds_t>( startT, endT );

    ASSERT_GT( zones.size(), 0 );

    constexpr size_t NUM_RANDOM_SAMPLES     = 10000;
    constexpr size_t NUM_RANDOM_SAMPLES_STR = 1000;

    for( auto const& zone : zones )
    {
        // 'Factory' is not loaded/handled by Howard Hinnant's timezone library
        if( zone == "Factory" ) continue;

        auto states    = file.getZoneStates( zone, 2901 );
        auto tz        = TimeZone( zone, states );
        auto tzHinnant = date::locate_zone( zone );
        auto zoneInfo  = getSysInfo( zone, startT, endT );
        fmt::println( "Testing {:>5} sys_info entries from {}", zoneInfo.size(), zone );

        ADD_CONTEXT( "Zone information",
            zone,
            states.stdoffInitial_,
            states.stdoff_,
            toSysVec( states.stdoffTrans_ ) );

        for( date::sys_info const& info : zoneInfo )
        {
            auto Tbegin     = info.begin.time_since_epoch().count();
            auto Tend       = info.end.time_since_epoch().count();
            auto offset     = info.offset.count();
            auto Tmid       = Tbegin / 2 + Tend / 2;
            auto zoneStdoff = seconds( info.offset - info.save ).count();

            ADD_CONTEXT( "Times",
                info.begin,
                info.end,
                info.abbrev,
                info.save,
                info.offset,
                sys_seconds( seconds( Tmid ) ),
                zoneStdoff );

            ASSERT_LT( Tbegin, Tend );
            ASSERT_LT( Tbegin, Tmid );
            ASSERT_LT( Tmid, Tend );
            ASSERT_EQ_QUIET( tz.offset_s( Tbegin ), offset );
            ASSERT_EQ_QUIET( tz.offset_s( Tmid ), offset );
            ASSERT_EQ_QUIET( tz.offset_s( Tend - 1 ), offset );
            ASSERT_EQ_QUIET( tz.to_local_s( Tbegin ), Tbegin + offset );
            ASSERT_EQ_QUIET( tz.to_local_s( Tmid ), Tmid + offset );
            ASSERT_EQ_QUIET( tz.to_local_s( Tend - 1 ), Tend + offset - 1 );
            ASSERT_EQ_QUIET( tz.abbrev_s( Tbegin ), info.abbrev );
            ASSERT_EQ_QUIET( tz.abbrev_s( Tmid ), info.abbrev );
            ASSERT_EQ_QUIET( tz.abbrev_s( Tend - 1 ), info.abbrev );
            ASSERT_EQ_QUIET( tz.stdoff_s( Tbegin ), zoneStdoff );
            ASSERT_EQ_QUIET( tz.stdoff_s( Tmid ), zoneStdoff );
            ASSERT_EQ_QUIET( tz.stdoff_s( Tend - 1 ), zoneStdoff );
            ASSERT_EQ_QUIET( seconds( tz.save_s( Tbegin ) ), seconds( info.save ) );
            ASSERT_EQ_QUIET( seconds( tz.save_s( Tmid ) ), seconds( info.save ) );
            ASSERT_EQ_QUIET( seconds( tz.save_s( Tend - 1 ) ), seconds( info.save ) );
        }

        for( size_t zi = 1; zi < zoneInfo.size(); zi++ )
        {
            date::sys_info const& prev = zoneInfo[zi - 1];
            date::sys_info const& info = zoneInfo[zi];

            // Current time at which sys_info applies
            auto sysT = info.begin.time_since_epoch().count();
            // Last time that previous sys_info applied
            auto sysT0      = sysT - 1;
            auto offset     = info.offset.count();
            auto prevOffset = prev.offset.count();

            auto localPrev = sysT + prevOffset - 1;
            auto localT    = sysT + offset;

            ADD_CONTEXT( "Testing local -> UTC",
                dateStr( sysT ),
                dateStr( sysT0 ),
                offset,
                prevOffset,
                dateStr( localPrev ),
                dateStr( localT ),
                dateStr( tz.to_local_s( sysT - 1 ) ),
                tzHinnant->to_local( toSys( sysT - 1 ) ),
                dateStr( tz.to_local_s( sysT ) ),
                tzHinnant->to_local( toSys( sysT ) ) );

            if( localPrev < localT )
            {
                // If this is the case, we're moving the clock forward
                // (eg, moving the clock from 1:59:59am to 3am)
                ASSERT_EQ_QUIET( tz.to_local_s( sysT - 1 ), localPrev );
                ASSERT_EQ_QUIET( tz.to_local_s( sysT ), localT );
                ASSERT_EQ_QUIET( tz.to_sys_s( localPrev, choose::earliest ), sysT - 1 );
                ASSERT_EQ_QUIET( tz.to_sys_s( localPrev, choose::latest ), sysT - 1 );
                ASSERT_EQ_QUIET( tz.to_sys_s( localT, choose::earliest ), sysT );
                ASSERT_EQ_QUIET( tz.to_sys_s( localT, choose::latest ), sysT );
            }
            else
            {
                ASSERT_EQ_QUIET( tz.to_sys_s( localT, choose::latest ), sysT );
                ASSERT_EQ_QUIET( tz.to_sys_s( localPrev, choose::earliest ), sysT - 1 );
            }
        }

        for( size_t i = 0; i < NUM_RANDOM_SAMPLES; ++i )
        {
            auto T       = dist( rng );
            auto llTV    = local_seconds( seconds( T ) );
            auto llTH    = date::local_seconds( seconds( T ) );
            auto ssT     = sys_seconds( seconds( T ) );
            auto sysInfo = tzHinnant->get_info( ssT );

            ASSERT_EQ_QUIET( TimeT( tz.to_local( ssT ) ), TimeT( tzHinnant->to_local( ssT ) ) );
            ASSERT_EQ_QUIET( tz.to_sys( llTV, E ), tzHinnant->to_sys( llTH, DE ) );
            ASSERT_EQ_QUIET( tz.to_sys( llTV, L ), tzHinnant->to_sys( llTH, DL ) );
            ASSERT_EQ_QUIET( tz.abbrev_s( T ), sysInfo.abbrev );
            ASSERT_EQ_QUIET( tz.save_s( T ), seconds( sysInfo.save ).count() );
        }

        for( size_t i = 0; i < NUM_RANDOM_SAMPLES_STR; ++i )
        {
            auto T = sys_seconds( seconds( dist( rng ) ) );
            ASSERT_EQ_QUIET( tz.format_compact( T ),
                date::format( "%Y%m%d %H%M%S %Z", date::make_zoned( tzHinnant, T ) ) );
            ASSERT_EQ_QUIET( tz.format( T, '/', 'T', ' ' ),
                date::format( "%Y/%m/%dT%H:%M:%S %Z", date::make_zoned( tzHinnant, T ) ) );
        }
    }
}


TEST( vtz, TimeZoneToString ) {
    COUNT_ASSERTIONS();
    constexpr sysseconds_t startT = daysToSeconds( resolveCivil( 1800, 1, 1 ) );
    constexpr sysseconds_t endT   = daysToSeconds( resolveCivil( 2900, 1, 1 ) );
    auto const&            fp     = "build/data/tzdata/tzdata.zi";

    auto content = readFile( fp );
    auto file    = parseTZData( content, fp );
    auto zones   = file.listZones();
    auto rng     = std::mt19937_64{};
    auto dist    = std::uniform_int_distribution<sysseconds_t>( startT, endT );

    ASSERT_GT( zones.size(), 0 );

    constexpr size_t NUM_RANDOM_SAMPLES_STR = 1000;

    for( auto const& zone : zones )
    {
        // 'Factory' is not loaded/handled by Howard Hinnant's timezone library
        if( zone == "Factory" ) continue;

        auto states    = file.getZoneStates( zone, 2901 );
        auto tz        = TimeZone( zone, states );
        auto tzHinnant = date::locate_zone( zone );

        ADD_CONTEXT( "Zone information", zone );

        for( size_t i = 0; i < NUM_RANDOM_SAMPLES_STR; ++i )
        {
            auto T = sys_seconds( seconds( dist( rng ) ) );
            ASSERT_EQ_QUIET( tz.format_compact( T ),
                date::format( "%Y%m%d %H%M%S %Z", date::make_zoned( tzHinnant, T ) ) );
            ASSERT_EQ_QUIET( tz.format( T, '/', 'T', '-' ),
                date::format( "%Y/%m/%dT%H:%M:%S-%Z", date::make_zoned( tzHinnant, T ) ) );
        }
    }
}
