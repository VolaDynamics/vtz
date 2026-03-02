#include "bench_zones.h"
#include <absl/time/time.h>
#include <benchmark/benchmark.h>
#include <chrono>
#include <date/date.h>
#include <date/tz.h>
#include <exception>
#include <random>
#include <vector>
#include <vtz/civil.h>
#include <vtz/impl/bit.h>

#include <vtz/format.h>
#include <vtz/parse.h>
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
vector<T> to_chrono( vector<i64> const& tt ) {
    vector<T> result( tt.size() );
    for( size_t i = 0; i < tt.size(); ++i ) result[i] = T( Dur( tt[i] ) );
    return result;
}

/// Get a shuffled list of zones for benchmarking
vector<std::string_view> random_zones( size_t count ) {
    vector<std::string_view> result( count );
    constexpr size_t         ZONE_COUNT = std::size( ZONES_FOR_BENCH );
    for( size_t i = 0; i < count; ++i )
        result[i] = ZONES_FOR_BENCH[i % ZONE_COUNT];
    std::shuffle( result.begin(), result.end(), std::mt19937_64{} );
    return result;
}

template<class T, class F>
auto random_values( size_t count, T start, T end, F func ) {
    using value_type = decltype( func( start ) );
    auto result      = vector<value_type>( count );
    auto rng         = std::mt19937_64{};
    auto dist        = std::uniform_int_distribution<T>( start, end );

    for( size_t i = 0; i < count; ++i ) { result[i] = func( dist( rng ) ); }
    return result;
}

template<class T>
auto random_values( size_t count, T start, T end ) {
    return random_values( count, start, end, []( auto x ) { return x; } );
}
vector<i64> random_times(
    size_t count, int start_year, int end_year, i64 mul = 1 ) {
    return random_values( count,
        vtz::days_to_seconds( vtz::resolve_civil( start_year ) ),
        vtz::days_to_seconds( vtz::resolve_civil( end_year ) ),
        [=]( i64 x ) { return x * mul; } );
}


vector<sysdays_t> random_days( size_t count, int start_year, int end_year ) {
    return random_values( count,
        vtz::resolve_civil( start_year ),
        vtz::resolve_civil( end_year ) );
}

vector<vtz::sys_days> random_sys_days(
    size_t count, int start_year, int end_year ) {
    return random_values( count,
        vtz::resolve_civil( start_year ),
        vtz::resolve_civil( end_year ),
        []( sysdays_t days ) { return vtz::sys_days( vtz::days( days ) ); } );
}


vector<vtz::civil_ymd> random_ymd(
    size_t count, int start_year, int end_year ) {
    return random_values( count,
        vtz::resolve_civil( start_year ),
        vtz::resolve_civil( end_year ),
        []( sysdays_t days ) { return vtz::to_civil( days ); } );
}


static vector<absl::Time> to_absl_time( vector<i64> const& tt ) {
    vector<absl::Time> result( tt.size() );
    for( size_t i = 0; i < tt.size(); ++i )
        result[i] = absl::FromUnixSeconds( tt[i] );
    return result;
}

static vector<absl::CivilSecond> to_absl_civil( vector<i64> const& tt ) {
    auto                      utc = absl::UTCTimeZone();
    vector<absl::CivilSecond> result( tt.size() );
    for( size_t i = 0; i < tt.size(); ++i )
        result[i] = utc.At( absl::FromUnixSeconds( tt[i] ) ).cs;
    return result;
}

constexpr size_t COUNT = 65536;


BENCH( hinnant_to_local, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( hinnant_to_sys_latest, state ) {
    auto tt
        = to_chrono<date::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], date::choose::latest ) );
        ++i;
    }
}


BENCH( hinnant_to_sys_earliest, state ) {
    auto tt
        = to_chrono<date::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = date::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], date::choose::earliest ) );
        ++i;
    }
}


BENCH( hinnant_to_local_with_lookup, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( date::locate_zone( "America/New_York" )
                ->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( hinnant_to_sys_latest_with_lookup, state ) {
    auto tt
        = to_chrono<date::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( date::locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], date::choose::latest ) );
        ++i;
    }
}


