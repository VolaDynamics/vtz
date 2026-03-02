#include <vtz/format.h>
#include <vtz/parse.h>
#include <vtz/tz.h>

#include "absl_helper.h"

#include <date/date.h>
#include <date/tz.h>

static std::vector<std::string> input_dates = random_values( COUNT,
    vtz::resolve_civil( 1900 ),
    vtz::resolve_civil( 2100 ),
    []( sysdays_t days ) { return vtz::format_d( "%F", days ); } );

static std::vector<std::string> input_times = random_values( COUNT,
    vtz::resolve_civil_time( 1900, 1, 1, 0, 0, 0 ),
    vtz::resolve_civil_time( 2100, 1, 1, 0, 0, 0 ),
    []( vtz::sec_t t ) { return vtz::format_s( "%F %T", t ); } );


BENCH( hinnant_parse_date, state ) {
    auto               dd = input_dates;
    std::istringstream ss;
    ss.exceptions( std::ios::failbit );
    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        ss.clear(); // clear flags
        ss.str( entry );
        date::year_month_day ymd;
        ss >> date::parse( "%F", ymd );
        benchmark::DoNotOptimize( ymd );
        ++i;
    }
}


BENCH( hinnant_parse_time, state ) {
    auto dd = input_times;

    std::istringstream ss;
    ss.exceptions( std::ios::failbit );
    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        ss.clear(); // clear flags
        ss.str( entry );
        date::sys_seconds tp;
        date::from_stream( ss, "%F %T", tp );
        benchmark::DoNotOptimize( tp );
        ++i;
    }
}


BENCH( absl_parse_date, state ) {
    auto dd = input_dates;

    absl::Time  t;
    std::string err;
    size_t      i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        if( !absl::ParseTime( "%F", entry, &t, &err ) )
            throw std::runtime_error( err );
        benchmark::DoNotOptimize( t );
        ++i;
    }
}


BENCH( absl_parse_time, state ) {
    auto dd = input_times;

    absl::Time  t;
    std::string err;
    size_t      i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        if( !absl::ParseTime( "%F %T", entry, &t, &err ) )
            throw std::runtime_error( err );
        benchmark::DoNotOptimize( t );
        ++i;
    }
}

#if HAS_CHRONO_TIMEZONE
BENCH( chrono_parse_date, state ) {
    auto dd = input_dates;

    std::istringstream ss;
    ss.exceptions( std::ios::failbit );
    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        ss.clear(); // clear flags
        ss.str( entry );
        std::chrono::year_month_day ymd;
        ss >> std::chrono::parse( "%F", ymd );
        benchmark::DoNotOptimize( ymd );
        ++i;
    }
}

BENCH( chrono_parse_time, state ) {
    auto dd = input_times;

    std::istringstream ss;
    ss.exceptions( std::ios::failbit );
    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        ss.clear(); // clear flags
        ss.str( entry );
        std::chrono::sys_seconds tp;
        ss >> std::chrono::parse( "%F %T", tp );
        benchmark::DoNotOptimize( tp );
        ++i;
    }
}
#endif


BENCH( vtz_parse_date, state ) {
    auto dd = input_dates;

    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::parse_d( "%F", entry ) );
        ++i;
    }
}

BENCH( vtz_parse_time, state ) {
    auto dd = input_times;

    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::parse_s( "%F %T", entry ) );
        ++i;
    }
}
