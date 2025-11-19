#include "vtz/bit.h"
#include <benchmark/benchmark.h>
#include <chrono>
#include <date/date.h>
#include <date/tz.h>
#include <random>
#include <vector>
#include <vtz/civil.h>
#include <vtz/tz.h>

#define BENCH( name, state )                                                   \
    void name( benchmark::State& state );                                      \
    BENCHMARK( name );                                                         \
    void name( benchmark::State& state )

using date::sys_seconds;
using std::vector;
using std::chrono::seconds;
using vtz::i64;
using vtz::sysdays_t;

template<class T, class Dur = typename T::duration>
vector<T> toChrono( vector<i64> const& tt ) {
    vector<T> result( tt.size() );
    for( size_t i = 0; i < tt.size(); ++i ) result[i] = T( Dur( tt[i] ) );
    return result;
}

template<class T, class F>
auto randomValues( size_t count, T start, T end, F func ) {
    using value_type = decltype( func( start ) );
    auto result      = vector<value_type>( count );
    auto rng         = std::mt19937_64{};
    auto dist        = std::uniform_int_distribution<T>( start, end );

    for( size_t i = 0; i < count; ++i ) { result[i] = func( dist( rng ) ); }
    return result;
}

template<class T>
auto randomValues( size_t count, T start, T end ) {
    return randomValues( count, start, end, []( auto x ) { return x; } );
}
vector<i64> randomTimes(
    size_t count, int startYear, int endYear, i64 mul = 1 ) {
    return randomValues( count,
        vtz::daysToSeconds( vtz::resolveCivil( startYear ) ),
        vtz::daysToSeconds( vtz::resolveCivil( endYear ) ),
        [=]( i64 x ) { return x * mul; } );
}


vector<sysdays_t> randomDays( size_t count, int startYear, int endYear ) {
    return randomValues(
        count, vtz::resolveCivil( startYear ), vtz::resolveCivil( endYear ) );
}

vector<vtz::sys_days> randomSysDays(
    size_t count, int startYear, int endYear ) {
    return randomValues( count,
        vtz::resolveCivil( startYear ),
        vtz::resolveCivil( endYear ),
        []( sysdays_t days ) { return vtz::sys_days( vtz::days( days ) ); } );
}


vector<vtz::YMD> randomYMD( size_t count, int startYear, int endYear ) {
    return randomValues( count,
        vtz::resolveCivil( startYear ),
        vtz::resolveCivil( endYear ),
        []( sysdays_t days ) { return vtz::toCivil( days ); } );
}


constexpr size_t COUNT = 65536;


BENCH( hinnant_to_local, state ) {
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


BENCH( hinnant_to_local_with_lookup, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( date::locate_zone( "America/New_York" )->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( hinnant_to_sys_latest_with_lookup, state ) {
    auto tt = toChrono<date::local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            date::locate_zone( "America/New_York" )->to_sys( tt[i % COUNT], date::choose::latest ) );
        ++i;
    }
}


BENCH( hinnant_to_sys_earliest_with_lookup, state ) {
    auto tt = toChrono<date::local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto tz = date::locate_zone( "America/New_York" );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            date::locate_zone( "America/New_York" )->to_sys( tt[i % COUNT], date::choose::earliest ) );
        ++i;
    }
}


BENCH( vtz_to_local, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_to_sys_latest, state ) {
    auto tt  = toChrono<vtz::local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto tz  = vtz::locate_zone( "America/New_York" );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], vtz::choose::latest ) );
        ++i;
    }
}


BENCH( vtz_to_sys_earliest, state ) {
    auto tt  = toChrono<vtz::local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto tz  = vtz::locate_zone( "America/New_York" );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], vtz::choose::earliest ) );
        ++i;
    }
}


BENCH( vtz_to_local_with_lookup, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::locate_zone( "America/New_York" )->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_to_sys_latest_with_lookup, state ) {
    auto tt  = toChrono<vtz::local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::locate_zone( "America/New_York" )->to_sys( tt[i % COUNT], vtz::choose::latest ) );
        ++i;
    }
}


