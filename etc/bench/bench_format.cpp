#include <vtz/format.h>
#include <vtz/tz.h>

#include <fmt/chrono.h>
#include <fmt/format.h>

#include "absl_helper.h"

#include <date/date.h>
#include <date/tz.h>


BENCH( hinnant_format, state ) {
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


BENCH( absl_format, state ) {
    auto           tt = to_absl_time( random_times( COUNT, 1900, 2100 ) );
    absl::TimeZone tz;
    absl::LoadTimeZone( "America/New_York", &tz );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            absl::FormatTime( "%F %T %Z", tt[i % COUNT], tz ) );
        ++i;
    }
}


BENCH( libfmt_format_utc, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( fmt::format( "{:%F %T %Z}", tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( libfmt_format_to_utc, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    char   buff[256];
    for( auto _ : state )
    {
        auto result = fmt::format_to( buff, "{:%F %T %Z}", tt[i % COUNT] );
        benchmark::DoNotOptimize( result );
        ++i;
    }
}


#if HAS_CHRONO_TIMEZONE

BENCH( chrono_format, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = std::chrono::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( std::format(
            "{:%F %T %Z}", std::chrono::zoned_time{ tz, tt[i % COUNT] } ) );
        ++i;
    }
}

BENCH( chrono_format_to, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = std::chrono::locate_zone( "America/New_York" );
    size_t i  = 0;
    char   buffer[256];
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( std::format_to( buffer,
            "{:%F %T %Z}",
            std::chrono::zoned_time{ tz, tt[i % COUNT] } ) );
        ++i;
    }
}

#endif


BENCH( vtz_format, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->format( "%F %T %Z", tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_format_to_nanos, state ) {
    auto tt = to_chrono<nanos>( random_times( COUNT, 1900, 2100, 1000000000 ) );
    auto tz = vtz::locate_zone( "America/New_York" );
    size_t i = 0;
    char   buff[64];
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->format_to( "%F %T %Z", tt[i % COUNT], buff, sizeof( buff ) ) );
        ++i;
    }
}


BENCH( vtz_format_to, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    char   buff[64];
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->format_to( "%F %T %Z", tt[i % COUNT], buff, sizeof( buff ) ) );
        ++i;
    }
}
