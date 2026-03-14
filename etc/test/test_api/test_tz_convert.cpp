#include <test_vtz/utils.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <vtz/parse.h>
#include <vtz/tz.h>

using namespace vtz;
using testing::HasSubstr;


/// Parse a "%F %T" datetime string as a local_seconds
static local_seconds _pl( string_view s ) {
    return parse_local<seconds>( "%F %T", s );
}

/// Parse a "%F %T" datetime string as a sys_seconds
static sys_seconds _ps( string_view s ) { return parse<seconds>( "%F %T", s ); }


TEST( vtz_to_sys, sanity ) {
    auto const* ny = locate_zone( "America/New_York" );

    // Unambiguous times: both overloads must agree

    // Winter (EST, UTC-5): 2025-01-15 12:00 local -> 17:00 UTC
    {
        auto lt  = _pl( "2025-01-15 12:00:00" );
        auto exp = _ps( "2025-01-15 17:00:00" );
        EXPECT_EQ( ny->to_sys( lt ), exp );
        EXPECT_EQ( ny->to_sys( lt, choose::earliest ), exp );
        EXPECT_EQ( ny->to_sys( lt, choose::latest ), exp );
    }

    // Summer (EDT, UTC-4): 2025-07-04 15:30:00 local -> 19:30 UTC
    {
        auto lt  = _pl( "2025-07-04 15:30:00" );
        auto exp = _ps( "2025-07-04 19:30:00" );
        EXPECT_EQ( ny->to_sys( lt ), exp );
        EXPECT_EQ( ny->to_sys( lt, choose::earliest ), exp );
        EXPECT_EQ( ny->to_sys( lt, choose::latest ), exp );
    }

    // Ambiguous time (fall-back): 2025-11-02 01:30 AM
    // On this date the clocks fall back at 2AM EDT -> 1AM EST, so 1:30 AM
    // occurs twice.
    {
        auto lt       = _pl( "2025-11-02 01:30:00" );
        auto earliest = _ps( "2025-11-02 05:30:00" ); // EDT (UTC-4)
        auto latest   = _ps( "2025-11-02 06:30:00" ); // EST (UTC-5)

        EXPECT_EQ( ny->to_sys( lt, choose::earliest ), earliest );
        EXPECT_EQ( ny->to_sys( lt, choose::latest ), latest );

        // The throwing overload must reject ambiguous times
        try
        {
            ny->to_sys( lt );
            ADD_FAILURE() << "expected exception for ambiguous time";
        }
        catch( std::exception const& e )
        { EXPECT_THAT( e.what(), HasSubstr( "ambiguous" ) ); }
    }

    // Nonexistent time (spring-forward): 2025-03-09 02:30 AM
    // On this date the clocks spring forward at 2AM EST -> 3AM EDT, so
    // 2:30 AM doesn't exist.  All nonexistent times in the gap map to the
    // same UTC instant (7AM UTC).
    {
        auto lt  = _pl( "2025-03-09 02:30:00" );
        auto exp = _ps( "2025-03-09 07:00:00" );

        EXPECT_EQ( ny->to_sys( lt, choose::earliest ), exp );
        EXPECT_EQ( ny->to_sys( lt, choose::latest ), exp );

        // The throwing overload must reject nonexistent times
        try
        {
            ny->to_sys( lt );
            ADD_FAILURE() << "expected exception for nonexistent time";
        }
        catch( std::exception const& e )
        { EXPECT_THAT( e.what(), HasSubstr( "nonexistent" ) ); }
    }

    // Round-trip: to_sys(to_local(t)) == t for unambiguous times
    {
        sys_seconds times[] = {
            _ps( "2025-01-15 12:00:00" ),
            _ps( "2025-07-04 19:30:00" ),
            _ps( "2025-06-01 00:00:00" ),
            _ps( "1999-12-31 23:59:59" ),
        };

        for( auto t : times )
        {
            auto local_t = ny->to_local( t );
            EXPECT_EQ( ny->to_sys( local_t ), t );
        }
    }

    // Sub-second precision: template<Dur> overloads
    {
        using ms = std::chrono::milliseconds;

        auto lt = local_time<ms>(
            _pl( "2025-01-15 12:00:00" ).time_since_epoch() + ms( 500 ) );
        auto exp = sys_time<ms>(
            _ps( "2025-01-15 17:00:00" ).time_since_epoch() + ms( 500 ) );

        EXPECT_EQ( ny->to_sys( lt ), exp );
        EXPECT_EQ( ny->to_sys( lt, choose::earliest ), exp );
    }

    // Fixed-offset zone (UTC)
    {
        auto const* utc = locate_zone( "UTC" );
        auto        lt  = _pl( "2025-06-15 08:00:00" );
        auto        exp = _ps( "2025-06-15 08:00:00" );
        EXPECT_EQ( utc->to_sys( lt ), exp );
        EXPECT_EQ( utc->to_sys( lt, choose::earliest ), exp );
    }

    // Asia/Kolkata (UTC+5:30, no DST): 2025-01-15 12:00 local -> 06:30 UTC
    {
        auto const* kol = locate_zone( "Asia/Kolkata" );
        auto        lt  = _pl( "2025-01-15 12:00:00" );
        auto        exp = _ps( "2025-01-15 06:30:00" );
        EXPECT_EQ( kol->to_sys( lt ), exp );
    }
}


