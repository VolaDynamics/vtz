#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/impl/math.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>

#include <vtz/libfmt_compat.h>

#include "vtz_debug.h"
#include "vtz_testing.h"

#include <algorithm>
#include <atomic>
#include <random>
#include <thread>
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

    /// True if a reference result fits in sys_days_t. Used by the range-end test,
    /// which reaches inputs where the true answer can fall off either end.
    constexpr bool representable( i64 dse ) noexcept {
        return dse >= INT32_MIN && dse <= INT32_MAX;
    }
} // namespace


/// Compile-time guards on the magic constants that the civil_add_{months,years}
/// century split relies on. These live here rather than in civil.h so that the
/// checking machinery is not compiled into every consumer of that public
/// header - but they are still static_asserts, so a bad constant fails the build
/// rather than a test run.
///
/// They are load-bearing: they have caught real bugs, and the month magic has
/// only 63 days of margin (first divergence at 428, against a maximum doy of
/// 365). Anything that widens the doy range must re-derive the constants.
namespace {
    using namespace vtz::_civ;
    /// Largest day-of-year split_century can produce. A March-based year
    /// runs Mar 1st .. Feb 28th/29th, so doy only reaches 365 on a Feb 29th.
    constexpr u32 MAX_DOY = 365u;

    /// Highest century index reachable from an i32 day count. Derived rather
    /// than written out, so it follows split_century instead of having to be
    /// re-derived by hand whenever the shift or its width changes.
    constexpr u32 MAX_CENTURY = century_of( MAX_SHIFTED_DAYS );

    /// First doy at which the month magic stops agreeing with the exact
    /// expressions. Asserted below, so the margin over MAX_DOY is a checked
    /// fact rather than a claim in a comment.
    constexpr u32 MONTH_MAGIC_FIRST_BAD_DOY = 428u;

    /// Checks the magic-multiply century index at century `m`: it must read `m`
    /// on that century's first day and `m - 1` on the day before.
    ///
    /// Only the step points need checking - both the exact quotient and the
    /// magic form are non-decreasing, so agreeing wherever the exact value steps
    /// forces agreement in between.
    constexpr bool century_magic_ok_at( u32 m ) noexcept {
        u64 start = century_start( m );
        if( century_of( start ) != m ) return false;
        return m == 0 || century_of( start - 1 ) == m - 1;
    }

    /// Checks the century magic over the whole reachable range.
    ///
    /// Eight points are exhaustive, not a sample: writing `m = 4 * q + r` makes
    /// `century_start` exactly `146097 * q + 36524 * r`, so for fixed `r`
    /// the floored quantity is *affine* in `q`, and an affine function stays
    /// within a half-open interval iff it does so at both ends. So the first and
    /// last `m` of each residue class settle it. (Brute force over every
    /// reachable century agrees, and this rejects CENTURY_MAGIC +- 1 - which
    /// fail at centuries 4 and 23135 - so it is not vacuous. The brute-force
    /// version is not used here because it costs 0.43s per TU and exceeds
    /// clang's constexpr step limit.)
    constexpr bool century_magic_ok() noexcept {
        for( u32 r = 0; r < 4; ++r )
        {
            if( !century_magic_ok_at( r ) ) return false;
            if( !century_magic_ok_at( MAX_CENTURY - ( ( MAX_CENTURY - r ) % 4 ) ) ) return false;
        }
        return true;
    }

    static_assert( century_magic_ok(),
                   "CENTURY_MAGIC must give the exact century index for every "
                   "reachable day count" );

    // MAX_CENTURY really is the last century an i32 day count reaches: it has
    // begun by the largest shifted count, and the next one has not.
    static_assert( century_start( MAX_CENTURY ) <= MAX_SHIFTED_DAYS,
                   "MAX_CENTURY must have begun by the largest shifted day count" );
    static_assert( century_start( MAX_CENTURY + 1 ) > MAX_SHIFTED_DAYS,
                   "MAX_CENTURY must be the *last* reachable century" );

