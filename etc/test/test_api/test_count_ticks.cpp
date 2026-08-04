#include "test_api_util.h"

#include <test_vtz/utils.h>

#include <gtest/gtest.h>
#include <random>

#include <vtz/impl/math.h>
#include <vtz/parse.h>
#include <vtz/tz.h>

using namespace vtz;

/// Parse a "%F %T" datetime string as a sys_seconds_t
static sys_seconds_t _pt( string_view s ) { return parse_s( "%F %T", s ); }

static sys_seconds_t _pte( vtz::time_zone const* tz, string_view s ) {
    return tz->to_sys_s( parse_s( "%F %T", s ), vtz::choose::earliest );
}

template<class F>
struct Ctx {
    F func;

    friend std::ostream& operator<<( std::ostream& os, Ctx const& ctx ) {
        os << ctx.func();
        return os;
    }
};
template<class T>
Ctx( T ) -> Ctx<T>;


/// Zones chosen to cover the interesting behaviors: ordinary DST zones in
/// both hemispheres, fixed-offset zones, sub-minute LMT offsets, 30-minute
/// DST (Lord Howe), negative DST (Dublin), date-line skips (Apia and
/// Kwajalein each skipped an entire calendar day), a date-line *repeat*
/// (Sitka repeated a day when Alaska was purchased), and zones with dense or
/// irregular transition history (Casablanca, Cairo, Santiago).
constexpr std::string_view TICK_ZONES[] = {
    "America/New_York",
    "America/Sitka",
    "Africa/Cairo",
    "Africa/Casablanca",
    "Africa/Monrovia",
    "America/Santiago",
    "Antarctica/Troll",
    "Asia/Kathmandu",
    "Asia/Tehran",
    "Australia/Lord_Howe",
    "Australia/Sydney",
    "Asia/Kolkata",
    "Europe/Dublin",
    "Europe/London",
    "Pacific/Apia",
    "Pacific/Chatham",
    "Pacific/Kwajalein",
    "Etc/UTC",
};

constexpr std::array<i64, 48> get_tods() {
    std::array<i64, 48> tods{};
    for( int i = 0; i < 48; i++ ) { tods[i] = i * 30 * 60; }
    return tods;
}

constexpr auto LOCAL_TIMES_INTRADAY = get_tods();

std::string ctx_tt( vtz::time_zone const* tz, int64_t tod, sec_t T, sec_t tt ) {
    return fmt::format( "tz     = {}\n"
                        "tod    = {}\n"
                        "tt     = {}  # transition time (UTC)\n"
                        "T      = {}  # time of failure (UTC)\n"
                        "tt - 1 = {} offset={}\n"
                        "tt     = {} offset={}\n"
                        "T - 1  = {}\n"
                        "T      = {}\n",
        tz->name(),
        vtz::format_s( "%T", tod ),
        vtz::format_s( "%F %T %z", tt ),
        vtz::format_s( "%F %T %z", T ),
        tz->format_s( "%F %T %z (%Z)", tt - 1 ),
        tz->offset_s( tt - 1 ),
        tz->format_s( "%F %T %z (%Z)", tt ),
        tz->offset_s( tt ),
        tz->format_s( "%F %T %z (%Z)", T - 1 ),
        tz->format_s( "%F %T %z (%Z)", T ) );
}

std::string ctx( vtz::time_zone const* tz, sec_t tod, sec_t T ) {
    return fmt::format( "tz     = {}\n"
                        "tod    = {}\n"
                        "T      = {}  # time of failure (UTC)\n"
                        "T - 1  = {}\n"
                        "T      = {}\n",
        tz->name(),
        vtz::format_s( "%T", tod ),
        vtz::format_s( "%F %T %z", T ),
        tz->format_s( "%F %T %z (%Z)", T - 1 ),
        tz->format_s( "%F %T %z (%Z)", T ) );
}

TEST( vtz_count_ticks, sanity_check ) {
    auto time_of_day = 3600ll * 18; // 6pm
    auto ny          = vtz::locate_zone( "America/New_York" );
    ASSERT_EQ(
        ny->count_ticks_s( _pte( ny, "2024-01-01 18:00:00" ), time_of_day )
            - ny->count_ticks_s(
                _pte( ny, "2024-01-01 17:59:59" ), time_of_day ),
        1 );
}

TEST( vtz_count_ticks, transition_times ) {
    auto T0   = vtz::parse<seconds>( "%F %T", "1600-01-01 00:00:00" );
    auto Tmax = vtz::parse<seconds>( "%F %T", "2400-01-01 00:00:00" );
    for( auto tz_name : TICK_ZONES )
    {
        fmt::println( "Testing {}", tz_name );
        auto tz     = vtz::locate_zone( tz_name );
        auto states = extract_states( date::locate_zone( tz_name ), T0, Tmax );

        for( auto time_of_day : LOCAL_TIMES_INTRADAY )
        {
            for( auto const& st : states )
            {
                // Transition time
                auto tt = st.end.time_since_epoch().count();
                for( int off = -48 * 60; off <= 48 * 60; ++off )
                {
                    auto T = off * 60ll + tt;

                    ASSERT_LE( tz->count_ticks_s( T - 1, time_of_day ),
                        tz->count_ticks_s( T, time_of_day ) )
                        << ctx_tt( tz, time_of_day, T, tt );
                }
            }
        }
    }
}


TEST( vtz_count_ticks, increasing ) {
    auto day0 = vtz::parse_d( "%F", "1800-01-01" );
    auto day1 = vtz::parse_d( "%F", "2300-01-01" );
    for( auto zone : TICK_ZONES )
    {
        fmt::println( "Testing {}", zone );
        auto const* tz = locate_zone( zone );
        for( auto tod : LOCAL_TIMES_INTRADAY )
        {
            for( auto day = day0; day < day1; ++day )
            {
                auto tick_time_local = 86400ll * day + tod;
                auto T = tz->to_sys_s( tick_time_local, vtz::choose::latest );
                // measure the jump size, in local seconds
                auto jump = tz->to_local_s( T ) - tz->to_local_s( T - 1 );

                auto c0 = tz->count_ticks_s( T - 1, tod );
                auto c1 = tz->count_ticks_s( T, tod );
                // Check that computed values match reference values
                ASSERT_LT( c0, c1 ) << ctx( tz, tod, T );
                if( jump >= 86400 )
                {
                    // Check that the delta is bounded by the jump in days
                    ASSERT_LE( c1 - c0, 1 + jump / 86400 ) << ctx( tz, tod, T );
                }
            }
        }
    }
}
