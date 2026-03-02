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


BENCH( date_to_civil_vtz, state ) {
    auto   dd = random_days( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::to_civil( dd[i % COUNT] ) );
        ++i;
    }
}

BENCH( date_from_civil_date_vtz, state ) {
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

BENCH( date_from_civil_year_vtz, state ) {
    auto   dd = random_ymd( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::resolve_civil( entry.year ) );
        ++i;
    }
}