    // The largest shifted day count does not fit in a u32, which is why
    // split_century forms it in 64 bits. This assertion's *failure* would be the
    // good news: it would mean the origin could be narrowed back down.
    static_assert( MAX_SHIFTED_DAYS > 4294967295ull,
                   "if the shifted day count fits in u32, split_century can narrow" );

    /// Checks the packed month-length table against the closed form it replaces:
    /// for every March-based month, `30 - <2-bit field>` must be the 0-based
    /// last day, ie the gap to the next month's start (and for February, to the
    /// non-leap year end at day 365).
    constexpr bool march_month_last_bits_ok() noexcept {
        for( u32 mp = 0; mp < 12; ++mp )
        {
            u32 end  = mp == 11 ? 365u : month_start( mp + 1 );
            u32 last = end - month_start( mp ) - 1;
            if( 30u - ( ( MARCH_MONTH_LAST_BITS >> ( 2 * mp ) ) & 3u ) != last ) return false;
        }
        return true;
    }

    static_assert( march_month_last_bits_ok(),
                   "MARCH_MONTH_LAST_BITS must encode the 0-based last day of "
                   "every month of a March-based year" );

    /// Checks the two identities the months path relies on, for every doy in
    /// [0, MAX_DOY]:
    ///
    ///   ( 535 * doy + 331 ) >> 14             == ( 5 * doy + 2 ) / 153
    ///   ( ( 535 * doy + 331 ) & 16383 ) / 535 == doy - ( 153 * mp + 2 ) / 5
    ///
    /// ie one multiply yields both the month and the 0-based day within it. The
    /// second identity holds because the low bits of the product are exactly
    /// `535 * dom`.
    constexpr bool month_magic_ok() noexcept {
        for( u32 doy = 0; doy <= MAX_DOY; ++doy )
        {
            u32 t  = MONTH_MAGIC_MUL * doy + MONTH_MAGIC_ADD;
            u32 mp = t >> MONTH_MAGIC_SHIFT;
            if( mp != ( 5 * doy + 2 ) / 153 ) return false;
            u32 dom = ( t & MONTH_MAGIC_MASK ) / MONTH_MAGIC_MUL;
            if( dom != doy - month_start( mp ) ) return false;
        }
        return true;
    }

    static_assert( month_magic_ok(),
                   "MONTH_MAGIC_* must reproduce ( 5 * doy + 2 ) / 153 and the "
                   "day within the month exactly, for every doy in [0, MAX_DOY]" );

    // The margin past MAX_DOY is thin - 63 days - so pin the first divergence
    // down too. If a future change grows the doy range this fires and points at
    // the real constraint instead of silently going wrong.
    static_assert( MONTH_MAGIC_FIRST_BAD_DOY > MAX_DOY,
                   "the month magic must be exact over the whole doy range" );
    static_assert( ( ( MONTH_MAGIC_MUL * MONTH_MAGIC_FIRST_BAD_DOY + MONTH_MAGIC_ADD )
                     >> MONTH_MAGIC_SHIFT )
                       != ( 5 * MONTH_MAGIC_FIRST_BAD_DOY + 2 ) / 153,
                   "MONTH_MAGIC_FIRST_BAD_DOY must be where the identity actually first "
                   "fails - if this fires, re-derive the true bound" );

    /// Checks that split_century never produces a doy above MAX_DOY,
    /// which is what the month-extraction constants depend on.
    ///
    /// One 4-year cycle covers every case: doy is `( ( 4 * doc + 3 ) % 1461 )
    /// >> 2`, which depends on doc only through `doc % 1461`, and a century is a
    /// whole number of such cycles.
    constexpr bool split_century_bound_ok() noexcept {
        u32 worst = 0;
        for( u32 doc = 0; doc < 1461u; ++doc )
        {
            u32 t   = 4 * doc + 3;
            u32 doy = ( t - 1461u * ( t / 1461u ) ) >> 2;
            if( doy > worst ) worst = doy;
        }
        // Exactly equal: the bound must be tight, not merely an upper limit, so
        // that a decode change lowering the real maximum shows up here rather
        // than leaving a stale constant in place.
        return worst == MAX_DOY;
    }

