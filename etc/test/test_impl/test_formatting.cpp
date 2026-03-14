#include <gtest/gtest.h>
#include <random>
#include <vtz/date_types.h>
#include <vtz/files.h>
#include <vtz/strings.h>
#include <vtz/tz_impl.h>
#include <vtz/tz_reader.h>

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>

#include <vtz/civil.h>
#include <vtz/tz.h>
#include <vtz/tz_cache.h>

#include "test_utils.h"
#include "test_zones.h"
#include "vtz_testing.h"

#include <date/date.h>
#include <date/tz.h>


using namespace vtz;

TEST( vtz, format_time ) {
    COUNT_ASSERTIONS();

    auto fmt_h = []( date::time_zone const* tz, sys_seconds s, char const* format ) {
        return date::format( format, date::make_zoned( tz, s ) );
    };
    auto fmt_vtz = []( vtz::time_zone const* tz, sys_seconds s, char const* format ) {
        return tz->format( format, s );
    };

    // Do sanity check - we know how these should be formatted
    {
        auto T    = sys_seconds( seconds( _ct( 2025, 11, 6, 15, 24, 45 ) ) );
        auto NY_H = date::locate_zone( "America/New_York" );
        auto NY_V = vtz::locate_zone( "America/New_York" );
        auto De_H = date::locate_zone( "America/Denver" );
        auto De_V = vtz::locate_zone( "America/Denver" );

        // Check that America/New_York matches
        ASSERT_EQ( fmt_h( NY_H, T, "%Y%m%d %H:%M:%S-%Z" ), "20251106 10:24:45-EST" );
        ASSERT_EQ( fmt_vtz( NY_V, T, "%Y%m%d %H:%M:%S-%Z" ), "20251106 10:24:45-EST" );

        // Check that America/Denver matches
        ASSERT_EQ( fmt_h( De_H, T, "%Y%m%d %H:%M:%S-%Z" ), "20251106 08:24:45-MST" );
        ASSERT_EQ( fmt_vtz( De_V, T, "%Y%m%d %H:%M:%S-%Z" ), "20251106 08:24:45-MST" );

        // Check some other formats
        ASSERT_EQ( fmt_vtz( NY_V, T, "%y%m%d %H:%M:%S-%Z" ), "251106 10:24:45-EST" );
        ASSERT_EQ( fmt_vtz( De_V, T, "%y%m%d %H:%M:%S-%Z" ), "251106 08:24:45-MST" );

        ASSERT_EQ( fmt_vtz( NY_V, T, "%F %T %Z" ), "2025-11-06 10:24:45 EST" );
        ASSERT_EQ( fmt_vtz( De_V, T, "%F %T %Z" ), "2025-11-06 08:24:45 MST" );

        ASSERT_EQ( fmt_vtz( NY_V, T, "%D %T %Z" ), "11/06/25 10:24:45 EST" );
        ASSERT_EQ( fmt_vtz( De_V, T, "%D %T %Z" ), "11/06/25 08:24:45 MST" );

        // Check the case that the string does NOT end in a format specifier (ensure the last
        // character is copied)
        ASSERT_EQ( fmt_vtz( NY_V, T, "%Y%m%d %H:%M:%S-%Zx" ), "20251106 10:24:45-ESTx" );
        ASSERT_EQ( fmt_vtz( NY_V, T, "%Y%m%d %H:%M:%S-%Zxyz" ), "20251106 10:24:45-ESTxyz" );
        ASSERT_EQ( fmt_vtz( NY_V, T, "%Y%m%d %H:%M:%Sx" ), "20251106 10:24:45x" );
        ASSERT_EQ( fmt_vtz( NY_V, T, "%Y%m%d %H:%M:%Sxyz" ), "20251106 10:24:45xyz" );

        ASSERT_EQ( NY_V->format( "%F %T %Z", T ), "2025-11-06 10:24:45 EST" );

        auto t          = T.time_since_epoch().count();
        using _25th     = duration<int, std::ratio<1, 25>>;
        using _3rd      = duration<int, std::ratio<1, 3>>;
        using millis    = std::chrono::milliseconds;
        using micros    = std::chrono::microseconds;
        using nanos     = std::chrono::nanoseconds;
        using seconds_r = std::chrono::duration<double>;
        using std::chrono::hours;
        using std::chrono::minutes;

        // Adding minutes or hours shouldn't affect the precision
        ASSERT_EQ( NY_V->format( "%F %T %Z", T + minutes( 10 ) ), "2025-11-06 10:34:45 EST" );
        ASSERT_EQ( NY_V->format( "%F %T %Z", T + hours( 24 ) ), "2025-11-07 10:24:45 EST" );

        // Only the necessary amount of precision (for a given type) is added, but the same
        // amount of precision is always added
        ASSERT_EQ( NY_V->format( "%F %T %Z", T + _25th( 0 ) ), "2025-11-06 10:24:45.00 EST" );
        ASSERT_EQ( NY_V->format( "%F %T %Z", T + _25th( 3 ) ), "2025-11-06 10:24:45.12 EST" );
        ASSERT_EQ( NY_V->format( "%F %T %Z", T + _3rd( 1 ) ), "2025-11-06 10:24:45.333333333 EST" );
        ASSERT_EQ( NY_V->format( "%F %T %Z", T + _3rd( 2 ) ), "2025-11-06 10:24:45.666666666 EST" );
        ASSERT_EQ( NY_V->format( "%F %T %Z", T + _3rd( 3 ) ), "2025-11-06 10:24:46.000000000 EST" );

        ASSERT_EQ( NY_V->format( "%F %T %Z", T + millis( 3295 ) ), "2025-11-06 10:24:48.295 EST" );
        ASSERT_EQ( NY_V->format( "%F %T %Z", T + micros( 3295 ) ),
                   "2025-11-06 10:24:45.003295 EST" );
        ASSERT_EQ( NY_V->format( "%F %T %Z", T + nanos( 3295 ) ),
                   "2025-11-06 10:24:45.000003295 EST" );

        // Using floating-point for the duration always results in 9 digits of precision
        ASSERT_EQ( NY_V->format( "%F %T %Z", T + seconds_r( 0.375 ) ),
                   "2025-11-06 10:24:45.375000000 EST" );

        // We can request that a given quantity of precision always be used
        ASSERT_EQ( NY_V->format_precise( "%F %T %Z", T, 3 ), "2025-11-06 10:24:45.000 EST" );
        ASSERT_EQ( NY_V->format_precise( "%F %T %Z", T + _25th( 3 ), 3 ),
                   "2025-11-06 10:24:45.120 EST" );
        ASSERT_EQ( NY_V->format_precise( "%F %T %Z", T + micros( 3295 ), 3 ),
                   "2025-11-06 10:24:45.003 EST" );
        ASSERT_EQ( NY_V->format_precise( "%F %T %Z", T + seconds_r( 0.375 ), 3 ),
                   "2025-11-06 10:24:45.375 EST" );

        ASSERT_EQ( NY_V->format_precise_s( "%F %T %Z", t, 987654321, 9 ),
                   "2025-11-06 10:24:45.987654321 EST" );
        ASSERT_EQ( NY_V->format_precise_s( "%F %T %Z", t, 987654321, 8 ),
                   "2025-11-06 10:24:45.98765432 EST" );
        ASSERT_EQ( NY_V->format_precise_s( "%F %T %Z", t, 987654321, 7 ),
                   "2025-11-06 10:24:45.9876543 EST" );
        ASSERT_EQ( NY_V->format_precise_s( "%F %T %Z", t, 987654321, 6 ),
                   "2025-11-06 10:24:45.987654 EST" );
        ASSERT_EQ( NY_V->format_precise_s( "%F %T %Z", t, 987654321, 5 ),
                   "2025-11-06 10:24:45.98765 EST" );
        ASSERT_EQ( NY_V->format_precise_s( "%F %T %Z", t, 987654321, 4 ),
                   "2025-11-06 10:24:45.9876 EST" );
        ASSERT_EQ( NY_V->format_precise_s( "%F %T %Z", t, 987654321, 3 ),
                   "2025-11-06 10:24:45.987 EST" );
        ASSERT_EQ( NY_V->format_precise_s( "%F %T %Z", t, 987654321, 2 ),
                   "2025-11-06 10:24:45.98 EST" );
        ASSERT_EQ( NY_V->format_precise_s( "%F %T %Z", t, 987654321, 1 ),
                   "2025-11-06 10:24:45.9 EST" );
        ASSERT_EQ( NY_V->format_precise_s( "%F %T %Z", t, 987654321, 0 ),
                   "2025-11-06 10:24:45 EST" );
    }

    {
        // `%z` is specified as:
        //
        //     %z: writes offset from UTC in the ISO 8601 format (e.g. -0430), or no characters if
        //     the time zone information is not available
        //
        // However, ISO 8601 specifies multiple ways to express this format:
        //
        //     <time>±hh:mm
        //     <time>±hhmm
        //     <time>±hh
        //
        // Hinnant's date library seems to always use ±hhmm, however our implementation uses the
        // shortest possible representation of the offset (as this is more aesthetic).
        //
        // Therefore, we need to test it separately (here), instead of using Hinnant's library as
        // our reference implementation.

        auto NY = vtz::locate_zone( "America/New_York" );

        // Australia/Eucla is a nice test case because it has a offset of +0845,
        // which allows us to check that we handle positive offsets correctly
        auto Eucla = vtz::locate_zone( "Australia/Eucla" );

        auto T_NY_lmt  = sys_seconds( seconds( _ct( 1883, 11, 18, 16, 59, 59 ) ) );
        auto T_NY_rail = T_NY_lmt + seconds( 1 );
        ASSERT_EQ( "zoneoff: -045602", fmt_vtz( NY, T_NY_lmt, "zoneoff: %z" ) );
        ASSERT_EQ( "zoneoff: -05", fmt_vtz( NY, T_NY_rail, "zoneoff: %z" ) );

        ASSERT_EQ( "1883-11-18 12:03:57 LMT", fmt_vtz( NY, T_NY_lmt, "%F %T %Z" ) );
        ASSERT_EQ( "1883-11-18 12:00:00 EST", fmt_vtz( NY, T_NY_rail, "%F %T %Z" ) );

        ASSERT_EQ( "1883-11-18 12:03:57 -045602", fmt_vtz( NY, T_NY_lmt, "%F %T %z" ) );
        ASSERT_EQ( "1883-11-18 12:00:00 -05", fmt_vtz( NY, T_NY_rail, "%F %T %z" ) );

        auto T = sys_seconds( seconds( _ct( 2025, 11, 6, 12, 0, 37 ) ) );
        ASSERT_EQ( "+0845", fmt_vtz( Eucla, T, "%z" ) );
        ASSERT_EQ( "2025-11-06 20:45:37 +0845", fmt_vtz( Eucla, T, "%F %T %z" ) );

        // At time of writing, Eucla uses '%z' for the format specifier for the zone, so rather than
        // a 3-letter abbreviation (such as 'EST' or 'EDT'), %Z should be the same as %z:
        ASSERT_EQ( "2025-11-06 20:45:37 +0845", fmt_vtz( Eucla, T, "%F %T %Z" ) );
        ASSERT_EQ( "+0845", fmt_vtz( Eucla, T, "%Z" ) );
    }

    constexpr sys_seconds_t start_t = days_to_seconds( resolve_civil( 1800, 1, 1 ) );
    constexpr sys_seconds_t end_t   = days_to_seconds( resolve_civil( 2900, 1, 1 ) );

    auto rng  = std::mt19937_64{};
    auto dist = std::uniform_int_distribution<sys_seconds_t>( start_t, end_t );

    for( auto const& zone : ALL_ZONES )
    {
        auto tz_h = date::locate_zone( zone );
        auto tz_v = vtz::locate_zone( zone );
        for( size_t i = 0; i < 100; ++i )
        {
            auto T = sys_seconds( seconds( dist( rng ) ) );
            ASSERT_EQ_QUIET( fmt_vtz( tz_v, T, "%y%m%d %H:%M:%S-%Z" ),
                             fmt_h( tz_h, T, "%y%m%d %H:%M:%S-%Z" ) );
            ASSERT_EQ_QUIET( fmt_vtz( tz_v, T, "%F %T %Z" ), fmt_h( tz_h, T, "%F %T %Z" ) );
            ASSERT_EQ_QUIET( fmt_vtz( tz_v, T, "%D %T %Z" ), fmt_h( tz_h, T, "%D %T %Z" ) );
        }
    }
}