BENCH( hinnant_to_sys_earliest_with_lookup, state ) {
    auto tt
        = to_chrono<date::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( date::locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], date::choose::earliest ) );
        ++i;
    }
}


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


BENCH( vtz_to_local, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_to_sys_latest, state ) {
    auto tt
        = to_chrono<vtz::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], vtz::choose::latest ) );
        ++i;
    }
}


BENCH( vtz_to_sys_earliest, state ) {
    auto tt
        = to_chrono<vtz::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            tz->to_sys( tt[i % COUNT], vtz::choose::earliest ) );
        ++i;
    }
}


BENCH( vtz_to_local_with_lookup, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::locate_zone( "America/New_York" )->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_to_sys_latest_with_lookup, state ) {
    auto tt
        = to_chrono<vtz::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], vtz::choose::latest ) );
        ++i;
    }
}


BENCH( vtz_to_sys_earliest_with_lookup, state ) {
    auto tt
        = to_chrono<vtz::local_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], vtz::choose::earliest ) );
        ++i;
    }
}


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


BENCH( absl_to_local, state ) {
    auto           tt = to_absl_time( random_times( COUNT, 1900, 2100 ) );
    absl::TimeZone tz;
    absl::LoadTimeZone( "America/New_York", &tz );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz.At( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( absl_to_sys_latest, state ) {
    auto           cs = to_absl_civil( random_times( COUNT, 1900, 2100 ) );
    absl::TimeZone tz;
    absl::LoadTimeZone( "America/New_York", &tz );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz.At( cs[i % COUNT] ).post );
        ++i;
    }
}


BENCH( absl_to_sys_earliest, state ) {
    auto           cs = to_absl_civil( random_times( COUNT, 1900, 2100 ) );
    absl::TimeZone tz;
    absl::LoadTimeZone( "America/New_York", &tz );
    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz.At( cs[i % COUNT] ).pre );
        ++i;
    }
}


BENCH( absl_to_local_with_lookup, state ) {
    auto   tt = to_absl_time( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        absl::TimeZone tz;
        absl::LoadTimeZone( "America/New_York", &tz );
        benchmark::DoNotOptimize( tz.At( tt[i % COUNT] ) );
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


BENCH( vtz_to_local_s, state ) {
    auto   tt = random_times( COUNT, 1900, 2100 );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->to_local_s( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_to_sys_latest_s, state ) {
    auto   tt = random_times( COUNT, 1900, 2100 );
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
    auto   tt = random_times( COUNT, 1900, 2100 );
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


BENCH( vtz_format_strftime, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->format( "%F %T %Z", tt[i % COUNT] ) );
        ++i;
    }
}

BENCH( vtz_format_to_strftime, state ) {
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

BENCH( vtz_format_to_strftime_nanos, state ) {
    using nanos = vtz::sys_time<std::chrono::nanoseconds>;
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

BENCH( vtz_format, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->format( tt[i % COUNT] ) );
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
            tz->format_to( tt[i % COUNT], buff, sizeof( buff ) ) );
        ++i;
    }
}


BENCH( vtz_format_compact, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->format_compact( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( vtz_format_compact_to, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
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
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    auto   tz = vtz::locate_zone( "America/New_York" );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( tz->format_iso8601( tt[i % COUNT] ) );
        ++i;
    }
}

BENCH( vtz_format_iso8601_to, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
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
    auto   dd = random_sys_days( COUNT, 1900, 2100 );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( vtz::format( "%F", dd[i % COUNT] ) );
        ++i;
    }
}

BENCH( vtz_format_date_to, state ) {
    auto   dd = random_sys_days( COUNT, 1900, 2100 );
    size_t i  = 0;
    char   buff[64];
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            vtz::format_to( "%F", dd[i % COUNT], buff, sizeof( buff ) ) );
        ++i;
    }
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