    static_assert( split_century_bound_ok(),
                   "split_century must produce doy in [0, MAX_DOY] - the "
                   "month-extraction constants are only exact over that range" );

    /// Checks the collapsed leap test against the real three-clause rule.
    ///
    /// Four centuries are exhaustive, because both sides depend on `c` only
    /// through `c % 4`. The calendar year of March-based year (c, z) is
    /// `100 * c + z` up to the multiple of 400 that CENTURY_ORIGIN contributes,
    /// and that multiple is invisible to a leap test - so any four consecutive
    /// centuries of a year divisible by 400 will do.
    constexpr bool march_year_is_leap_ok() noexcept {
        for( u32 c = 0; c < 4; ++c )
        {
            for( u32 z = 0; z < 100; ++z )
            {
                // The Feb that ends March-based year (c, z) falls in the
                // following calendar year, hence the + 1.
                i32 calendar_year = i32( 100 * c + z ) + 1;
                if( march_year_is_leap( c, z ) != vtz::is_leap( calendar_year ) ) return false;
            }
        }
        return true;
    }

    static_assert( march_year_is_leap_ok(),
                   "march_year_is_leap must agree with the three-clause is_leap "
                   "for every (c % 4, z)" );
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

    // Compile-time spot checks. Expected values are written as dates and run
    // through resolve_civil, which is the era-split encoder and so an
    // independent oracle rather than a restatement of the code under test.
    static_assert( civil_add_months( resolve_civil( 2025, 10, 15 ), 3 )
                   == resolve_civil( 2026, 1, 15 ) );
    static_assert( civil_add_months( resolve_civil( 2025, 10, 15 ), -3 )
                   == resolve_civil( 2025, 7, 15 ) );
    // The day of the month is kept, so a short target month rolls over
    static_assert( civil_add_months( resolve_civil( 2025, 1, 31 ), 1 )
                   == resolve_civil( 2025, 3, 3 ) );
    // ...and rolls one day less far when that February has 29 days
    static_assert( civil_add_months( resolve_civil( 2024, 1, 31 ), 1 )
                   == resolve_civil( 2024, 3, 2 ) );
    static_assert( civil_add_years( resolve_civil( 2025, 12, 13 ), 3 )
                   == resolve_civil( 2028, 12, 13 ) );
    // Crossing a century that is not a leap year, and one that is
    static_assert( civil_add_years( resolve_civil( 2050, 6, 1 ), 100 )
                   == resolve_civil( 2150, 6, 1 ) );
    static_assert( civil_add_years( resolve_civil( 1950, 6, 1 ), 100 )
                   == resolve_civil( 2050, 6, 1 ) );

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

