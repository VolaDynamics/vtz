#include <string>
#include <vtz/date_types.h>
#include <vtz/files.h>
#include <vtz/strings.h>
#include <vtz/tz_reader.h>

#include <gtest/gtest.h>

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>

#include "vtz_testing.h"
#include <vtz/civil.h>
#include <vtz/tz.h>

#include <date/date.h>
#include <date/tz.h>

using namespace vtz;
using _test_vtz::TEST_LOG;

using sys_seconds = date::sys_seconds;

struct entry {
    string       abbr;
    sysseconds_t begin;
    sysseconds_t end;
    FromUTC      offset;
    RuleSave     save;

    template<size_t N>
    FixStr<N> fixAbbr() const {
        FixStr<N> result{};
        if( abbr.size() > N ) { throw std::runtime_error( "fixAbbr(): abbreviation too big!" ); }
        memcpy( result.buff_, abbr.data(), abbr.size() );
        result.size_ = abbr.size();
        return result;
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
            info.offset.count(),
            info.save.count() * 60,
        } );
        T = info.end;
    }

    return entries;
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
            ASSERT_EQ_QUIET( &s1, &s2 );
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

    using sec      = std::chrono::seconds;
    auto const& fp = "build/data/tzdata/tzdata.zi";

    constexpr sysseconds_t startT = daysToSeconds( resolveCivil( 1600, 1, 1 ) );
    constexpr sysseconds_t endT   = daysToSeconds( resolveCivil( 2900, 1, 1 ) );


    fmt::println( "Start time: {}", utcToString( startT ) );
    fmt::println( "End time:   {}", utcToString( endT ) );

    auto content = readFile( fp );
    auto file    = parseTZData( content, fp );

    auto zones = file.listZones();

    auto checkZone = [&]( std::string const& zone ) {
        fmt::println( "Checking transitions within zone {}", zone );

        ADD_CONTEXT( "Checking timezone", zone );
        // Get all the zone states
        auto        zoneStates = file.getZoneStates( zone, 2900 );
        auto const& tt         = zoneStates.transitions;

        auto tz = TimeZone( zone, zoneStates );

        ZoneState prev = zoneStates.initial;
        for( size_t zi = 0; zi < tt.size(); ++zi )
        {
            auto const& trans = tt[zi];
            ADD_CONTEXT( "Checking transition", utcToString( trans.when ), zi );

            ASSERT_EQ_QUIET( tz.offsetFromUTC( trans.when - 1 ), prev.walloff.off );
            ASSERT_EQ_QUIET( tz.offsetFromUTC( trans.when ), trans.state.walloff.off );
            prev = trans.state;
        }

        if( tt.size() > 0 )
        {
            int64_t T0   = daysToSeconds( resolveCivil( 1700, 1, 1 ) );
            int64_t TMax = tt.back().when;
            auto    st   = zoneStates.initial;
            auto    it   = tt.data();
            // loop through times in 1h intervals until we get to the end of the states we
            // calculated
            for( ; T0 < TMax; T0 += 3600 )
            {
                while( T0 >= it->when )
                {
                    st = it->state;
                    ++it;
                }

                ASSERT_EQ_QUIET( tz.offsetFromUTC( T0 ), st.walloff.off );
            }
        }
    };

    checkZone( "America/Ciudad_Juarez" );
    checkZone( "America/New_York" );
    for( auto const& zone : zones ) { checkZone( zone ); }
}
