#pragma once
#include "bench_zones.h"
#include <benchmark/benchmark.h>

#include <random>
#include <string_view>
#include <vector>

#include <vtz/civil.h>
#include <vtz/impl/chrono_types.h>
#include <vtz/types.h>

#include <vtz/tz.h>

using std::vector;
using std::chrono::seconds;
using vtz::i64;
using vtz::sys_days_t;
using vtz::sys_seconds;

using nanos = vtz::sys_time<std::chrono::nanoseconds>;


constexpr size_t COUNT = 65536;


#if __cpp_lib_chrono >= 201907L
    #define HAS_CHRONO_TIMEZONE 1
#else
    #define HAS_CHRONO_TIMEZONE 0
#endif


#define BENCH( name, lib, state )                                              \
    void name##_##lib( benchmark::State& state );                              \
    BENCHMARK( name##_##lib )->Name( #name "/" #lib );                         \
    void name##_##lib( benchmark::State& state )


template<class T, class F>
auto random_values( size_t count, T start, T end, F func ) {
    using value_type = decltype( func( start ) );
    auto result      = vector<value_type>( count );
    auto rng         = std::mt19937_64{};
    auto dist        = std::uniform_int_distribution<T>( start, end );

    for( size_t i = 0; i < count; ++i ) { result[i] = func( dist( rng ) ); }
    return result;
}


template<class T, class F, class Filter>
auto random_values( size_t count, T start, T end, F func, Filter filter ) {
    using value_type = decltype( func( start ) );
    auto result      = vector<value_type>( count );
    auto rng         = std::mt19937_64{};
    auto dist        = std::uniform_int_distribution<T>( start, end );

    for( size_t i = 0; i < count; )
    {
        auto value = func( dist( rng ) );

        if( filter( value ) ) { result[i++] = std::move( value ); }
    }
    return result;
}


template<class T>
auto random_values( size_t count, T start, T end ) {
    return random_values( count, start, end, []( auto x ) { return x; } );
}


template<class T, class Dur = typename T::duration>
vector<T> to_chrono( vector<i64> const& tt ) {
    vector<T> result( tt.size() );
    for( size_t i = 0; i < tt.size(); ++i ) result[i] = T( Dur( tt[i] ) );
    return result;
}


/// Get a shuffled list of zones for benchmarking
inline vector<std::string_view> random_zones( size_t count ) {
    vector<std::string_view> result( count );
    constexpr size_t         ZONE_COUNT = std::size( ZONES_FOR_BENCH );
    for( size_t i = 0; i < count; ++i )
        result[i] = ZONES_FOR_BENCH[i % ZONE_COUNT];
    std::shuffle( result.begin(), result.end(), std::mt19937_64{} );
    return result;
}


inline vector<vtz::sys_days> random_sys_days(
    size_t count, int start_year, int end_year ) {
    return random_values( count,
        vtz::resolve_civil( start_year ),
        vtz::resolve_civil( end_year ),
        []( sys_days_t days ) { return vtz::sys_days( vtz::days( days ) ); } );
}


inline vector<i64> random_times(
    size_t count, int start_year, int end_year, i64 mul = 1 ) {
    return random_values( count,
        vtz::days_to_seconds( vtz::resolve_civil( start_year ) ),
        vtz::days_to_seconds( vtz::resolve_civil( end_year ) ),
        [=]( i64 x ) { return x * mul; } );
}


template<class Filter>
inline vector<i64> random_times(
    size_t count, int start_year, int end_year, i64 mul, Filter filter ) {
    return random_values(
        count,
        vtz::days_to_seconds( vtz::resolve_civil( start_year ) ),
        vtz::days_to_seconds( vtz::resolve_civil( end_year ) ),
        [=]( i64 x ) { return x * mul; },
        filter );
}


inline vector<sys_days_t> random_days(
    size_t count, int start_year, int end_year ) {
    return random_values( count,
        vtz::resolve_civil( start_year ),
        vtz::resolve_civil( end_year ) );
}


// Given a zone returns a function which takes an input local time in seconds
// and returns true if that local time is unique
inline auto is_unique( std::string_view name ) {
    return [tz = vtz::locate_zone( name )]( i64 sec ) -> bool {
        return tz->local_type( vtz::local_seconds( vtz::seconds( sec ) ) )
               == vtz::local_type::unique;
    };
}
