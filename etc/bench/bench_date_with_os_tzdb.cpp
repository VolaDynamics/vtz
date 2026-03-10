// This file is compiled with -Ddate=date_os_tzdb and -DUSE_OS_TZDB=1,
// so all date:: references resolve to date_os_tzdb:: (a separately compiled
// copy of the Hinnant date library that reads timezone data from the OS).

#include <vtz/format.h>
#include <vtz/parse.h>
#include <vtz/tz.h>

#include <date/date.h>
#include <date/tz.h>

#include "bench_common.h"

#include <sstream>


BENCH( to_local, date_os_tzdb, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( to_sys_latest, date_os_tzdb, state ) {
    auto tt
        = to_chrono<date::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], date::choose::latest ) );
        ++i;
    }
}


BENCH( to_sys_earliest, date_os_tzdb, state ) {
    auto tt
        = to_chrono<date::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], date::choose::earliest ) );
        ++i;
    }
}


BENCH( to_sys, date_os_tzdb, state ) {
    auto tt = to_chrono<date::local_seconds>(
        random_times( COUNT, 1900, 2100, 1, is_unique( "America/New_York" ) ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_sys( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( to_local_with_lookup, date_os_tzdb, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( date::locate_zone( "America/New_York" )
                ->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( to_sys_latest_with_lookup, date_os_tzdb, state ) {
    auto tt
        = to_chrono<date::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( date::locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], date::choose::latest ) );
        ++i;
    }
}


BENCH( to_sys_earliest_with_lookup, date_os_tzdb, state ) {
    auto tt
        = to_chrono<date::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( date::locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], date::choose::earliest ) );
        ++i;
    }
}


BENCH( locate_zone, date_os_tzdb, state ) {
    auto zones = random_zones( COUNT );

    for( auto z : ZONES_FOR_BENCH ) { (void)date::locate_zone( z ); }

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            date::locate_zone( zones[( i >> 8 ) % COUNT] ) );
        ++i;
    }
}


BENCH( locate_rand, date_os_tzdb, state ) {
    auto zones = random_zones( COUNT );

    for( auto z : ZONES_FOR_BENCH ) { (void)date::locate_zone( z ); }

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( date::locate_zone( zones[i % COUNT] ) );
        ++i;
    }
}


BENCH( format, date_os_tzdb, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            date::format( "%F %T %Z", date::make_zoned( tz, tt[i % COUNT] ) ) );
        ++i;
    }
}


BENCH( parse_date, date_os_tzdb, state ) {
    auto dd = random_values( COUNT,
        vtz::resolve_civil( 1900 ),
        vtz::resolve_civil( 2100 ),
        []( sysdays_t days ) { return vtz::format_d( "%F", days ); } );

    std::istringstream ss;
    ss.exceptions( std::ios::failbit );
    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        ss.clear();
        ss.str( entry );
        date::year_month_day ymd;
        ss >> date::parse( "%F", ymd );
        benchmark::DoNotOptimize( ymd );
        ++i;
    }
}


BENCH( parse_time, date_os_tzdb, state ) {
    auto dd = random_values( COUNT,
        vtz::resolve_civil_time( 1900, 1, 1, 0, 0, 0 ),
        vtz::resolve_civil_time( 2100, 1, 1, 0, 0, 0 ),
        []( vtz::sec_t t ) { return vtz::format_s( "%F %T", t ); } );

    std::istringstream ss;
    ss.exceptions( std::ios::failbit );
    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        ss.clear();
        ss.str( entry );
        date::sys_seconds tp;
        date::from_stream( ss, "%F %T", tp );
        benchmark::DoNotOptimize( tp );
        ++i;
    }
}
