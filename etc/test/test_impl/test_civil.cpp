#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/impl/math.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>

#include <vtz/libfmt_compat.h>

#include "vtz_debug.h"
#include "vtz_testing.h"

#include <algorithm>
#include <random>
using namespace vtz;

TEST( vtz_math, div_floor ) {
    using vtz::math::div_floor;
    using vtz::math::div_floor2;
    using vtz::math::div_t;

    static_assert( div_floor2<5>( -5 ) == div_t{ -1, 0 } );
    static_assert( div_floor2<5>( -4 ) == div_t{ -1, 1 } );
    static_assert( div_floor2<5>( -3 ) == div_t{ -1, 2 } );
    static_assert( div_floor2<5>( -2 ) == div_t{ -1, 3 } );
    static_assert( div_floor2<5>( -1 ) == div_t{ -1, 4 } );
    static_assert( div_floor2<5>( 0 ) == div_t{ 0, 0 } );
    static_assert( div_floor2<5>( 1 ) == div_t{ 0, 1 } );
    static_assert( div_floor2<5>( 2 ) == div_t{ 0, 2 } );
    static_assert( div_floor2<5>( 3 ) == div_t{ 0, 3 } );
    static_assert( div_floor2<5>( 4 ) == div_t{ 0, 4 } );
    static_assert( div_floor2<5>( 5 ) == div_t{ 1, 0 } );

    ASSERT_EQ( div_floor2<5>( -5 ), ( div_t{ -1, 0 } ) );
    ASSERT_EQ( div_floor2<5>( -4 ), ( div_t{ -1, 1 } ) );
    ASSERT_EQ( div_floor2<5>( -3 ), ( div_t{ -1, 2 } ) );
    ASSERT_EQ( div_floor2<5>( -2 ), ( div_t{ -1, 3 } ) );
    ASSERT_EQ( div_floor2<5>( -1 ), ( div_t{ -1, 4 } ) );
    ASSERT_EQ( div_floor2<5>( 0 ), ( div_t{ 0, 0 } ) );
    ASSERT_EQ( div_floor2<5>( 1 ), ( div_t{ 0, 1 } ) );
    ASSERT_EQ( div_floor2<5>( 2 ), ( div_t{ 0, 2 } ) );
    ASSERT_EQ( div_floor2<5>( 3 ), ( div_t{ 0, 3 } ) );
    ASSERT_EQ( div_floor2<5>( 4 ), ( div_t{ 0, 4 } ) );
    ASSERT_EQ( div_floor2<5>( 5 ), ( div_t{ 1, 0 } ) );
}
namespace {
    constexpr u8 DAYS_IN_EACH_MONTH[]{
        0,
        31, // Jan
        28, // Feb
        31, // Mar
        30, // Apr
        31, // May
        30, // Jun
        31, // Jul
        31, // Aug
        30, // Sep
        31, // Oct
        30, // Nov
        31, // Dec
    };

    u8 days_in_month_reference( int year, u8 month ) {
        if( month == 2 )
        {
            bool is_leap = year % 4 == 0 && ( year % 400 == 0 || year % 100 != 0 );
            return is_leap ? 29 : 28;
        }
        return DAYS_IN_EACH_MONTH[month];
    }

    constexpr civil_ymd ymd( i32 year, i32 mon, i32 day ) noexcept {
        return { year, u16( mon ), u16( day ) };
    }

    /// Reference implementation of clamped month addition. Shift the year and
    /// month, then clamp the day of the month to the last day of the target
    /// month, so that Jan 31st + 1 month is Feb 28th rather than Mar 3rd.
    sys_days_t add_months_clamped_reference( int year, int month, int day, int k ) {
        auto parts    = math::div_floor2<12>( month + k - 1 );
        int  year2    = year + parts.quot;
        int  month2   = parts.rem + 1;
        int  last_dom = days_in_month_reference( year2, u8( month2 ) );
        return resolve_civil( year2, u32( month2 ), u32( std::min( day, last_dom ) ) );
    }