BENCH( vtz_to_sys_earliest_with_lookup, state ) {
    auto tt  = toChrono<vtz::local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::locate_zone( "America/New_York" )->to_sys( tt[i % COUNT], vtz::choose::earliest ) );
        ++i;
    }
}


BENCH( vtz_to_local_s, state ) {
    auto   tt = randomTimes( COUNT, 1900, 2100 );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local_s( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_to_sys_latest_s, state ) {
    auto   tt = randomTimes( COUNT, 1900, 2100 );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys_s( tt[i % COUNT], vtz::choose::latest ) );
        ++i;
    }
}


BENCH( vtz_to_sys_earliest_s, state ) {
    auto   tt = randomTimes( COUNT, 1900, 2100 );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys_s( tt[i % COUNT], vtz::choose::earliest ) );
        ++i;
    }
}


BENCH( hinnant_format, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            date::format( "%F %T %Z", date::make_zoned( tz, tt[i % COUNT] ) ) );
        ++i;
    }
}


BENCH( vtz_format_strftime, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->format( "%F %T %Z", tt[i % COUNT] ) );
        ++i;
    }
}

BENCH( vtz_format_to_strftime, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
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

BENCH( vtz_format_to_strftime_nanos, state ) {
    using nanos = vtz::sys_time<std::chrono::nanoseconds>;
    auto   tt = toChrono<nanos>( randomTimes( COUNT, 1900, 2100, 1000000000 ) );
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

BENCH( vtz_format, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->format( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_format_to, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    char   buff[64];
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->format_to( tt[i % COUNT], buff, sizeof( buff ) ) );
        ++i;
    }
}


BENCH( vtz_format_compact, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->format_compact( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_format_compact_to, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    char   buff[32];
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->format_compact_to( tt[i % COUNT], buff, sizeof( buff ) ) );
        ++i;
    }
}

BENCH( vtz_format_iso8601, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->format_iso8601( tt[i % COUNT] ) );
        ++i;
    }
}

BENCH( vtz_format_iso8601_to, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    char   buff[32];
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->format_iso8601_to( tt[i % COUNT], buff, sizeof( buff ) ) );
        ++i;
    }
}

BENCH( vtz_format_date, state ) {
    auto   dd = randomSysDays( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::format_date( "%F", dd[i % COUNT] ) );
        ++i;
    }
}

BENCH( vtz_format_date_to, state ) {
    auto   dd = randomSysDays( COUNT, 1900, 2100 );
    size_t i  = 0;
    char   buff[64];
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::format_date_to( "%F", dd[i % COUNT], buff, sizeof( buff ) ) );
        ++i;
    }
}


BENCH( vtz_date_to_civil, state ) {
    auto   dd = randomDays( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::toCivil( dd[i % COUNT] ) );
        ++i;
    }
}

BENCH( vtz_date_from_civil_date, state ) {
    auto   dd = randomYMD( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize(
            vtz::resolveCivil( entry.year, entry.month, entry.day ) );
        ++i;
    }
}

BENCH( vtz_date_from_civil_year, state ) {
    auto   dd = randomYMD( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::resolveCivil( entry.year ) );
        ++i;
    }
}

BENCH( vtz_parse_date, state ) {
    auto   dd = randomValues( COUNT,
        vtz::resolveCivil( 1900 ),
        vtz::resolveCivil( 2100 ),
        []( sysdays_t days ) { return vtz::format_date_d( "%F", days ); } );
    size_t i  = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::parse_date_d( "%Y-%m-%d", entry ) );
        ++i;
    }
}


BENCH( vtz_parse_time, state ) {
    static auto const& utc = vtz::TimeZone::utc();

    auto dd = randomValues( COUNT,
        vtz::resolveCivilTime( 1900, 1, 1, 0, 0, 0 ),
        vtz::resolveCivilTime( 2100, 1, 1, 0, 0, 0 ),
        []( vtz::sec_t t ) { return utc.format_s( "%F %T %Z", t ); } );

    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize(
            vtz::parse_time_s( "%Y-%m-%d %H:%M:%S", entry ) );
        ++i;
    }
}