BENCH( hinnant_parse_date, state ) {
    auto               dd = random_values( COUNT,
        vtz::resolve_civil( 1900 ),
        vtz::resolve_civil( 2100 ),
        []( sysdays_t days ) { return vtz::format_d( "%F", days ); } );
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
    auto dd = random_values( COUNT,
        vtz::resolve_civil_time( 1900, 1, 1, 0, 0, 0 ),
        vtz::resolve_civil_time( 2100, 1, 1, 0, 0, 0 ),
        []( vtz::sec_t t ) { return vtz::format_s( "%F %T %Z", t ); } );

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

BENCH( vtz_parse_date, state ) {
    auto dd = random_values( COUNT,
        vtz::resolve_civil_time( 1900, 1, 1, 0, 0, 0 ),
        vtz::resolve_civil_time( 2100, 1, 1, 0, 0, 0 ),
        []( vtz::sec_t t ) { return vtz::format_s( "%F %T %Z", t ); } );

    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::parse_d( "%F", entry ) );
        ++i;
    }
}

BENCH( vtz_parse_time, state ) {
    auto dd = random_values( COUNT,
        vtz::resolve_civil_time( 1900, 1, 1, 0, 0, 0 ),
        vtz::resolve_civil_time( 2100, 1, 1, 0, 0, 0 ),
        []( vtz::sec_t t ) { return vtz::format_s( "%F %T %Z", t ); } );

    size_t i = 0;
    for( auto _ : state )
    {
        auto const& entry = dd[i % COUNT];
        benchmark::DoNotOptimize( vtz::parse_s( "%F %T", entry ) );
        ++i;
    }
}

// if std::chrono::locate_zone is available, benchmark std::chrono::time_zone
#if __cpp_lib_chrono >= 201907L
BENCH( chrono_to_local, state ) {
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
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

    auto tt = to_chrono<local_seconds>( random_times( COUNT, 1900, 2100 ) );
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

    auto   tt = to_chrono<local_seconds>( random_times( COUNT, 1900, 2100 ) );
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
    auto   tt = to_chrono<sys_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize(
            locate_zone( "America/New_York" )->to_local( tt[i % COUNT] ) );
        ++i;
    }
}


BENCH( chrono_to_sys_latest_with_lookup, state ) {
    using std::chrono::choose;
    using std::chrono::local_seconds;
    using std::chrono::locate_zone;

    auto tt = to_chrono<local_seconds>( random_times( COUNT, 1900, 2100 ) );

    size_t i = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], choose::latest ) );
        ++i;
    }
}


BENCH( chrono_to_sys_earliest_with_lookup, state ) {
    using std::chrono::choose;
    using std::chrono::local_seconds;
    using std::chrono::locate_zone;

    auto   tt = to_chrono<local_seconds>( random_times( COUNT, 1900, 2100 ) );
    size_t i  = 0;
    for( auto _ : state )
    {
        benchmark::DoNotOptimize( locate_zone( "America/New_York" )
                ->to_sys( tt[i % COUNT], choose::earliest ) );
        ++i;
    }
}


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


BENCH( chrono_parse_date, state ) {
    auto dd = random_values( COUNT,
        vtz::resolve_civil_time( 1900, 1, 1, 0, 0, 0 ),
        vtz::resolve_civil_time( 2100, 1, 1, 0, 0, 0 ),
        []( vtz::sec_t t ) { return vtz::format_s( "%F %T %Z", t ); } );

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
    auto dd = random_values( COUNT,
        vtz::resolve_civil_time( 1900, 1, 1, 0, 0, 0 ),
        vtz::resolve_civil_time( 2100, 1, 1, 0, 0, 0 ),
        []( vtz::sec_t t ) { return vtz::format_s( "%F %T %Z", t ); } );

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

#include <fmt/core.h>
int main( int argc, char** argv ) {
    try
    {
        benchmark::Initialize( &argc, argv );
        if( benchmark::ReportUnrecognizedArguments( argc, argv ) ) return 1;

        vtz::set_install( "build/data/tzdata" );
        date::set_install( "build/data/tzdata" );
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
        return 0;
    }
    catch( std::exception const& ex )
    {
        fmt::println( "Error: {}", ex.what() );
        return 1;
    }
    catch( ... )
    {
        fmt::println( "Failed with unknown exception." );
        return 1;
    }
}