TEST( vtz_to_sys, ambiguity ) {
    auto const* ny = locate_zone( "America/New_York" );

    // Spring forward 2025-03-09: clocks jump from 2:00 AM EST to 3:00 AM EDT.
    // Local times in [02:00:00, 03:00:00) are nonexistent.
    {
        auto gap_start = _pl( "2025-03-09 02:00:00" );
        auto gap_end   = _pl( "2025-03-09 03:00:00" );

        // Every second in the gap is nonexistent
        for( auto t = gap_start; t < gap_end; t += seconds( 1 ) )
        {
            auto info = ny->get_info( t );
            ASSERT_EQ( info.result, local_info::nonexistent )
                << t.time_since_epoch().count();

            bool threw = false;
            try
            { ny->to_sys( t ); }
            catch( std::runtime_error& )
            { threw = true; }
            ASSERT_TRUE( threw ) << t.time_since_epoch().count();
        }

        // One second before the gap is unique
        auto before = gap_start - seconds( 1 );
        EXPECT_EQ( ny->get_info( before ).result, local_info::unique );
        EXPECT_EQ( ny->to_sys( before ), _ps( "2025-03-09 06:59:59" ) );

        // First second after the gap is unique
        EXPECT_EQ( ny->get_info( gap_end ).result, local_info::unique );
        EXPECT_EQ( ny->to_sys( gap_end ), _ps( "2025-03-09 07:00:00" ) );

        // Verify get_info contents for a nonexistent time
        auto info = ny->get_info( _pl( "2025-03-09 02:30:00" ) );
        EXPECT_EQ( info.first.offset, seconds( -5 * 3600 ) );  // EST
        EXPECT_EQ( info.second.offset, seconds( -4 * 3600 ) ); // EDT
    }

    // Fall back 2025-11-02: clocks fall from 2:00 AM EDT to 1:00 AM EST.
    // Local times in [01:00:00, 02:00:00) are ambiguous.
    {
        auto amb_start = _pl( "2025-11-02 01:00:00" );
        auto amb_end   = _pl( "2025-11-02 02:00:00" );

        // Every second in the range is ambiguous
        for( auto t = amb_start; t < amb_end; t += seconds( 1 ) )
        {
            auto info = ny->get_info( t );
            ASSERT_EQ( info.result, local_info::ambiguous )
                << t.time_since_epoch().count();

            bool threw = false;
            try
            { ny->to_sys( t ); }
            catch( std::runtime_error& )
            { threw = true; }
            ASSERT_TRUE( threw ) << t.time_since_epoch().count();
        }

        // One second before the ambiguous range is unique
        auto before = amb_start - seconds( 1 );
        EXPECT_EQ( ny->get_info( before ).result, local_info::unique );
        EXPECT_EQ( ny->to_sys( before ), _ps( "2025-11-02 04:59:59" ) );

        // First second after the ambiguous range is unique
        EXPECT_EQ( ny->get_info( amb_end ).result, local_info::unique );
        EXPECT_EQ( ny->to_sys( amb_end ), _ps( "2025-11-02 07:00:00" ) );

        // Verify get_info contents for an ambiguous time
        auto info = ny->get_info( _pl( "2025-11-02 01:30:00" ) );
        EXPECT_EQ( info.first.offset, seconds( -4 * 3600 ) );  // EDT (earlier)
        EXPECT_EQ( info.second.offset, seconds( -5 * 3600 ) ); // EST (later)
    }
}


