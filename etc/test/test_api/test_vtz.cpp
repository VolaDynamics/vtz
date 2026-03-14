#include <test_vtz/context.h>
#include <test_vtz/utils.h>
#include <test_vtz/zones.h>

#include <random>

#include <vtz/impl/chrono_types.h>
#include <vtz/parse.h>
#include <vtz/tz.h>

#include <fmt/format.h>

#include <date/tz.h>


TEST( vtz_api, locate_zone_sanity ) {
    // Sanity check for ensuring we can locate the given zone
    //
    // There are MUCH more thorough tests throughout etc/test/

    auto tz = vtz::locate_zone( "America/New_York" );

    using std::chrono::seconds;

    // Check date before DST
    auto T = vtz::parse<seconds>( "%F %T", "2025-02-25 18:12:00" );
    ASSERT_EQ( tz->format( "%F %T %Z", T ), "2025-02-25 13:12:00 EST" );

    // Check date after DST
    auto T2 = vtz::parse<seconds>( "%F %T", "2025-06-25 18:12:00" );
    ASSERT_EQ( tz->format( "%F %T %Z", T2 ), "2025-06-25 14:12:00 EDT" );
}

class zones : public testing::TestWithParam<std::string_view> {};

static_assert( std::is_same_v<vtz::sys_seconds, date::sys_seconds> );


using std::chrono::seconds;
using vtz::sys_seconds;

struct test_state : vtz::sys_info {
    vtz::local_seconds to_local( sys_seconds T ) const {
        return _to_local( T, offset );
    }
};

std::vector<test_state> extract_states(
    date::time_zone const* tz, sys_seconds T, sys_seconds Tmax ) {
    std::vector<test_state> result;

    while( T < Tmax )
    {
        auto info = tz->get_info( T );
        result.push_back( test_state{
            info.begin,
            info.end,
            info.offset,
            info.save,
            info.abbrev,
        } );
        T = info.end;
    }

    return result;
}

using namespace std::chrono_literals;

