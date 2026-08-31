#include "bench_common.h"

#include <vtz/civil.h>

using std::vector;

vector<vtz::civil_ymd> random_ymd(
    size_t count, int start_year, int end_year ) {
    return random_values( count,
        vtz::resolve_civil( start_year ),
        vtz::resolve_civil( end_year ),
        []( sys_days_t days ) { return vtz::to_civil( days ); } );
}


BENCH( date_to_civil, vtz, state ) {
    auto   dd = random_days( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::to_civil( dd[i % COUNT] ) );
        ++i;
    }
}


BENCH( date_civil_add_months, vtz, state ) {
    auto   dd = random_days( COUNT, 1900, 2100 );
    auto   nn = random_values( COUNT, -60, 50 );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::civil_add_months( dd[i % COUNT], nn[i % COUNT] ) );
        ++i;
    }
}


BENCH( date_civil_add_years, vtz, state ) {
    auto   dd = random_days( COUNT, 1900, 2100 );
    auto   nn = random_values( COUNT, -60, 50 );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::civil_add_years( dd[i % COUNT], nn[i % COUNT] ) );
        ++i;
    }
}


BENCH( date_civil_add_months_clamped, vtz, state ) {
    auto   dd = random_days( COUNT, 1900, 2100 );
    auto   nn = random_values( COUNT, -60, 50 );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::civil_add_months_clamped( dd[i % COUNT], nn[i % COUNT] ) );
        ++i;
    }
}


BENCH( date_civil_add_years_clamped, vtz, state ) {
    auto   dd = random_days( COUNT, 1900, 2100 );
    auto   nn = random_values( COUNT, -60, 50 );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::civil_add_years_clamped( dd[i % COUNT], nn[i % COUNT] ) );
        ++i;
    }
}


BENCH( date_from_civil_date, vtz, state ) {
    auto   dd = random_ymd( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize(
            vtz::resolve_civil( entry.year, entry.month, entry.day ) );
        ++i;
    }
}

BENCH( date_from_civil_year, vtz, state ) {
    auto   dd = random_ymd( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::resolve_civil( entry.year ) );
        ++i;
    }
}