TEST( vtz_to_local, sanity ) {
    auto const* ny = locate_zone( "America/New_York" );

    // Winter (EST, UTC-5): 2025-01-15 17:00 UTC -> 12:00 local
    {
        auto utc = _ps( "2025-01-15 17:00:00" );
        auto exp = _pl( "2025-01-15 12:00:00" );
        EXPECT_EQ( ny->to_local( utc ), exp );
    }

    // Summer (EDT, UTC-4): 2025-07-04 19:30 UTC -> 15:30 local
    {
        auto utc = _ps( "2025-07-04 19:30:00" );
        auto exp = _pl( "2025-07-04 15:30:00" );
        EXPECT_EQ( ny->to_local( utc ), exp );
    }

    // Across midnight: late UTC time maps to previous local day
    // 2025-01-16 03:00 UTC -> 2025-01-15 22:00 local (EST, UTC-5)
    {
        auto utc = _ps( "2025-01-16 03:00:00" );
        auto exp = _pl( "2025-01-15 22:00:00" );
        EXPECT_EQ( ny->to_local( utc ), exp );
    }

    // UTC itself: offset is zero
    {
        auto const* utc_tz = locate_zone( "UTC" );
        auto        t      = _ps( "2025-06-15 08:00:00" );
        auto        exp    = _pl( "2025-06-15 08:00:00" );
        EXPECT_EQ( utc_tz->to_local( t ), exp );
    }

    // Non-integer-hour offset: Asia/Kolkata (UTC+5:30)
    // 2025-01-15 06:30 UTC -> 12:00 local
    {
        auto const* kol = locate_zone( "Asia/Kolkata" );
        auto        utc = _ps( "2025-01-15 06:30:00" );
        auto        exp = _pl( "2025-01-15 12:00:00" );
        EXPECT_EQ( kol->to_local( utc ), exp );
    }

    // Round-trip: to_local(to_sys(lt)) == lt for unambiguous times
    {
        local_seconds locals[] = {
            _pl( "2025-01-15 12:00:00" ),
            _pl( "2025-07-04 15:30:00" ),
            _pl( "2025-06-01 00:00:00" ),
            _pl( "1999-12-31 23:59:59" ),
        };

        for( auto lt : locals )
        {
            auto sys_t = ny->to_sys( lt );
            EXPECT_EQ( ny->to_local( sys_t ), lt );
        }
    }

    // Sub-second precision
    {
        using ms = std::chrono::milliseconds;

        auto utc = sys_time<ms>(
            _ps( "2025-01-15 17:00:00" ).time_since_epoch() + ms( 250 ) );
        auto exp = local_time<ms>(
            _pl( "2025-01-15 12:00:00" ).time_since_epoch() + ms( 250 ) );
        EXPECT_EQ( ny->to_local( utc ), exp );
    }

    // At the transition instant itself
    // The UTC instant when DST starts (2025-03-09 07:00 UTC) is unambiguous.
    // to_local should return 3:00 AM EDT (not 2:00 AM EST, which no longer
    // applies).
    {
        auto utc = _ps( "2025-03-09 07:00:00" );
        auto exp = _pl( "2025-03-09 03:00:00" );
        EXPECT_EQ( ny->to_local( utc ), exp );
    }

    // One second before the spring-forward transition: still EST
    {
        auto utc = _ps( "2025-03-09 06:59:59" );
        auto exp = _pl( "2025-03-09 01:59:59" );
        EXPECT_EQ( ny->to_local( utc ), exp );
    }
}
