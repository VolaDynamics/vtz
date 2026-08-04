#include <benchmark/benchmark.h>
#include <date/date.h>
#include <date/tz.h>
#include <vtz/tz.h>

#include "bench_common.h"

using vtz::choose;
using vtz::local_seconds;
using vtz::sys_seconds_t;
using vtz::time_zone;

/// The time of day used for tick benchmarks: 02:30:00 local. This is
/// deliberately inside the window that DST transitions repeat or skip, so
/// the benchmark exercises the interesting cases rather than avoiding them.
constexpr i64 BENCH_TOD = 18 * 3600;

/// Reference implementation of count_ticks_s built on the standard timezone
/// API (see test_count_ticks.cpp). Benchmarked as the baseline that the
/// optimized table-based implementation is measured against.
static i64 ref_count_ticks( time_zone const* tz, sys_seconds_t T, i64 tod ) {
    auto tick = [&]( i64 day ) -> sys_seconds_t {
        auto local = local_seconds( seconds( day * 86400 + tod ) );
        return tz->to_sys( local, choose::latest ).time_since_epoch().count();
    };
    i64 D = vtz::math::div_floor<86400>( tz->to_local_s( T ) - tod );
    while( tick( D ) >= T ) --D;
    while( tick( D ) < T ) ++D;
    return D;
}


BENCH( count_ticks, vtz, state ) {
    auto        tt = random_times( COUNT, 1900, 2100 );
    auto const* tz = vtz::locate_zone( "America/New_York" );
    size_t      i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->count_ticks_s( tt[i % COUNT], BENCH_TOD ) );
        ++i;
    }
}


BENCH( count_ticks, ref, state ) {
    auto        tt = random_times( COUNT, 1900, 2100 );
    auto const* tz = vtz::locate_zone( "America/New_York" );
    size_t      i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            ref_count_ticks( tz, tt[i % COUNT], BENCH_TOD ) );
        ++i;
    }
}


/// The same reference algorithm on the Hinnant date library, as a
/// cross-library baseline
static i64 date_count_ticks( date::time_zone const* tz, i64 T, i64 tod ) {
    auto tick = [&]( i64 day ) -> i64 {
        auto local = date::local_seconds( seconds( day * 86400 + tod ) );
        return tz->to_sys( local, date::choose::latest )
            .time_since_epoch()
            .count();
    };
    auto local = tz->to_local( sys_seconds( seconds( T ) ) ).time_since_epoch();
    i64  D     = vtz::math::div_floor<86400>( i64( local.count() ) - tod );
    while( tick( D ) >= T ) --D;
    while( tick( D ) < T ) ++D;
    return D;
}


BENCH( count_ticks, date, state ) {
    auto        tt = random_times( COUNT, 1900, 2100 );
    auto const* tz = date::locate_zone( "America/New_York" );
    size_t      i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            date_count_ticks( tz, tt[i % COUNT], BENCH_TOD ) );
        ++i;
    }
}


BENCH( count_ticks_between, vtz, state ) {
    auto        tt0 = random_times( COUNT, 1900, 2000 );
    auto        tt1 = random_times( COUNT, 2000, 2100 );
    auto const* tz  = vtz::locate_zone( "America/New_York" );
    size_t      i   = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->count_ticks_s( tt0[i % COUNT], BENCH_TOD )
            - tz->count_ticks_s( tt1[i % COUNT], BENCH_TOD ) );
        ++i;
    }
}


/// Times out of the table's range: the 400-year cyclic reduction path
BENCH( count_ticks_far_future, vtz, state ) {
    auto        tt = random_times( COUNT, 3000, 3200 );
    auto const* tz = vtz::locate_zone( "America/New_York" );
    size_t      i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->count_ticks_s( tt[i % COUNT], BENCH_TOD ) );
        ++i;
    }
}


/// A fixed-offset zone uses the degenerate g=63 table layout
BENCH( count_ticks_fixed_zone, vtz, state ) {
    auto        tt = random_times( COUNT, 1900, 2100 );
    auto const* tz = vtz::locate_zone( "Asia/Kolkata" );
    size_t      i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->count_ticks_s( tt[i % COUNT], BENCH_TOD ) );
        ++i;
    }
}


/// Times drawn from the two days around each DST transition, so the clamp
/// logic is exercised constantly (rather than on the ~1% of inputs where a
/// transition happens to be nearby)
BENCH( count_ticks_near_transitions, vtz, state ) {
    auto const* tz = vtz::locate_zone( "America/New_York" );

    vector<i64> tt;
    auto        T    = vtz::sys_seconds( seconds( 0 ) );
    auto        Tend = vtz::sys_seconds( seconds( i64( 130 ) * 366 * 86400 ) );
    auto        rng  = std::mt19937_64{};
    auto        dist = std::uniform_int_distribution<i64>( -86400, 86400 );
    while( T < Tend && tt.size() < COUNT )
    {
        auto info = tz->get_info( T );
        if( info.end >= Tend ) break;
        tt.push_back( info.end.time_since_epoch().count() + dist( rng ) );
        T = info.end;
    }
    while( tt.size() < COUNT ) tt.push_back( tt[tt.size() % 240] );

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->count_ticks_s( tt[i % COUNT], BENCH_TOD ) );
        ++i;
    }
}