    /// Reference implementation of clamped year addition. Shift the year, then
    /// clamp the day of the month, so that Feb 29th + 1 year is Feb 28th
    /// rather than Mar 1st.
    sys_days_t add_years_clamped_reference( int year, int month, int day, int k ) {
        int year2    = year + k;
        int last_dom = days_in_month_reference( year2, u8( month ) );
        return resolve_civil( year2, u32( month ), u32( std::min( day, last_dom ) ) );
    }

    /// The reference implementations below work entirely in i64 and are
    /// independent of anything in civil.h, so they stay valid however the
    /// implementation is rewritten, and cannot themselves overflow at the edges
    /// of the i32 day range that the fuzzing tests reach.
    namespace ref {
        constexpr bool is_leap( i64 y ) noexcept {
            return y % 4 == 0 && ( y % 100 != 0 || y % 400 == 0 );
        }

        constexpr int days_in_month( i64 y, int m /* 1-based */ ) noexcept {
            return m == 2 ? ( is_leap( y ) ? 29 : 28 ) : int( DAYS_IN_EACH_MONTH[m] );
        }

        /// Floor division, so negative years behave the same way the
        /// implementation does
        constexpr i64 fdiv( i64 a, i64 b ) noexcept {
            i64 q = a / b;
            return ( a % b != 0 && ( ( a < 0 ) != ( b < 0 ) ) ) ? q - 1 : q;
        }
        constexpr i64 fmod( i64 a, i64 b ) noexcept { return a - b * fdiv( a, b ); }

        struct ymd {
            i64 year;
            int month, day; // both 1-based
        };

        /// days since 1970-01-01 -> (year, month, day)
        constexpr ymd to_civil( i64 z ) noexcept {
            z       += 719468;
            i64 era  = fdiv( z, 146097 );
            i64 doe  = z - era * 146097;
            i64 yoe  = ( doe - doe / 1460 + doe / 36524 - doe / 146096 ) / 365;
            i64 y    = yoe + era * 400;
            i64 doy  = doe - ( 365 * yoe + yoe / 4 - yoe / 100 );
            i64 mp   = ( 5 * doy + 2 ) / 153;
            i64 d    = doy - ( 153 * mp + 2 ) / 5 + 1;
            i64 m    = mp < 10 ? mp + 3 : mp - 9;
            return ymd{ y + ( m <= 2 ), int( m ), int( d ) };
        }

        /// (year, month, day) -> days since 1970-01-01
        constexpr i64 resolve_civil( i64 y, int m, int d ) noexcept {
            y       -= m <= 2;
            i64 era  = fdiv( y, 400 );
            i64 yoe  = y - era * 400;
            i64 doy  = ( 153 * ( m > 2 ? m - 3 : m + 9 ) + 2 ) / 5 + d - 1;
            i64 doe  = yoe * 365 + yoe / 4 - yoe / 100 + doy;
            return era * 146097 + doe - 719468;
        }

        /// Add months. @p clamp selects whether the day of the month is clamped
        /// to the target month's last day, or allowed to roll over.
        constexpr i64 add_months( i64 days, int months, bool clamp ) noexcept {
            ymd t  = to_civil( days );
            i64 m0 = i64( t.month ) - 1 + months; // 0-based
            i64 y2 = t.year + fdiv( m0, 12 );
            int m2 = int( fmod( m0, 12 ) ) + 1;
            int d  = t.day;
            if( clamp )
            {
                int last = days_in_month( y2, m2 );
                if( d > last ) d = last;
            }
            return resolve_civil( y2, m2, d );
        }

        /// Add years, with the same clamping choice as add_months
        constexpr i64 add_years( i64 days, int years, bool clamp ) noexcept {
            ymd t  = to_civil( days );
            i64 y2 = t.year + years;
            int d  = t.day;
            if( clamp )
            {
                int last = days_in_month( y2, t.month );
                if( d > last ) d = last;
            }
            return resolve_civil( y2, t.month, d );
        }
    } // namespace ref

    /// Days in 100 Gregorian years, rounded up. The fuzzing tests shift by at
    /// most this much, so both the input and the result have to stay inside the
    /// usable range with this much headroom.
    constexpr i64 MAX_SHIFT_DAYS = 36600;