BENCH( hinnant_parse_date, state ) {
    auto   dd = randomValues( COUNT,
        vtz::resolveCivil( 1900 ),
        vtz::resolveCivil( 2100 ),
        []( sysdays_t days ) { return vtz::format_date_d( "%F", days ); } );
    size_t i  = 0;
    for( auto _ : state )
    {
        auto const&          entry = dd[i % COUNT];
        std::istringstream   ss( entry );
        date::year_month_day ymd;
        ss >> date::parse( "%Y-%m-%d", ymd );
        benchmark::DoNotOptimize( ymd );
        ++i;
    }
}

BENCH( hinnant_parse_time, state ) {
    static auto const& utc = vtz::TimeZone::utc();

    auto dd = randomValues( COUNT,
        vtz::resolveCivilTime( 1900, 1, 1, 0, 0, 0 ),
        vtz::resolveCivilTime( 2100, 1, 1, 0, 0, 0 ),
        []( vtz::sec_t t ) { return utc.format_s( "%F %T %Z", t ); } );

    size_t i = 0;
    for( auto _ : state )
    {
        auto const&        entry = dd[i % COUNT];
        std::istringstream ss( entry );
        date::sys_seconds  tp;
        date::from_stream( ss, "%Y-%m-%d %H:%M:%S", tp );
        benchmark::DoNotOptimize( tp );
        ++i;
    }
}

BENCH( vtz_parse_date_compact, state ) {
    static auto const& utc = vtz::TimeZone::utc();

    auto dd = randomValues( COUNT,
        vtz::resolveCivilTime( 1900, 1, 1, 0, 0, 0 ),
        vtz::resolveCivilTime( 2100, 1, 1, 0, 0, 0 ),
        []( vtz::sec_t t ) { return utc.format_s( "%F %T %Z", t ); } );

    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::parse_date_d( "%F", entry ) );
        ++i;
    }
}

BENCH( vtz_parse_time_compact, state ) {
    static auto const& utc = vtz::TimeZone::utc();

    auto dd = randomValues( COUNT,
        vtz::resolveCivilTime( 1900, 1, 1, 0, 0, 0 ),
        vtz::resolveCivilTime( 2100, 1, 1, 0, 0, 0 ),
        []( vtz::sec_t t ) { return utc.format_s( "%F %T %Z", t ); } );

    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::parse_time_s( "%F %T", entry ) );
        ++i;
    }
}

// if std::chrono::locate_zone is available, benchmark std::chrono::time_zone
#if __cpp_lib_chrono >= 201907L
BENCH( chrono_to_local, state ) {
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = std::chrono::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( chrono_to_sys_latest, state ) {
    using std::chrono::choose;
    using std::chrono::local_seconds;
    using std::chrono::locate_zone;

    auto tt = toChrono<local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto tz = locate_zone( "America/New_York" );

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_sys( tt[i % COUNT], choose::latest ) );
        ++i;
    }
}


BENCH( chrono_to_sys_earliest, state ) {
    using std::chrono::choose;
    using std::chrono::local_seconds;
    using std::chrono::locate_zone;

    auto   tt = toChrono<local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    auto   tz = locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], choose::earliest ) );
        ++i;
    }
}


BENCH( chrono_to_local_with_lookup, state ) {
    using std::chrono::locate_zone;
    auto   tt = toChrono<sys_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( locate_zone( "America/New_York" )->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( chrono_to_sys_latest_with_lookup, state ) {
    using std::chrono::choose;
    using std::chrono::local_seconds;
    using std::chrono::locate_zone;

    auto tt = toChrono<local_seconds>( randomTimes( COUNT, 1900, 2100 ) );

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( locate_zone( "America/New_York" )->to_sys( tt[i % COUNT], choose::latest ) );
        ++i;
    }
}


BENCH( chrono_to_sys_earliest_with_lookup, state ) {
    using std::chrono::choose;
    using std::chrono::local_seconds;
    using std::chrono::locate_zone;

    auto   tt = toChrono<local_seconds>( randomTimes( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            locate_zone( "America/New_York" )->to_sys( tt[i % COUNT], choose::earliest ) );
        ++i;
    }
}
#endif
