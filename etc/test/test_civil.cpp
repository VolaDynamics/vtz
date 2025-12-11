#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/math.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>

#include "vtz_testing.h"
#include "vtz_debug.h"

#include <random>
using namespace vtz;

using _test_vtz::TEST_LOG;

namespace {
    int _do_set_install = ( set_install( "build/data/tzdata" ), 0 );
} // namespace

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

    constexpr YMD ymd( i32 year, i32 mon, i32 day ) noexcept {
        return { year, u16( mon ), u16( day ) };
    }
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

    static_assert( to_civil( 0 ) == YMD{ 1970, 1, 1 } );
    static_assert( to_civil( -135140 ) == YMD{ 1600, 1, 1 } );
    static_assert( to_civil( -135081 ) == YMD( 1600, 2, 29 ) );
    static_assert( to_civil( -135080 ) == YMD( 1600, 3, 1 ) );
    static_assert( to_civil( 10957 ) == YMD{ 2000, 1, 1 } );
    static_assert( to_civil( 20376 ) == YMD{ 2025, 10, 15 } );
    static_assert( to_civil( 19782 ) == YMD{ 2024, 02, 29 } );

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
        sysdays_t sysdays = -135140;
        int       y       = 1600;
        u16       m       = 1;

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
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 3, DOW::Sun ) ), ymd( 2025, 3, 30 ) );
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 9, DOW::Sun ) ), ymd( 2025, 9, 28 ) );
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 10, DOW::Sun ) ), ymd( 2025, 10, 26 ) );
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 3, DOW::Sat ) ), ymd( 2025, 3, 29 ) );
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 9, DOW::Sat ) ), ymd( 2025, 9, 27 ) );
    ASSERT_EQ( to_civil( resolve_last_dow( 2025, 10, DOW::Sat ) ), ymd( 2025, 10, 25 ) );

    for( auto dow : { DOW::Sun, DOW::Mon, DOW::Tue, DOW::Wed, DOW::Thu, DOW::Fri, DOW::Sat } )
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
            }
        }
    }
}


TEST( vtz, resolve_dow_ge ) {
    COUNT_ASSERTIONS();

    ASSERT_EQ( to_civil( resolve_dow_ge( 2025, 9, 30, DOW::Sun ) ), ymd( 2025, 10, 5 ) );
    ASSERT_EQ( to_civil( resolve_dow_ge( 2025, 9, 30, DOW::Mon ) ), ymd( 2025, 10, 6 ) );
    ASSERT_EQ( to_civil( resolve_dow_ge( 2025, 9, 30, DOW::Tue ) ), ymd( 2025, 9, 30 ) );

    for( auto dow : { DOW::Sun, DOW::Mon, DOW::Tue, DOW::Wed, DOW::Thu, DOW::Fri, DOW::Sat } )
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

    ASSERT_EQ( to_civil( resolve_dow_le( 2025, 4, 1, DOW::Sun ) ), ymd( 2025, 3, 30 ) );
    ASSERT_EQ( to_civil( resolve_dow_le( 2025, 4, 1, DOW::Mon ) ), ymd( 2025, 3, 31 ) );
    ASSERT_EQ( to_civil( resolve_dow_le( 2025, 4, 1, DOW::Tue ) ), ymd( 2025, 4, 1 ) );

    for( auto dow : { DOW::Sun, DOW::Mon, DOW::Tue, DOW::Wed, DOW::Thu, DOW::Fri, DOW::Sat } )
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
    auto US_DST_Start = RuleEntry{
        2007,
        Y_MAX,
        Mon::Mar,
        RuleOn::ge( DOW::Sun, 8 ), // Sun>=8
        RuleAt( "2:00" ),
        "1:00",
        "D",
    };
    auto US_DST_End = RuleEntry{
        2007,
        Y_MAX,
        Mon::Nov,
        RuleOn::ge( DOW::Sun, 1 ), // Sun>=1
        RuleAt( "2:00" ),
        "0",
        "S",
    };
    auto US_Peace_Time = RuleEntry{
        1945,
        1945,
        Mon::Aug,
        RuleOn::on( 14 ),
        RuleAt( "23:00u" ),
        "1:00",
        "P",
    };

    ASSERT_EQ(
        utc_to_string( US_DST_Start.resolve_at( 2025, FromUTC( "-5:00" ), FromUTC( "-5:00" ) ) ),
        "2025-03-09 07:00:00Z" );
    // Only WallOff should be used, STDOFF is ignored
    ASSERT_EQ( utc_to_string( US_DST_Start.resolve_at( 2025, FromUTC(), FromUTC( "-5:00" ) ) ),
        "2025-03-09 07:00:00Z" );
    ASSERT_EQ(
        utc_to_string( US_DST_End.resolve_at( 2025, FromUTC( "-5:00" ), FromUTC( "-4:00" ) ) ),
        "2025-11-02 06:00:00Z" );
    ASSERT_EQ(
        utc_to_string( US_Peace_Time.resolve_at( 1945, FromUTC( "-5:00" ), FromUTC( "-4:00" ) ) ),
        "1945-08-14 23:00:00Z" );
}