    /// Largest date the decoders accept without signed overflow. to_civil and
    /// friends compute `days + 719468` in i32, so anything above this overflows
    /// before the calendar arithmetic even starts.
    ///
    /// This is a property of the current implementation, not of the calendar -
    /// widening that shift would remove the limit, but that is out of scope
    /// here, so the tests stay below it.
    constexpr i64 MAX_SAFE_DSE = i64( INT32_MAX ) - 719468;

    /// Usable input range for the fuzzing tests, with room for the shift at
    /// both ends. The low end is bounded by i32 itself rather than by the
    /// implementation.
    constexpr i64 FUZZ_LO = i64( INT32_MIN ) + MAX_SHIFT_DAYS;
    constexpr i64 FUZZ_HI = MAX_SAFE_DSE - MAX_SHIFT_DAYS;
} // namespace


TEST( vtz, ymd_to_string ) {
    COUNT_ASSERTIONS();

    std::uniform_int_distribution<i32> year( 0, 9999 );
    std::uniform_int_distribution<u16> month( 1, 12 );
    std::uniform_int_distribution<u16> day( 1, 31 );
    std::mt19937_64                    rng;
    for( int i = 0; i < 20; i++ )
    {
        auto y = year( rng );
        auto m = month( rng );
        auto d = day( rng );
        ASSERT_EQ( ymd( y, m, d ).str(), fmt::format( "{:0>4}-{:0>2}-{:0>2}", y, m, d ) );
    }

    for( int i = 0; i < 200; i++ )
    {
        auto y = year( rng );
        auto m = month( rng );
        auto d = day( rng );
        ASSERT_EQ_QUIET( ymd( y, m, d ).str(), fmt::format( "{:0>4}-{:0>2}-{:0>2}", y, m, d ) );
    }
}

TEST( vtz, civil ) {
    COUNT_ASSERTIONS();

    static_assert( to_civil( 0 ) == civil_ymd{ 1970, 1, 1 } );
    static_assert( to_civil( -135140 ) == civil_ymd{ 1600, 1, 1 } );
    static_assert( to_civil( -135081 ) == civil_ymd( 1600, 2, 29 ) );
    static_assert( to_civil( -135080 ) == civil_ymd( 1600, 3, 1 ) );
    static_assert( to_civil( 10957 ) == civil_ymd{ 2000, 1, 1 } );
    static_assert( to_civil( 20376 ) == civil_ymd{ 2025, 10, 15 } );
    static_assert( to_civil( 19782 ) == civil_ymd{ 2024, 02, 29 } );

    static_assert( resolve_civil( 1970, 1, 1 ) == 0 );
    static_assert( resolve_civil( 1600, 1, 1 ) == -135140 );
    static_assert( resolve_civil( 1600, 2, 29 ) == -135081 );
    static_assert( resolve_civil( 1600, 3, 1 ) == -135080 );
    static_assert( resolve_civil( 2000, 1, 1 ) == 10957 );
    static_assert( resolve_civil( 2025, 10, 15 ) == 20376 );
    static_assert( resolve_civil( 2024, 2, 29 ) == 19782 );

    ASSERT_EQ( to_civil( 0 ).str(), "1970-01-01" );
    ASSERT_EQ( to_civil( -135140 ).str(), "1600-01-01" );
    ASSERT_EQ( to_civil( 10957 ).str(), "2000-01-01" );
    ASSERT_EQ( to_civil( 20376 ).str(), "2025-10-15" );
    ASSERT_EQ( to_civil( 19782 ).str(), "2024-02-29" );

    ASSERT_EQ( resolve_civil( 1970, 1, 1 ), 0 );
    ASSERT_EQ( resolve_civil( 1600, 1, 1 ), -135140 );
    ASSERT_EQ( resolve_civil( 2000, 1, 1 ), 10957 );
    ASSERT_EQ( resolve_civil( 2025, 10, 15 ), 20376 );
    ASSERT_EQ( resolve_civil( 2024, 2, 29 ), 19782 );

    {
        sys_days_t sysdays = -135140;
        int        y       = 1600;
        u16        m       = 1;

        while( y < 2401 )
        {
            u16 days_in_month = days_in_month_reference( y, m );
            for( u16 d = 1; d <= days_in_month; d++ )
            {
                ASSERT_EQ_QUIET( to_civil( sysdays ), ymd( y, m, d ) );
                ASSERT_EQ_QUIET( resolve_civil( y, m, d ), sysdays );
                sysdays++;
            }
            m += 1;
            if( m == 13 )
            {
                m  = 1;
                y += 1;
            }
        }
    }
}

