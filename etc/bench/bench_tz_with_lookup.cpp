#include <vtz/tz.h>

#include "absl_helper.h"

#include <date/date.h>
#include <date/tz.h>


BENCH( hinnant_to_local_with_lookup, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( date::locate_zone( "America/New_York" )
                ->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( hinnant_to_sys_latest_with_lookup, state ) {
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


BENCH( hinnant_to_sys_earliest_with_lookup, state ) {
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


BENCH( absl_to_local_with_lookup, state ) {
    auto   tt = to_absl_time( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        absl::TimeZone tz;
        absl::LoadTimeZone( "America/New_York", &tz );
        benchmark::DoNotOptimize( tz.At( tt[i % COUNT] ) );
        ++i;
    }
}


#if HAS_CHRONO_TIMEZONE
BENCH( chrono_to_local_with_lookup, state ) {
    using std::chrono::locate_zone;
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            locate_zone( "America/New_York" )->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( chrono_to_sys_latest_with_lookup, state ) {
    using std::chrono::choose;
    using std::chrono::local_seconds;
    using std::chrono::locate_zone;

    auto tt = to_chrono<local_seconds>( random_times( COUNT, 1900, 2100 ) );

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], choose::latest ) );
        ++i;
    }
}


BENCH( chrono_to_sys_earliest_with_lookup, state ) {
    using std::chrono::choose;
    using std::chrono::local_seconds;
    using std::chrono::locate_zone;

    auto   tt = to_chrono<local_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], choose::earliest ) );
        ++i;
    }
}
#endif


BENCH( vtz_to_local_with_lookup, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::locate_zone( "America/New_York" )->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_to_sys_latest_with_lookup, state ) {
    auto tt
        = to_chrono<vtz::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], vtz::choose::latest ) );
        ++i;
    }
}


BENCH( vtz_to_sys_earliest_with_lookup, state ) {
    auto tt
        = to_chrono<vtz::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], vtz::choose::earliest ) );
        ++i;
    }
}
