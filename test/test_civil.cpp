#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/math.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>

#include "vtz_testing.h"
#include <random>
using namespace vtz;

using _test_vtz::TEST_LOG;

namespace {
    int _do_set_install = ( set_install( "build/data/tzdata" ), 0 );
} // namespace

STRUCT_INFO( vtz::math::div_t<int>,
    FIELD( vtz::math::div_t<int>, quot ),
    FIELD( vtz::math::div_t<int>, rem ) );

STRUCT_INFO( vtz::YMD, FIELD( vtz::YMD, year ), FIELD( vtz::YMD, month ), FIELD( vtz::YMD, day ) );
STRUCT_INFO( vtz::year_doy, FIELD( vtz::year_doy, year ), FIELD( vtz::year_doy, doy ) );

TEST( vtz_math, divFloor ) {
    using vtz::math::div_t;
    using vtz::math::divFloor;
    using vtz::math::divFloor2;

    static_assert( divFloor2<5>( -5 ) == div_t{ -1, 0 } );
    static_assert( divFloor2<5>( -4 ) == div_t{ -1, 1 } );
    static_assert( divFloor2<5>( -3 ) == div_t{ -1, 2 } );
    static_assert( divFloor2<5>( -2 ) == div_t{ -1, 3 } );
    static_assert( divFloor2<5>( -1 ) == div_t{ -1, 4 } );
    static_assert( divFloor2<5>( 0 ) == div_t{ 0, 0 } );
    static_assert( divFloor2<5>( 1 ) == div_t{ 0, 1 } );
    static_assert( divFloor2<5>( 2 ) == div_t{ 0, 2 } );
    static_assert( divFloor2<5>( 3 ) == div_t{ 0, 3 } );
    static_assert( divFloor2<5>( 4 ) == div_t{ 0, 4 } );
    static_assert( divFloor2<5>( 5 ) == div_t{ 1, 0 } );

    ASSERT_EQ( divFloor2<5>( -5 ), ( div_t{ -1, 0 } ) );
    ASSERT_EQ( divFloor2<5>( -4 ), ( div_t{ -1, 1 } ) );
    ASSERT_EQ( divFloor2<5>( -3 ), ( div_t{ -1, 2 } ) );
    ASSERT_EQ( divFloor2<5>( -2 ), ( div_t{ -1, 3 } ) );
    ASSERT_EQ( divFloor2<5>( -1 ), ( div_t{ -1, 4 } ) );
    ASSERT_EQ( divFloor2<5>( 0 ), ( div_t{ 0, 0 } ) );
    ASSERT_EQ( divFloor2<5>( 1 ), ( div_t{ 0, 1 } ) );
    ASSERT_EQ( divFloor2<5>( 2 ), ( div_t{ 0, 2 } ) );
    ASSERT_EQ( divFloor2<5>( 3 ), ( div_t{ 0, 3 } ) );
    ASSERT_EQ( divFloor2<5>( 4 ), ( div_t{ 0, 4 } ) );
    ASSERT_EQ( divFloor2<5>( 5 ), ( div_t{ 1, 0 } ) );
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

    u8 daysInMonthReference( int year, u8 month ) {
        if( month == 2 )
        {
            bool isLeap = year % 4 == 0 && ( year % 400 == 0 || year % 100 != 0 );
            return isLeap ? 29 : 28;
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

    static_assert( toCivil( 0 ) == YMD{ 1970, 1, 1 } );
    static_assert( toCivil( -135140 ) == YMD{ 1600, 1, 1 } );
    static_assert( toCivil( -135081 ) == YMD( 1600, 2, 29 ) );
    static_assert( toCivil( -135080 ) == YMD( 1600, 3, 1 ) );
    static_assert( toCivil( 10957 ) == YMD{ 2000, 1, 1 } );
    static_assert( toCivil( 20376 ) == YMD{ 2025, 10, 15 } );
    static_assert( toCivil( 19782 ) == YMD{ 2024, 02, 29 } );

    static_assert( resolveCivil( 1970, 1, 1 ) == 0 );
    static_assert( resolveCivil( 1600, 1, 1 ) == -135140 );
    static_assert( resolveCivil( 1600, 2, 29 ) == -135081 );
    static_assert( resolveCivil( 1600, 3, 1 ) == -135080 );
    static_assert( resolveCivil( 2000, 1, 1 ) == 10957 );
    static_assert( resolveCivil( 2025, 10, 15 ) == 20376 );
    static_assert( resolveCivil( 2024, 2, 29 ) == 19782 );

    ASSERT_EQ( toCivil( 0 ).str(), "1970-01-01" );
    ASSERT_EQ( toCivil( -135140 ).str(), "1600-01-01" );
    ASSERT_EQ( toCivil( 10957 ).str(), "2000-01-01" );
    ASSERT_EQ( toCivil( 20376 ).str(), "2025-10-15" );
    ASSERT_EQ( toCivil( 19782 ).str(), "2024-02-29" );

    ASSERT_EQ( resolveCivil( 1970, 1, 1 ), 0 );
    ASSERT_EQ( resolveCivil( 1600, 1, 1 ), -135140 );
    ASSERT_EQ( resolveCivil( 2000, 1, 1 ), 10957 );
    ASSERT_EQ( resolveCivil( 2025, 10, 15 ), 20376 );
    ASSERT_EQ( resolveCivil( 2024, 2, 29 ), 19782 );

    {
        sysdays_t sysdays = -135140;
        int       y       = 1600;
        u16       m       = 1;

        while( y < 2401 )
        {
            u16 daysInMonth = daysInMonthReference( y, m );
            for( u16 d = 1; d <= daysInMonth; d++ )
            {
                ASSERT_EQ_QUIET( toCivil( sysdays ), ymd( y, m, d ) );
                ASSERT_EQ_QUIET( resolveCivil( y, m, d ), sysdays );
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

TEST( vtz, resolveLastDOW ) {
    COUNT_ASSERTIONS();

    // I checked these on a calendar :')
    ASSERT_EQ( toCivil( resolveLastDOW( 2025, 3, DOW::Sun ) ), ymd( 2025, 3, 30 ) );
    ASSERT_EQ( toCivil( resolveLastDOW( 2025, 9, DOW::Sun ) ), ymd( 2025, 9, 28 ) );
    ASSERT_EQ( toCivil( resolveLastDOW( 2025, 10, DOW::Sun ) ), ymd( 2025, 10, 26 ) );
    ASSERT_EQ( toCivil( resolveLastDOW( 2025, 3, DOW::Sat ) ), ymd( 2025, 3, 29 ) );
    ASSERT_EQ( toCivil( resolveLastDOW( 2025, 9, DOW::Sat ) ), ymd( 2025, 9, 27 ) );
    ASSERT_EQ( toCivil( resolveLastDOW( 2025, 10, DOW::Sat ) ), ymd( 2025, 10, 25 ) );

    for( auto dow : { DOW::Sun, DOW::Mon, DOW::Tue, DOW::Wed, DOW::Thu, DOW::Fri, DOW::Sat } )
    {
        for( int y = 1900; y <= 2100; y++ )
        {
            for( int m = 1; m <= 12; m++ )
            {
                auto day = resolveLastDOW( y, m, dow );

                auto lastDayOfMonth = resolveCivil( y, m, daysInMonthReference( y, m ) );

                ASSERT_LE( day, lastDayOfMonth );
                ASSERT_LT( lastDayOfMonth - day, 7 );
                ASSERT_EQ_QUIET( dowFromDays( day ), dow );
            }
        }
    }
}


TEST( vtz, resolveDOW_GE ) {
    COUNT_ASSERTIONS();

    ASSERT_EQ( toCivil( resolveDOW_GE( 2025, 9, 30, DOW::Sun ) ), ymd( 2025, 10, 5 ) );
    ASSERT_EQ( toCivil( resolveDOW_GE( 2025, 9, 30, DOW::Mon ) ), ymd( 2025, 10, 6 ) );
    ASSERT_EQ( toCivil( resolveDOW_GE( 2025, 9, 30, DOW::Tue ) ), ymd( 2025, 9, 30 ) );

    for( auto dow : { DOW::Sun, DOW::Mon, DOW::Tue, DOW::Wed, DOW::Thu, DOW::Fri, DOW::Sat } )
    {
        for( int y = 1900; y <= 2100; y++ )
        {
            for( int m = 1; m <= 12; m++ )
            {
                u16 daysInMonth = daysInMonthReference( y, m );
                for( u16 d = 1; d <= daysInMonth; d++ )
                {
                    // Get what the day should be
                    auto day = resolveCivil( y, m, d );

                    auto dayGE = resolveDOW_GE( y, m, d, dow );
                    ASSERT_LE( day, dayGE );
                    ASSERT_LT( dayGE - day, 7 );
                    ASSERT_EQ_QUIET( dowFromDays( dayGE ), dow );
                }
            }
        }
    }
}

TEST( vtz, resolveDOW_LE ) {
    COUNT_ASSERTIONS();

    ASSERT_EQ( toCivil( resolveDOW_LE( 2025, 4, 1, DOW::Sun ) ), ymd( 2025, 3, 30 ) );
    ASSERT_EQ( toCivil( resolveDOW_LE( 2025, 4, 1, DOW::Mon ) ), ymd( 2025, 3, 31 ) );
    ASSERT_EQ( toCivil( resolveDOW_LE( 2025, 4, 1, DOW::Tue ) ), ymd( 2025, 4, 1 ) );

    for( auto dow : { DOW::Sun, DOW::Mon, DOW::Tue, DOW::Wed, DOW::Thu, DOW::Fri, DOW::Sat } )
    {
        for( int y = 1900; y <= 2100; y++ )
        {
            for( int m = 1; m <= 12; m++ )
            {
                u16 daysInMonth = daysInMonthReference( y, m );
                for( u16 d = 1; d <= daysInMonth; d++ )
                {
                    // Get what the day should be
                    auto day = resolveCivil( y, m, d );

                    auto dayLE = resolveDOW_LE( y, m, d, dow );
                    ASSERT_LE( dayLE, day );
                    ASSERT_LT( day - dayLE, 7 );
                    ASSERT_EQ_QUIET( dowFromDays( dayLE ), dow );
                }
            }
        }
    }
}


TEST( vtz, resolveRule ) {
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
        utcToString( US_DST_Start.resolveAt( 2025, FromUTC( "-5:00" ), FromUTC( "-5:00" ) ) ),
        "2025-03-09 07:00:00Z" );
    // Only WallOff should be used, STDOFF is ignored
    ASSERT_EQ( utcToString( US_DST_Start.resolveAt( 2025, FromUTC(), FromUTC( "-5:00" ) ) ),
        "2025-03-09 07:00:00Z" );
    ASSERT_EQ( utcToString( US_DST_End.resolveAt( 2025, FromUTC( "-5:00" ), FromUTC( "-4:00" ) ) ),
        "2025-11-02 06:00:00Z" );
    ASSERT_EQ(
        utcToString( US_Peace_Time.resolveAt( 1945, FromUTC( "-5:00" ), FromUTC( "-4:00" ) ) ),
        "1945-08-14 23:00:00Z" );
}


TEST( vtz, civil_big_test ) {
    /// Corresponds to -400-01-01
    sysdays_t dayCounter = -865625;

    for( int year = -400; year < 3000; ++year )
    {
        // Beginning of year, as days since the epoch
        sysdays_t boyDays = dayCounter;
        // End of year, as days since epoch
        sysdays_t eoyDays = dayCounter + ( isLeap( year ) ? 365 : 364 );

        /// Counts days since the start of the year
        int doyCounter = 0;

        for( int month = 1; month <= 12; ++month )
        {
            int       daysInMonth = daysInMonthReference( year, month );
            sysdays_t bomDays     = dayCounter;
            sysdays_t eomDays     = dayCounter + daysInMonth - 1;

            for( int day = 1; day <= daysInMonth; ++day )
            {
                // Days since the epoch
                auto const dse = dayCounter++;
                int const  doy = doyCounter++;
                ADD_CONTEXT( "Testing date",
                    year,
                    month,
                    day,
                    dse,
                    doy,
                    toCivil( dse ),
                    toCivilYearDOY( dse ) );

                auto ymd  = YMD{ year, u16( month ), u16( day ) };
                auto ymd0 = YMD{ year, u16( month - 1 ), u16( day - 1 ) };

                ASSERT_EQ_QUIET( toCivil( dse ), ymd );
                ASSERT_EQ_QUIET( resolveCivil( year, month, day ), dse );
                ASSERT_EQ_QUIET( toCivil0( dse ), ymd0 );
                ASSERT_EQ_QUIET( resolveCivil0( year, month - 1, day - 1 ), dse );

                ASSERT_EQ_QUIET( civilYear( dse ), year );
                ASSERT_EQ_QUIET( civilMonth( dse ), month );
                ASSERT_EQ_QUIET( civilDayOfMonth( dse ), day );

                ASSERT_EQ_QUIET( civilMonth0( dse ), month - 1 );
                ASSERT_EQ_QUIET( civilDayOfMonth0( dse ), day - 1 );

                auto yearDoy = toCivilYearDOY( dse );
                ASSERT_EQ_QUIET( yearDoy.year, year );
                ASSERT_EQ_QUIET( yearDoy.doy, doy );

                ASSERT_EQ_QUIET( civilBOM( dse ), bomDays );
                ASSERT_EQ_QUIET( civilEOM( dse ), eomDays );
                ASSERT_EQ_QUIET( civilBOY( dse ), boyDays );
                ASSERT_EQ_QUIET( civilEOY( dse ), eoyDays );
            }
        }
    }
}