TEST( vtz, resolve_last_dow ) {
    COUNT_ASSERTIONS();

    // I checked these on a calendar :')
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 3, dow_t::Sun ) ), ymd( 2025, 3, 30 ) );
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 9, dow_t::Sun ) ), ymd( 2025, 9, 28 ) );
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 10, dow_t::Sun ) ), ymd( 2025, 10, 26 ) );
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 3, dow_t::Sat ) ), ymd( 2025, 3, 29 ) );
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 9, dow_t::Sat ) ), ymd( 2025, 9, 27 ) );
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 10, dow_t::Sat ) ), ymd( 2025, 10, 25 ) );

    for( auto dow :
         { dow_t::Sun, dow_t::Mon, dow_t::Tue, dow_t::Wed, dow_t::Thu, dow_t::Fri, dow_t::Sat } )
    {
        for( int y = 1900; y <= 2100; y++ )
        {
            for( int m = 1; m <= 12; m++ )
            {
                auto day = resolve_last_dow( y, m, dow );

                auto last_day_of_month = resolve_civil( y, m, days_in_month_reference( y, m ) );

                ASSERT_LE( day, last_day_of_month );
                ASSERT_LT( last_day_of_month - day, 7 );
                ASSERT_EQ_QUIET( dow_from_days( day ), dow );
                ASSERT_EQ_QUIET( dow_from_days( i64( day ) ), dow );
            }
        }
    }
}


TEST( vtz, resolve_dow_ge ) {
    COUNT_ASSERTIONS();

    ASSERT_EQ( to_civil( resolve_dow_ge( 2025, 9, 30, dow_t::Sun ) ), ymd( 2025, 10, 5 ) );
    ASSERT_EQ( to_civil( resolve_dow_ge( 2025, 9, 30, dow_t::Mon ) ), ymd( 2025, 10, 6 ) );
    ASSERT_EQ( to_civil( resolve_dow_ge( 2025, 9, 30, dow_t::Tue ) ), ymd( 2025, 9, 30 ) );

    for( auto dow :
         { dow_t::Sun, dow_t::Mon, dow_t::Tue, dow_t::Wed, dow_t::Thu, dow_t::Fri, dow_t::Sat } )
    {
        for( int y = 1900; y <= 2100; y++ )
        {
            for( int m = 1; m <= 12; m++ )
            {
                u16 days_in_month = days_in_month_reference( y, m );
                for( u16 d = 1; d <= days_in_month; d++ )
                {
                    // Get what the day should be
                    auto day = resolve_civil( y, m, d );

                    auto day_ge = resolve_dow_ge( y, m, d, dow );
                    ASSERT_LE( day, day_ge );
                    ASSERT_LT( day_ge - day, 7 );
                    ASSERT_EQ_QUIET( dow_from_days( day_ge ), dow );
                }
            }
        }
    }
}

TEST( vtz, resolve_dow_le ) {
    COUNT_ASSERTIONS();

    ASSERT_EQ( to_civil( resolve_dow_le( 2025, 4, 1, dow_t::Sun ) ), ymd( 2025, 3, 30 ) );
    ASSERT_EQ( to_civil( resolve_dow_le( 2025, 4, 1, dow_t::Mon ) ), ymd( 2025, 3, 31 ) );
    ASSERT_EQ( to_civil( resolve_dow_le( 2025, 4, 1, dow_t::Tue ) ), ymd( 2025, 4, 1 ) );

    for( auto dow :
         { dow_t::Sun, dow_t::Mon, dow_t::Tue, dow_t::Wed, dow_t::Thu, dow_t::Fri, dow_t::Sat } )
    {
        for( int y = 1900; y <= 2100; y++ )
        {
            for( int m = 1; m <= 12; m++ )
            {
                u16 days_in_month = days_in_month_reference( y, m );
                for( u16 d = 1; d <= days_in_month; d++ )
                {
                    // Get what the day should be
                    auto day = resolve_civil( y, m, d );

                    auto day_le = resolve_dow_le( y, m, d, dow );
                    ASSERT_LE( day_le, day );
                    ASSERT_LT( day - day_le, 7 );
                    ASSERT_EQ_QUIET( dow_from_days( day_le ), dow );
                }
            }
        }
    }
}


