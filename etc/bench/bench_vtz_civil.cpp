#include "bench_common.h"

#include <vtz/civil.h>

using std::vector;

vector<vtz::civil_ymd> random_ymd(
    size_t count, int start_year, int end_year ) {
    return random_values( count,
        vtz::resolve_civil( start_year ),
        vtz::resolve_civil( end_year ),
        []( sysdays_t days ) { return vtz::to_civil( days ); } );
}


BENCH( vtz_date_to_civil, state ) {
    auto   dd = random_days( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::to_civil( dd[i % COUNT] ) );
        ++i;
    }
}

BENCH( vtz_date_from_civil_date, state ) {
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

BENCH( vtz_date_from_civil_year, state ) {
    auto   dd = random_ymd( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::resolve_civil( entry.year ) );
        ++i;
    }
}