TEST( vtz, civil_big_test ) {
    /// Corresponds to -400-01-01
    sysdays_t day_counter = -865625;
    unsigned  dow_counter = 6; // -400-01-01 was a Saturday

    // Sanity Check - 2025-11-13 is a Thursday
    ASSERT_EQ_QUIET( dow_from_days( resolve_civil( 2025, 11, 13 ) ), DOW::Thu );

    // Test these functions over a huge span of time
    for( int year = -400; year < 3000; ++year )
    {
        // Beginning of year, as days since the epoch
        sysdays_t boy_days = day_counter;
        // End of year, as days since epoch
        sysdays_t eoy_days = day_counter + ( is_leap( year ) ? 365 : 364 );

        /// Counts days since the start of the year
        int doy_counter = 0;

        ASSERT_EQ_QUIET( resolve_civil( year ), boy_days );

        for( int month = 1; month <= 12; ++month )
        {
            int       days_in_month = days_in_month_reference( year, month );
            sysdays_t bom_days      = day_counter;
            sysdays_t eom_days      = day_counter + days_in_month - 1;

            for( int day = 1; day <= days_in_month; ++day )
            {
                // Days since the epoch
                auto const dse         = day_counter++;
                int const  doy         = doy_counter++;
                auto const day_of_week = dow_counter++;
                auto const dow         = DOW( day_of_week );
                if( dow_counter == 7 ) dow_counter = 0;


                ADD_CONTEXT( "Testing date",
                    year,
                    month,
                    day,
                    dse,
                    doy,
                    to_civil( dse ),
                    to_civil_year_doy( dse ) );

                auto ymd  = YMD{ year, u16( month ), u16( day ) };
                auto ymd0 = YMD{ year, u16( month - 1 ), u16( day - 1 ) };

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
    /// Check that adding years or months does the correct thing


    /// Corresponds to 1970-01-01
    sysdays_t day_counter = 0;

    // Sanity Check - 2025-11-13 is a Thursday
    ASSERT_EQ_QUIET( dow_from_days( resolve_civil( 2025, 11, 13 ) ), DOW::Thu );

    // Test these functions over a huge span of time
    for( int year = 1970; year < 2060; ++year )
    {
        for( int month = 1; month <= 12; ++month )
        {
            int days_in_month = days_in_month_reference( year, month );

            for( int day = 1; day <= days_in_month; ++day )
            {
                auto dse = day_counter++;
                for( int k = -60; k <= 60; ++k )
                {
                    ASSERT_EQ_QUIET(
                        civil_add_years( dse, k ), resolve_civil( year + k, month, day ) );

                    auto parts = math::div_floor2<12>( month + k - 1 );
                    ASSERT_EQ_QUIET( civil_add_months( dse, k ),
                        resolve_civil( year + parts.quot, parts.rem + 1, day ) );
                }
            }
        }
    }
}