TEST( vtz, resolve_rule ) {
    auto US_DST_Start = rule_entry{
        2007,
        Y_MAX,
        month_t::Mar,
        rule_on::ge( dow_t::Sun, 8 ), // Sun>=8
        rule_at( "2:00" ),
        "1:00",
        "D",
    };
    auto US_DST_End = rule_entry{
        2007,
        Y_MAX,
        month_t::Nov,
        rule_on::ge( dow_t::Sun, 1 ), // Sun>=1
        rule_at( "2:00" ),
        "0",
        "S",
    };
    auto US_Peace_Time = rule_entry{
        1945, 1945, month_t::Aug, rule_on::on( 14 ), rule_at( "23:00u" ), "1:00", "P",
    };

    ASSERT_EQ(
        utc_to_string( US_DST_Start.resolve_at( 2025, from_utc( "-5:00" ), from_utc( "-5:00" ) ) ),
        "2025-03-09 07:00:00Z" );
    // Only WallOff should be used, STDOFF is ignored
    ASSERT_EQ( utc_to_string( US_DST_Start.resolve_at( 2025, from_utc(), from_utc( "-5:00" ) ) ),
               "2025-03-09 07:00:00Z" );
    ASSERT_EQ(
        utc_to_string( US_DST_End.resolve_at( 2025, from_utc( "-5:00" ), from_utc( "-4:00" ) ) ),
        "2025-11-02 06:00:00Z" );
    ASSERT_EQ(
        utc_to_string( US_Peace_Time.resolve_at( 1945, from_utc( "-5:00" ), from_utc( "-4:00" ) ) ),
        "1945-08-14 23:00:00Z" );
}


TEST( vtz, civil_big_test ) {
    /// Corresponds to -400-01-01
    sys_days_t day_counter = -865625;
    unsigned   dow_counter = 6; // -400-01-01 was a Saturday

    // Sanity Check - 2025-11-13 is a Thursday
    ASSERT_EQ_QUIET( dow_from_days( resolve_civil( 2025, 11, 13 ) ), dow_t::Thu );

    // Test these functions over a huge span of time
    for( int year = -400; year < 3000; ++year )
    {
        // Beginning of year, as days since the epoch
        sys_days_t boy_days = day_counter;
        // End of year, as days since epoch
        sys_days_t eoy_days = day_counter + ( is_leap( year ) ? 365 : 364 );

        /// Counts days since the start of the year
        int doy_counter = 0;

        ASSERT_EQ_QUIET( resolve_civil( year ), boy_days );

        for( int month = 1; month <= 12; ++month )
        {
            int        days_in_month = days_in_month_reference( year, month );
            sys_days_t bom_days      = day_counter;
            sys_days_t eom_days      = day_counter + days_in_month - 1;

            for( int day = 1; day <= days_in_month; ++day )
            {
                // Days since the epoch
                auto const dse         = day_counter++;
                int const  doy         = doy_counter++;
                auto const day_of_week = dow_counter++;
                auto const dow         = dow_t( day_of_week );
                if( dow_counter == 7 ) dow_counter = 0;


                ADD_CONTEXT( "Testing date",
                             year,
                             month,
                             day,
                             dse,
                             doy,
                             to_civil( dse ),
                             to_civil_year_doy( dse ) );

                auto ymd  = civil_ymd{ year, u16( month ), u16( day ) };
                auto ymd0 = civil_ymd{ year, u16( month - 1 ), u16( day - 1 ) };

                ASSERT_EQ_QUIET( dow, dow_from_days( dse ) );
                ASSERT_EQ_QUIET( to_civil( dse ), ymd );
                ASSERT_EQ_QUIET( resolve_civil( year, month, day ), dse );
                ASSERT_EQ_QUIET( resolve_civil_ordinal( year, doy + 1 ), dse );
                ASSERT_EQ_QUIET( to_civil0( dse ), ymd0 );
                ASSERT_EQ_QUIET( resolve_civil0( year, month - 1, day - 1 ), dse );

                ASSERT_EQ_QUIET( civil_year( dse ), year );
                ASSERT_EQ_QUIET( civil_month( dse ), month );
                ASSERT_EQ_QUIET( civil_day_of_month( dse ), day );

                ASSERT_EQ_QUIET( civil_month0( dse ), month - 1 );
                ASSERT_EQ_QUIET( civil_day_of_month0( dse ), day - 1 );

                auto year_doy = to_civil_year_doy( dse );
                ASSERT_EQ_QUIET( year_doy.year, year );
                ASSERT_EQ_QUIET( year_doy.doy, doy );

                ASSERT_EQ_QUIET( civil_bom( dse ), bom_days );
                ASSERT_EQ_QUIET( civil_eom( dse ), eom_days );
                ASSERT_EQ_QUIET( civil_boy( dse ), boy_days );
                ASSERT_EQ_QUIET( civil_eoy( dse ), eoy_days );
            }
        }
    }
}