TEST_P( zones, tz_api ) {
    using vtz::choose;

    auto tz_name = GetParam();

    auto tz = vtz::locate_zone( tz_name );

    auto states = extract_states( date::locate_zone( tz_name ),
        _get_time( 1800, 1, 1 ),
        _get_time( 2900, 1, 1 ) );

    std::vector<sys_seconds> TT;

    // Check that expected properties for state transitions hold
    for( size_t i = 1; i < states.size(); ++i )
    {
        auto const& s0    = states[i - 1];
        auto const& s1    = states[i];
        auto        first = s0.begin;
        auto        last  = s0.end - 1s;

        auto ctx = _ctx{ [&] {
            return fmt::format( "i      = {}\n"
                                "first  = {}\n"
                                "last   = {}\n"
                                "offset = {}s\n",
                i,
                _fmt( first ),
                _fmt( last ),
                _to_str( s0.offset ) );
        } };


        ASSERT_EQ( tz->offset( first ), s0.offset ) << ctx;
        ASSERT_EQ( tz->to_local( first ), s0.to_local( first ) ) << ctx;
        ASSERT_EQ( tz->abbrev( first ), s0.abbrev ) << ctx;

        ASSERT_EQ( tz->offset( last ), s0.offset ) << ctx;
        ASSERT_EQ( tz->to_local( last ), s0.to_local( last ) ) << ctx;
        ASSERT_EQ( tz->abbrev( last ), s0.abbrev ) << ctx;

        ASSERT_EQ( tz->offset( s0.end ), s1.offset ) << ctx;
        ASSERT_EQ( tz->to_local( s0.end ), s1.to_local( s0.end ) ) << ctx;
        ASSERT_EQ( tz->abbrev( s0.end ), s1.abbrev ) << ctx;

        auto first_local = s0.to_local( first );
        auto last_local  = s0.to_local( last );

        ASSERT_EQ( tz->to_sys( first_local, choose::latest ), first ) << ctx;
        ASSERT_EQ( tz->to_sys( last_local, choose::earliest ), last ) << ctx;

        auto delta = s1.offset - s0.offset;
        // Sanity check - delta should be smaller than ~30h in either direction
        ASSERT_LT( -30h, delta );
        ASSERT_LT( delta, 30h );

        if( s0.offset < s1.offset )
        {
            // This case occurs when the clock springs forward. delta is
            // positive

            auto first_ne = last_local + 1s;    ///< First nonexistent time
            auto last_ne  = last_local + delta; ///< Last nonexistent time

            ASSERT_EQ( tz->local_type( last_local ), local_type::unique );
            ASSERT_EQ( tz->local_type( first_ne ), local_type::nonexistent );
            ASSERT_EQ( tz->local_type( last_ne ), local_type::nonexistent );
            ASSERT_EQ( tz->local_type( last_ne + 1s ), local_type::unique );

            // All nonexistent times should map to the transition time
            ASSERT_EQ( tz->to_sys( first_ne, choose::earliest ), s0.end );
            ASSERT_EQ( tz->to_sys( first_ne, choose::latest ), s0.end );
            ASSERT_EQ( tz->to_sys( last_ne, choose::earliest ), s0.end );
            ASSERT_EQ( tz->to_sys( last_ne, choose::latest ), s0.end );

            // THese are unique so to_sys won't throw here
            ASSERT_EQ( tz->to_sys( last_local ), last );
            ASSERT_EQ( tz->to_sys( last_ne + 1s ), s0.end );
        }
        else if( s0.offset == s1.offset )
        {
            // This case occurs when the abbreviation changes (or when 'date'
            // split a sys_info into two parts, that really ought not to have
            // been split)

            ASSERT_EQ( tz->local_type( last_local ), local_type::unique );
            ASSERT_EQ( tz->local_type( last_local + 1s ), local_type::unique );

            ASSERT_EQ( tz->to_sys( last_local ), last );
            ASSERT_EQ( tz->to_sys( last_local + 1s ), last + 1s );
        }
        else
        {
            // This case occurs when the clock falls back. delta is negative.

            // Example: when DST ends for America/New_York,
            // - last_local is at 1:59:59
            // - delta is -1h
            // - first_ambig = (1:59:59 + (-1h) + 1s) = 1:00:00
            // - last_ambig = 1:59:59
            auto first_ambig = last_local + delta + 1s;
            auto last_ambig  = last_local;

            ASSERT_EQ( tz->local_type( first_ambig - 1s ), local_type::unique );
            ASSERT_EQ( tz->local_type( first_ambig ), local_type::ambiguous );
            ASSERT_EQ( tz->local_type( last_ambig ), local_type::ambiguous );
            ASSERT_EQ( tz->local_type( last_ambig + 1s ), local_type::unique );

            // Eg, for America/New_York, the first ambiguous time is 1:00 am
            // this can either be s0.end + (-1h), or s0.end
            ASSERT_EQ(
                tz->to_sys( first_ambig, choose::earliest ), s0.end + delta );
            ASSERT_EQ( tz->to_sys( first_ambig, choose::latest ), s0.end );

            // Eg, for America/New_York, the last ambiguous time is 1:59:59 am
            // This can either be s0.end - 1s (1s before we switch over to
            // standard time), or it can be s0.end - 1s - (-1h) = s0.end +1h -
            // 1s (0:59:59 after the switch)
            ASSERT_EQ(
                tz->to_sys( last_ambig, choose::earliest ), s0.end - 1s );
            ASSERT_EQ(
                tz->to_sys( last_ambig, choose::latest ), s0.end - 1s - delta );
        }
    }


    for( auto const& state : states )
    {
        TT.clear();
        TT.push_back( state.begin );
        TT.push_back( state.end - 1s );
        for( auto T : TT )
        {
            auto ctx = _ctx{ [&] {
                return fmt::format( "T           = {}\n"
                                    "state.begin = {}\n"
                                    "state.end   = {}\n",
                    _fmt( T ),
                    _fmt( state.begin ),
                    _fmt( state.end ) );
            } };

            auto Tlocal = local_seconds( T.time_since_epoch() + state.offset );

            ASSERT_EQ( tz->offset( T ), state.offset );
            ASSERT_EQ( tz->to_local( T ), Tlocal );
            ASSERT_EQ( tz->abbrev( T ), state.abbrev );

            auto info = tz->get_info( T );

            // We know that the interval itself must be sane
            ASSERT_LT( info.begin, info.end ) << ctx;

            // We know that info.begin <= T < info.end
            ASSERT_LE( info.begin, T ) << ctx;
            ASSERT_LT( T, info.end ) << ctx;

            // Range of time covered by vtz::sys_info returned by get_info
            // should match or exceed that returned by `date`
            ASSERT_LE( info.begin, state.begin ) << ctx;
            ASSERT_GE( info.end, state.end ) << ctx;
        }
    }
}


auto name = []( testing::TestParamInfo<std::string_view> const& info ) {
    return esc_zone_name( info.param );
};


INSTANTIATE_TEST_SUITE_P(
    vtz, zones, testing::ValuesIn( get_test_zones() ), name );
