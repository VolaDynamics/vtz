
#include <benchmark/benchmark.h>
#include <chrono>
#include <date/date.h>
#include <date/tz.h>
#include <exception>
#include <vector>
#include <vtz/civil.h>
#include <vtz/impl/bit.h>

#include <vtz/format.h>
#include <vtz/parse.h>
#include <vtz/tz.h>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "absl_helper.h"
#include "bench_common.h"


BENCH( hinnant_to_local, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( hinnant_to_sys_latest, state ) {
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


BENCH( hinnant_to_sys_earliest, state ) {
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


BENCH( absl_to_local, state ) {
    auto           tt = to_absl_time( random_times( COUNT, 1900, 2100 ) );
    absl::TimeZone tz;
    absl::LoadTimeZone( "America/New_York", &tz );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz.At( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( absl_to_sys_latest, state ) {
    auto           cs = to_absl_civil( random_times( COUNT, 1900, 2100 ) );
    absl::TimeZone tz;
    absl::LoadTimeZone( "America/New_York", &tz );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz.At( cs[i % COUNT] ).post );
        ++i;
    }
}


BENCH( absl_to_sys_earliest, state ) {
    auto           cs = to_absl_civil( random_times( COUNT, 1900, 2100 ) );
    absl::TimeZone tz;
    absl::LoadTimeZone( "America/New_York", &tz );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz.At( cs[i % COUNT] ).pre );
        ++i;
    }
}


// if std::chrono::locate_zone is available, benchmark std::chrono::time_zone
#if HAS_CHRONO_TIMEZONE
BENCH( chrono_to_local, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = std::chrono::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( chrono_to_sys_latest, state ) {
    using std::chrono::choose;
    using std::chrono::local_seconds;
    using std::chrono::locate_zone;

    auto tt = to_chrono<local_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto tz = locate_zone( "America/New_York" );

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_sys( tt[i % COUNT], choose::latest ) );
        ++i;
    }
}


BENCH( chrono_to_sys_earliest, state ) {
    using std::chrono::choose;
    using std::chrono::local_seconds;
    using std::chrono::locate_zone;

    auto   tt = to_chrono<local_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], choose::earliest ) );
        ++i;
    }
}
#endif


BENCH( vtz_to_local, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_to_sys_latest, state ) {
    auto tt
        = to_chrono<vtz::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], vtz::choose::latest ) );
        ++i;
    }
}


BENCH( vtz_to_sys_earliest, state ) {
    auto tt
        = to_chrono<vtz::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], vtz::choose::earliest ) );
        ++i;
    }
}


BENCH( vtz_to_local_s, state ) {
    auto   tt = random_times( COUNT, 1900, 2100 );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local_s( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_to_sys_latest_s, state ) {
    auto   tt = random_times( COUNT, 1900, 2100 );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys_s( tt[i % COUNT], vtz::choose::latest ) );
        ++i;
    }
}


BENCH( vtz_to_sys_earliest_s, state ) {
    auto   tt = random_times( COUNT, 1900, 2100 );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys_s( tt[i % COUNT], vtz::choose::earliest ) );
        ++i;
    }
}
