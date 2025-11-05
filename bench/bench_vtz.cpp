#include "vtz/bit.h"
#include <benchmark/benchmark.h>
#include <chrono>
#include <date/date.h>
#include <date/tz.h>
#include <random>
#include <vector>
#include <vtz/civil.h>
#define BENCH( name, state )                                                   \
    void name( benchmark::State& state );                                      \
    BENCHMARK( name );                                                         \
    void name( benchmark::State& state )

using date::sys_seconds;
using std::vector;
using std::chrono::seconds;
using vtz::i64;

template<class T>
vector<T> toChrono( vector<i64> const& tt ) {
    vector<T> result( tt.size() );
    for( size_t i = 0; i < tt.size(); ++i )
    { result[i] = T( seconds( tt[i] ) ); }
    return result;
}

vector<i64> randomTimes( size_t count, int startYear, int endYear ) {
    vector<i64>                        result( count );
    std::mt19937_64                    rng;
    std::uniform_int_distribution<i64> dist{
        vtz::resolveCivilTime( startYear, 1, 1, 0, 0, 0 ),
        vtz::resolveCivilTime( endYear, 1, 1, 0, 0, 0 ),
    };

    for( size_t i = 0; i < count; ++i ) result[i] = dist( rng );

    return result;
}


BENCH( hinnant_to_local, state ) {
    constexpr size_t COUNT = 65536;
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( hinnant_to_sys_latest, state ) {
    constexpr size_t COUNT = 65536;
    auto tt = toChrono<date::local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto tz = date::locate_zone( "America/New_York" );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], date::choose::latest ) );
        ++i;
    }
}


BENCH( hinnant_to_sys_earliest, state ) {
    constexpr size_t COUNT = 65536;
    auto tt = toChrono<date::local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto tz = date::locate_zone( "America/New_York" );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], date::choose::earliest ) );
        ++i;
    }
}