    // Check that the clamping works at compile time, too
    static_assert( civil_add_months_clamped( resolve_civil( 2025, 1, 31 ), 1 )
                   == resolve_civil( 2025, 2, 28 ) );
    static_assert( civil_add_months_clamped( resolve_civil( 2024, 1, 31 ), 1 )
                   == resolve_civil( 2024, 2, 29 ) );
    // Clamping to a 30 day month, and backwards
    static_assert( civil_add_months_clamped( resolve_civil( 2025, 5, 31 ), 1 )
                   == resolve_civil( 2025, 6, 30 ) );
    static_assert( civil_add_months_clamped( resolve_civil( 2025, 3, 31 ), -1 )
                   == resolve_civil( 2025, 2, 28 ) );
    // ...while the unclamped version rolls over into the following month
    static_assert( civil_add_months( resolve_civil( 2025, 1, 31 ), 1 )
                   == resolve_civil( 2025, 3, 3 ) );

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
    // 2100 is not a leap year, so Feb 29th clamps; 2104 is, so it does not
    static_assert( civil_add_years_clamped( resolve_civil( 2000, 2, 29 ), 100 )
                   == resolve_civil( 2100, 2, 28 ) );
    static_assert( civil_add_years_clamped( resolve_civil( 2000, 2, 29 ), 104 )
                   == resolve_civil( 2104, 2, 29 ) );

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


TEST( vtz, civil_arithmetic_range_ends ) {
    /// The civil_add_* family accepts the whole sys_days_t range, including the
    /// top of it.
    ///
    /// split_century forms the shifted day count in 64 bits (MAX_SHIFTED_DAYS is
    /// past 2^32, and no era-aligned origin makes it fit), so nothing wraps at
    /// the top of i32; and the delta arithmetic is unsigned, so nothing
    /// overflows. Before that, the top 131235 days wrapped: the shifted count
    /// reduced mod 2^32, the decode was self-consistent but described a date at
    /// a different point in the era, and roughly half of those inputs came back
    /// one to four days out.
    ///
    /// Kept out of civil_arithmetic_fuzz because that test's bounds also encode
    /// the *decoders'* narrower range - to_civil still adds 719468 in i32 - and
    /// this test deliberately goes past it. So nothing here may call to_civil,
    /// resolve_civil or their friends; expectations come from ref::, which works
    /// in i64 throughout.

    COUNT_ASSERTIONS();

    /// First day count that a u32 shifted count could not hold. Derived from the
    /// origin rather than written out, so it follows CENTURY_ORIGIN.
    constexpr i64 FIRST_PAST_U32 = i64( UINT32_MAX ) - vtz::_civ::CENTURY_ORIGIN + 1;

    // This exact call used to return 1 day short. Pinned at compile time,
    // against the i64 reference rather than a transcribed day count.
    static_assert( civil_add_months( sys_days_t( FIRST_PAST_U32 ), -1 )
                   == ref::add_months( FIRST_PAST_U32, -1, false ) );
    static_assert( civil_add_years( sys_days_t( INT32_MIN ), 1 )
                   == ref::add_years( INT32_MIN, 1, false ) );

    // Every day of the window that used to wrap, plus both extremes of the
    // range more densely. Only pairs whose true answer is representable are
    // checked - past that the caller is out of range whatever we do.
    struct Span {
        char const* what;
        i64         lo, hi;
        int         max_shift;
    };
    constexpr Span spans[]{
        { "window that used to wrap", FIRST_PAST_U32, i64( INT32_MAX ), 12 },
        { "bottom of the range", i64( INT32_MIN ), i64( INT32_MIN ) + 999, 100 },
        { "top of the range", i64( INT32_MAX ) - 999, i64( INT32_MAX ), 100 },
    };

    for( auto const& span : spans )
    {
        for( i64 dse64 = span.lo; dse64 <= span.hi; ++dse64 )
        {
            auto dse = sys_days_t( dse64 );

            for( int k = -span.max_shift; k <= span.max_shift; ++k )
            {
                ADD_CONTEXT( "Range end", span.what, dse64, k );

                // Braces are load-bearing: ASSERT_EQ_QUIET expands to an
                // if/else, so an unbraced body here is a dangling else.
                i64 want = ref::add_months( dse64, k, false );
                if( representable( want ) )
                { ASSERT_EQ_QUIET( i64( civil_add_months( dse, k ) ), want ); }

                want = ref::add_months( dse64, k, true );
                if( representable( want ) )
                { ASSERT_EQ_QUIET( i64( civil_add_months_clamped( dse, k ) ), want ); }

                want = ref::add_years( dse64, k, false );
                if( representable( want ) )
                { ASSERT_EQ_QUIET( i64( civil_add_years( dse, k ) ), want ); }

                want = ref::add_years( dse64, k, true );
                if( representable( want ) )
                { ASSERT_EQ_QUIET( i64( civil_add_years_clamped( dse, k ) ), want ); }
            }
        }
    }
}


TEST( vtz, civil_reduced_space ) {
    /// Exhaustive over the *reduced* state space, which is exhaustive over the
    /// arithmetic. This is the check that the proof in
    /// docs/civil_add_algorithms.md section 11 rests on.
    ///
    /// Two exact symmetries make a 2^64 input space finite:
    ///
    ///   A. `delta` depends on `days` only through `(c % 4, doc)`. The code
    ///      mentions `c` only as `c & 3` and as `(c + cq) & 3` inside
    ///      march_year_is_leap, and z/doy/mp/dom are all functions of `doc`. So
    ///      the day axis has 3 * 36524 + 36525 == 146097 classes - one per day
    ///      of an era, which is Hinnant's "you only need to debug a single era"
    ///      surviving the century split.
    ///
    ///   B. Shifting by one era adds exactly one era of days. k + 4800 months
    ///      (== 400 years) sends yq -> yq + 400 and cq -> cq + 4 while leaving
    ///      mp2 and z2 alone, so delta gains 365 * 400 + 24 * 4 + 1 == 146097.
    ///      One residue of k modulo 4800 therefore settles every k.
    ///
    /// So enumerating 146097 * 4800 classes for the month functions and
    /// 146097 * 400 for the years covers the algebra completely. Both symmetries
    /// are checked here too, so the reduction is verified rather than assumed.
    ///
    /// What this deliberately does *not* cover is the machine arithmetic at the
    /// range ends: it runs mid-range so that nothing overflows, and the
    /// comparison is purely about the calendar arithmetic. Overflow behaviour is
    /// civil_arithmetic_range_ends' job.
    ///
    /// Threaded, because it is ~1.5e9 comparisons against the i64 reference.
    /// gtest assertions are not usable off the main thread, so the workers
    /// accumulate counts and the first failure, and the assertions happen below.

    COUNT_ASSERTIONS();

    using namespace vtz::_civ;

    /// Four consecutive centuries covering every value of `c % 4`. Chosen
    /// mid-range so that shifting by up to 400 years cannot leave sys_days_t -
    /// this test is about the arithmetic, not the range ends.
    constexpr u32 CBASE = 58800; // CBASE % 4 == 0
    static_assert( CBASE % 4 == 0, "CBASE must start on a leap century" );

    auto rep_days = []( u32 cr, u32 doc ) -> i64 {
        return i64( century_start( CBASE + cr ) ) + doc - i64( CENTURY_ORIGIN );
    };
    auto century_len = []( u32 cr ) -> u32 {
        return u32( century_start( CBASE + cr + 1 ) - century_start( CBASE + cr ) );
    };

    /// One era, expressed the two ways the symmetries need it
    constexpr i32 MONTHS_PER_ERA = 4800;
    constexpr i32 YEARS_PER_ERA  = 400;
    constexpr i64 DAYS_PER_ERA   = 146097;

    static_assert( MONTHS_PER_ERA == YEARS_PER_ERA * 12 );

    struct Failure {
        bool        seen = false;
        char const* who  = "";
        u32         cr = 0, doc = 0;
        i32         k   = 0;
        i64         got = 0, want = 0;
    };
    struct Result {
        i64     checked = 0, failed = 0;
        Failure first;
    };

    auto record = []( Result& r, char const* who, u32 cr, u32 doc, i32 k, i64 got, i64 want ) {
        ++r.checked;
        if( got == want ) return;
        ++r.failed;
        if( !r.first.seen ) r.first = Failure{ true, who, cr, doc, k, got, want };
    };

    // Capped: ctest may already be running other tests in parallel.
    unsigned hw = std::thread::hardware_concurrency();
    unsigned nt = std::min( hw ? hw : 4u, 8u );

    std::vector<Result> results( size_t( nt ), Result{} );
    {
        std::vector<std::thread> workers;
        for( unsigned t = 0; t < nt; ++t )
            workers.emplace_back( [&, t] {
                Result& r = results[t];
                for( u32 cr = 0; cr < 4; ++cr )
                {
                    u32 clen = century_len( cr );
                    for( u32 doc = t; doc < clen; doc += nt )
                    {
                        i64 d64 = rep_days( cr, doc );
                        auto d  = sys_days_t( d64 );

                        for( i32 k = 0; k < MONTHS_PER_ERA; ++k )
                        {
                            record( r, "civil_add_months", cr, doc, k,
                                    civil_add_months( d, k ), ref::add_months( d64, k, false ) );
                            record( r, "civil_add_months_clamped", cr, doc, k,
                                    civil_add_months_clamped( d, k ),
                                    ref::add_months( d64, k, true ) );
                            if( k < YEARS_PER_ERA )
                            {
                                record( r, "civil_add_years", cr, doc, k,
                                        civil_add_years( d, k ), ref::add_years( d64, k, false ) );
                                record( r, "civil_add_years_clamped", cr, doc, k,
                                        civil_add_years_clamped( d, k ),
                                        ref::add_years( d64, k, true ) );
                            }
                        }

                        // Symmetry B, on this representative: one era of shift
                        // moves the date by exactly one era of days.
                        for( i32 k = -MONTHS_PER_ERA; k < MONTHS_PER_ERA; k += 97 )
                        {
                            i64 lo = i64( civil_add_months( d, k ) ) - d64;
                            i64 hi = i64( civil_add_months( d, k + MONTHS_PER_ERA ) ) - d64;
                            record( r, "symmetry B (months)", cr, doc, k, hi - lo, DAYS_PER_ERA );
                        }
                        for( i32 k = -YEARS_PER_ERA; k < YEARS_PER_ERA; k += 11 )
                        {
                            i64 lo = i64( civil_add_years( d, k ) ) - d64;
                            i64 hi = i64( civil_add_years( d, k + YEARS_PER_ERA ) ) - d64;
                            record( r, "symmetry B (years)", cr, doc, k, hi - lo, DAYS_PER_ERA );
                        }

                        // Symmetry A: the same `c % 4` in a different century
                        // must give an identical delta.
                        for( u32 off = 4; off <= 12; off += 4 )
                        {
                            i64 e64 = rep_days( cr + off, doc );
                            auto e  = sys_days_t( e64 );
                            for( i32 k : { -1201, -13, 0, 1, 1200, 4799 } )
                            {
                                record( r, "symmetry A (months)", cr, doc, k,
                                        i64( civil_add_months( e, k ) ) - e64,
                                        i64( civil_add_months( d, k ) ) - d64 );
                                record( r, "symmetry A (years)", cr, doc, k,
                                        i64( civil_add_years( e, k ) ) - e64,
                                        i64( civil_add_years( d, k ) ) - d64 );
                            }
                        }
                    }
                }
            } );
        for( auto& w : workers ) w.join();
    }

    i64     checked = 0, failed = 0;
    Failure first{};
    for( auto const& r : results )
    {
        checked += r.checked;
        failed  += r.failed;
        if( r.first.seen && !first.seen ) first = r.first;
    }

    if( first.seen )
        fmt::println( "first failure: {}( {}, {} ) = {}, expected {}  (c%4={}, doc={})",
                      first.who, rep_days( first.cr, first.doc ), first.k,
                      first.got, first.want, first.cr, first.doc );

    // The workers cannot call inc() safely, so fold their count in here. This is
    // what makes the suite's assertion total reflect the work actually done.
    _test_count_assertions.count += size_t( checked );

    ASSERT_EQ( failed, i64( 0 ) );

    // The class count is a load-bearing part of the reduction: if a century
    // length or the era changed, the space enumerated above would no longer be
    // the whole space.
    u32 classes = 0;
    for( u32 cr = 0; cr < 4; ++cr ) classes += century_len( cr );
    ASSERT_EQ( i64( classes ), DAYS_PER_ERA );
}
