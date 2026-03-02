#include <vtz/format.h>
#include <vtz/parse.h>
#include <vtz/tz.h>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "absl_helper.h"

#include <date/date.h>
#include <date/tz.h>


BENCH( hinnant_locate_zone, state ) {
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


BENCH( hinnant_locate_random_zone, state ) {
    auto zones = random_zones( COUNT );

    for( auto z : ZONES_FOR_BENCH ) { (void)date::locate_zone( z ); }

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( date::locate_zone( zones[i % COUNT] ) );
        ++i;
    }
}


BENCH( absl_locate_zone, state ) {
    auto zones = random_zones( COUNT );

    for( auto z : ZONES_FOR_BENCH )
    {
        absl::TimeZone tz;
        absl::LoadTimeZone( z, &tz );
        (void)tz;
    }

    size_t i = 0;
    for( auto _ : state )
    {
        absl::TimeZone tz;
        absl::LoadTimeZone( zones[( i >> 8 ) % COUNT], &tz );
        benchmark::DoNotOptimize( tz );
        ++i;
    }
}


BENCH( absl_locate_random_zone, state ) {
    auto zones = random_zones( COUNT );

    for( auto z : ZONES_FOR_BENCH )
    {
        absl::TimeZone tz;
        absl::LoadTimeZone( z, &tz );
        (void)tz;
    }

    size_t i = 0;
    for( auto _ : state )
    {
        absl::TimeZone tz;
        absl::LoadTimeZone( zones[i % COUNT], &tz );
        benchmark::DoNotOptimize( tz );
        ++i;
    }
}


#if HAS_CHRONO_TIMEZONE
BENCH( chrono_locate_zone, state ) {
    auto zones = random_zones( COUNT );

    for( auto z : ZONES_FOR_BENCH ) { (void)std::chrono::locate_zone( z ); }

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            std::chrono::locate_zone( zones[( i >> 8 ) % COUNT] ) );
        ++i;
    }
}


BENCH( chrono_locate_random_zone, state ) {
    auto zones = random_zones( COUNT );

    for( auto z : ZONES_FOR_BENCH ) { (void)std::chrono::locate_zone( z ); }

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            std::chrono::locate_zone( zones[i % COUNT] ) );
        ++i;
    }
}
#endif


BENCH( vtz_locate_zone, state ) {
    auto zones = random_zones( COUNT );

    for( auto z : ZONES_FOR_BENCH ) { (void)vtz::locate_zone( z ); }

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::locate_zone( zones[( i >> 8 ) % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_locate_random_zone, state ) {
    auto zones = random_zones( COUNT );

    for( auto z : ZONES_FOR_BENCH ) { (void)vtz::locate_zone( z ); }

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::locate_zone( zones[i % COUNT] ) );
        ++i;
    }
}