TEST( vtz, civil_arithmetic ) {
    /// Check that adding years or months does the correct thing, for every day
    /// of an 800 year span centred on the epoch, shifted by up to +/-100 years
    /// or months in either direction.
    ///
    /// 1570..2370 is +/-400 years around 1970, so it covers two whole centuries
    /// either side of the epoch and both kinds of century boundary: 1600 and
    /// 2000 are leap years, 1700/1800/1900/2100/2200/2300 are not.

    COUNT_ASSERTIONS();

    /// Corresponds to 1570-01-01
    sys_days_t day_counter = resolve_civil( 1570, 1, 1 );

    // Sanity Check - 2025-11-13 is a Thursday
    ASSERT_EQ_QUIET( dow_from_days( resolve_civil( 2025, 11, 13 ) ), dow_t::Thu );

    for( int year = 1570; year <= 2370; ++year )
    {
        for( int month = 1; month <= 12; ++month )
        {
            int days_in_month = days_in_month_reference( year, month );

            for( int day = 1; day <= days_in_month; ++day )
            {
                auto dse = day_counter++;

                ADD_CONTEXT( "Testing date", year, month, day, dse );

                for( int k = -100; k <= 100; ++k )
                {
                    ASSERT_EQ_QUIET( civil_add_years( dse, k ),
                                     resolve_civil( year + k, month, day ) );

                    auto parts = math::div_floor2<12>( month + k - 1 );
                    ASSERT_EQ_QUIET( civil_add_months( dse, k ),
                                     resolve_civil( year + parts.quot, parts.rem + 1, day ) );
                }
            }
        }
    }

    // The loop should have walked exactly to the end of 2370
    ASSERT_EQ( day_counter, resolve_civil( 2371, 1, 1 ) );
}


TEST( vtz, civil_add_months_clamped ) {
    /// Check that civil_add_months_clamped clamps the day of the month, instead
    /// of rolling over into the following month

    COUNT_ASSERTIONS();

    // Spot checks, so that a failure names a specific date. The dates are
    // printed as strings, since that is much easier to read on failure than
    // days since the epoch.
    auto add_months = []( int y, int m, int d, int k ) {
        return to_civil( civil_add_months_clamped( resolve_civil( y, m, d ), k ) ).str();
    };

    // Jan 31st + 1 month clamps to the end of February
    ASSERT_EQ( add_months( 2025, 1, 31, 1 ), "2025-02-28" );
    // ...and February has 29 days in a leap year
    ASSERT_EQ( add_months( 2024, 1, 31, 1 ), "2024-02-29" );
    // Clamping to a 30 day month
    ASSERT_EQ( add_months( 2025, 5, 31, 1 ), "2025-06-30" );
    // Clamping applies when going backwards, too
    ASSERT_EQ( add_months( 2025, 3, 31, -1 ), "2025-02-28" );
    // Clamping across a year boundary
    ASSERT_EQ( add_months( 2025, 12, 31, 2 ), "2026-02-28" );
    // No clamping needed - the day of the month is valid in the target month
    ASSERT_EQ( add_months( 2025, 12, 13, 3 ), "2026-03-13" );

    /// Corresponds to 1570-01-01. See civil_arithmetic for why this span.
    sys_days_t day_counter = resolve_civil( 1570, 1, 1 );

    for( int year = 1570; year <= 2370; ++year )
    {
        for( int month = 1; month <= 12; ++month )
        {
            int days_in_month = days_in_month_reference( year, month );

            for( int day = 1; day <= days_in_month; ++day )
            {
                auto dse = day_counter++;

                ADD_CONTEXT( "Testing date", year, month, day, dse, to_civil( dse ) );

                for( int k = -100; k <= 100; ++k )
                {
                    ASSERT_EQ_QUIET( civil_add_months_clamped( dse, k ),
                                     add_months_clamped_reference( year, month, day, k ) );
                }
            }
        }
    }

    ASSERT_EQ( day_counter, resolve_civil( 2371, 1, 1 ) );
}


TEST( vtz, civil_add_years_clamped ) {
    /// Check that civil_add_years_clamped clamps Feb 29th back to Feb 28th,
    /// instead of rolling over into March

    COUNT_ASSERTIONS();

    // Check that the clamping works at compile time, too
    static_assert( civil_add_years_clamped( resolve_civil( 2024, 2, 29 ), 1 )
                   == resolve_civil( 2025, 2, 28 ) );
    static_assert( civil_add_years_clamped( resolve_civil( 2024, 2, 29 ), 4 )
                   == resolve_civil( 2028, 2, 29 ) );
    // ...while the unclamped version rolls over into March
    static_assert( civil_add_years( resolve_civil( 2024, 2, 29 ), 1 )
                   == resolve_civil( 2025, 3, 1 ) );

    auto add_years = []( int y, int m, int d, int k ) {
        return to_civil( civil_add_years_clamped( resolve_civil( y, m, d ), k ) ).str();
    };

    // Feb 29th + 1 year clamps to Feb 28th
    ASSERT_EQ( add_years( 2024, 2, 29, 1 ), "2025-02-28" );
    ASSERT_EQ( add_years( 2024, 2, 29, -1 ), "2023-02-28" );
    // 2100 is not a leap year, but 2000 and 2104 are
    ASSERT_EQ( add_years( 2000, 2, 29, 100 ), "2100-02-28" );
    ASSERT_EQ( add_years( 2000, 2, 29, 104 ), "2104-02-29" );
    // Feb 28th is never clamped, and Mar 1st is unaffected
    ASSERT_EQ( add_years( 2024, 2, 28, 1 ), "2025-02-28" );
    ASSERT_EQ( add_years( 2024, 3, 1, 1 ), "2025-03-01" );
    // No clamping needed
    ASSERT_EQ( add_years( 2025, 12, 13, 3 ), "2028-12-13" );

    /// Corresponds to 1570-01-01. See civil_arithmetic for why this span.
    sys_days_t day_counter = resolve_civil( 1570, 1, 1 );

    for( int year = 1570; year <= 2370; ++year )
    {
        for( int month = 1; month <= 12; ++month )
        {
            int days_in_month = days_in_month_reference( year, month );

            for( int day = 1; day <= days_in_month; ++day )
            {
                auto dse = day_counter++;

                ADD_CONTEXT( "Testing date", year, month, day, dse, to_civil( dse ) );

                for( int k = -100; k <= 100; ++k )
                {
                    ASSERT_EQ_QUIET( civil_add_years_clamped( dse, k ),
                                     add_years_clamped_reference( year, month, day, k ) );
                }
            }
        }
    }

    ASSERT_EQ( day_counter, resolve_civil( 2371, 1, 1 ) );
}


TEST( vtz, civil_arithmetic_fuzz ) {
    /// Fuzz all four add functions over the whole usable day range, well
    /// outside the dense spans the tests above cover.
    ///
    /// Checked against reference implementations that work in i64 and call
    /// nothing from civil.h, so they stay valid however the implementation is
    /// rewritten.
    ///
    /// The generator is seeded with a fixed value, so a failure reproduces.

    COUNT_ASSERTIONS();

    std::mt19937_64 rng( 0x5eed15702370ull );

    // Uniform over the usable range - mostly far-flung dates, which is where an
    // era or century boundary bug shows up.
    {
        std::uniform_int_distribution<i64> day_dist( FUZZ_LO, FUZZ_HI );
        std::uniform_int_distribution<int> off_dist( -100, 100 );

        for( int i = 0; i < 1000000; ++i )
        {
            auto dse = sys_days_t( day_dist( rng ) );
            int  k   = off_dist( rng );

            ADD_CONTEXT( "Fuzz (uniform)", i, dse, k );

            ASSERT_EQ_QUIET( i64( civil_add_months( dse, k ) ), ref::add_months( dse, k, false ) );
            ASSERT_EQ_QUIET( i64( civil_add_months_clamped( dse, k ) ),
                             ref::add_months( dse, k, true ) );
            ASSERT_EQ_QUIET( i64( civil_add_years( dse, k ) ), ref::add_years( dse, k, false ) );
            ASSERT_EQ_QUIET( i64( civil_add_years_clamped( dse, k ) ),
                             ref::add_years( dse, k, true ) );
        }
    }

    // Biased towards the inputs most likely to be wrong: month ends (where
    // clamping bites), February, and years sitting on a century or era
    // boundary. The year is drawn from the whole usable range, then snapped to
    // one of those interesting cases.
    {
        i64 year_lo = ref::to_civil( FUZZ_LO ).year + 1;
        i64 year_hi = ref::to_civil( FUZZ_HI ).year - 1;

        std::uniform_int_distribution<i64> year_dist( year_lo, year_hi );
        std::uniform_int_distribution<int> month_dist( 1, 12 );
        std::uniform_int_distribution<int> off_dist( -100, 100 );
        std::uniform_int_distribution<int> pick( 0, 5 );
        // Small nudge, so days adjacent to the interesting ones get hit too
        std::uniform_int_distribution<int> nudge( -2, 2 );

        for( int i = 0; i < 1000000; ++i )
        {
            i64 y = year_dist( rng );
            int m = month_dist( rng );

            // Snap the year onto a boundary that the leap rules care about
            switch( pick( rng ) )
            {
            case 0: y -= ref::fmod( y, 4 ); break;   // leap year
            case 1: y -= ref::fmod( y, 100 ); break; // century, not leap
            case 2: y -= ref::fmod( y, 400 ); break; // century, leap
            case 3: m = 2; break;                    // February
            case 4:
                m  = 2;
                y -= ref::fmod( y, 4 );
                break; // February of a leap year
            default: break;
            }

            // Land on the end of the month, where clamping applies, then nudge
            int last = ref::days_in_month( y, m );
            int d    = last + nudge( rng );
            if( d < 1 ) d = 1;
            if( d > last ) d = last;

            i64 dse64 = ref::resolve_civil( y, m, d );
            if( dse64 < FUZZ_LO || dse64 > FUZZ_HI ) continue;

            auto dse = sys_days_t( dse64 );
            int  k   = off_dist( rng );

            ADD_CONTEXT( "Fuzz (boundary)", i, y, m, d, dse, k );

            ASSERT_EQ_QUIET( i64( civil_add_months( dse, k ) ), ref::add_months( dse, k, false ) );
            ASSERT_EQ_QUIET( i64( civil_add_months_clamped( dse, k ) ),
                             ref::add_months( dse, k, true ) );
            ASSERT_EQ_QUIET( i64( civil_add_years( dse, k ) ), ref::add_years( dse, k, false ) );
            ASSERT_EQ_QUIET( i64( civil_add_years_clamped( dse, k ) ),
                             ref::add_years( dse, k, true ) );
        }
    }
}
