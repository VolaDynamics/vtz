#pragma once

#include <string>
#include <vtz/date_types.h>
#include <vtz/impl/bit.h>
#include <vtz/impl/math.h>
#include <vtz/strings.h>
#include <vtz/tz_reader/from_utc.h>

namespace vtz::_civ {
    // clang-format off
    /// Contains the last 2 bits of the number of days in a month
    /// (non-leap-year)
    ///
    /// See `days_per_month_standard` for example usage
    constexpr u32 DAYS_PER_MONTH_BITS_COMMON = 0b11'10'11'10'11'11'10'11'10'11'00'11'00;
    // month:                                    12 11 10 09 08 07 06 05 04 03 02 01

    /// Contains the last 2 bits of the number of days in a month
    /// (non-leap-year)
    ///
    /// See `days_per_month_standard` for example usage
    constexpr u32 DAYS_PER_MONTH_BITS_LEAP = 0b11'10'11'10'11'11'10'11'10'11'01'11'00;
    // month:                                  12 11 10 09 08 07 06 05 04 03 02 01
    // clang-format on

    /// Returns the number of days from March 1st to the first day of the given
    /// month, where the month is 0-based and March-based (0 = March, 11 = Feb).
    ///
    /// In a March-based year the months follow a regular pattern that repeats
    /// every 5 months (153 days), which is what makes this a closed form. This
    /// is the same expression used by to_civil0/resolve_civil0.
    VTZ_INLINE constexpr u32 month_start( u32 mp ) noexcept {
        return ( 153 * mp + 2 ) / 5;
    }


    /// Decoded form used by the civil_add_{months,years} family: just enough of
    /// a decomposition to count leap days between two years. Those functions
    /// only need *differences*, so the absolute year never appears. A century
    /// is the right window because inside one there are no 100- or 400-year
    /// leap exceptions, leaving only "divisible by 4" - a shift, not a
    /// division.
    ///
    /// The magic constants below are guarded by static_asserts in
    /// etc/test/test_impl/test_civil.cpp, keeping that machinery out of every
    /// TU that includes this header.
    struct century_parts {
        /// Century index, counting March-based centuries from
        /// CENTURY_ORIGIN. Only its low two bits are ever used
        /// (a century is a leap century iff `c % 4 == 0`).
        u32 c;
        /// Year within the century - [0, 99]
        u32 z;
        /// Day within the century - [0, 36524]
        u32 doc;
        /// Day of the March-based year - [0, 365]. 365 only on Feb 29th.
        u32 doy;
    };

    /// Epoch shift used by the civil_add_* family: 719468 (1970-01-01 ->
    /// 0000-03-01) plus 14695 whole 400-year eras. Whole eras cancel out of
    /// every difference we compute, and they lift the result above zero for any
    /// i32 input - so the decode stays unsigned, where truncating division is
    /// already floor division and needs no fixups.
    constexpr u32 CENTURY_ORIGIN = 2147614883u;

    /// Days in a century that does not end in a leap year. An era is 4 of them
    /// plus the leap day the divisible-by-400 rule puts back: 4 * 36524 + 1.
    constexpr u32 DAYS_PER_CENTURY = 36524u;

    // The origin must be a whole number of eras past the 0000-03-01 shift, or
    // the calendar would not line up...
    static_assert( ( CENTURY_ORIGIN - 719468u ) % 146097u == 0,
        "CENTURY_ORIGIN must be 719468 plus a whole number of eras" );
    // ...and at least 2^31, so the smallest i32 still maps above zero.
    static_assert( CENTURY_ORIGIN >= 2147483648u,
        "CENTURY_ORIGIN must be >= 2^31 so that u32( days ) + origin "
        "does not wrap for days == INT32_MIN" );

    /// Multiplier for extracting the century index from the shifted day count:
    /// `( ( n + 1 ) * CENTURY_MAGIC ) >> 47` is the number of whole
    /// centuries elapsed. Exact for every u32 day count.
    constexpr u64 CENTURY_MAGIC = 3853261555ull;
    constexpr u32 CENTURY_SHIFT = 47u;

    // The 64-bit product cannot overflow, even at the largest u32 input.
    static_assert(
        CENTURY_MAGIC <= 18446744073709551615ull / ( 4294967295ull + 1ull ),
        "( n + 1 ) * CENTURY_MAGIC must not overflow u64" );

    /// First day of century `m`, counting from the origin: 36524 days per
    /// century plus one leap day for each leap century (every 4th) passed.
    constexpr u32 century_start( u32 m ) noexcept {
        return DAYS_PER_CENTURY * m + ( m >> 2 );
    }

    /// The century index as computed by split_century.
    constexpr u32 century_of( u32 n ) noexcept {
        return u32( ( ( u64( n ) + 1 ) * CENTURY_MAGIC ) >> CENTURY_SHIFT );
    }

    /// Packed table of `30 - <0-based last day>` for each month of a
    /// March-based year, two bits per month, mp = 0 (March) in the low bits.
    /// Gives 30 29 30 29 30 30 29 30 29 30 30 27 - the trailing 27 is
    /// February, which gets +1 in a leap year.
    constexpr u32 MARCH_MONTH_LAST_BITS = 12652612u;

    /// Multiplier and shift that yield both the March-based month and the day
    /// within it from the day-of-year in one multiply: the high bits are
    /// `( 5 * doy + 2 ) / 153` and the low 14 hold `535 * dom`.
    ///
    /// Only exact for doy in [0, 365] - the range split_century
    /// produces. The first doy where it breaks is 428, so the margin is 63
    /// days; do not widen the doy range without re-deriving these.
    constexpr u32 MONTH_MAGIC_MUL   = 535u;
    constexpr u32 MONTH_MAGIC_ADD   = 331u;
    constexpr u32 MONTH_MAGIC_SHIFT = 14u;
    constexpr u32 MONTH_MAGIC_MASK  = ( 1u << MONTH_MAGIC_SHIFT ) - 1u;

    /// Split a day count into (century, year-of-century, day-of-century,
    /// day-of-year).
    ///
    /// The era split used by to_civil needs `( doe - doe / 1460 + doe / 36524
    /// - doe / 146096 ) / 365` - four divisions - to recover the year, because
    /// a 400-year era straddles the 100- and 400-year leap exceptions. Cutting
    /// on the century first costs one multiply-shift and leaves a window with
    /// no exceptions, so the year within it is one multiply-shift too.
    VTZ_INLINE constexpr century_parts split_century(
        sys_days_t days ) noexcept {
        // Unsigned from here on: the origin is large enough that this cannot
        // wrap below zero for any i32 input.
        u32 n   = u32( days ) + CENTURY_ORIGIN;
        u32 c   = century_of( n );
        u32 doc = n - century_start( c );
        // Inside a century every 4th year is a leap year with no exceptions, so
        // 4 years is exactly 1461 days. Scaling by 4 therefore lets a single
        // division recover the year, and the +3 lines the quotient up on year
        // boundaries (4 * 365 is 1460, one short of 1461).
        u32 t   = 4 * doc + 3;
        u32 z   = t / 1461;              // [0, 99]
        u32 doy = ( t - 1461 * z ) >> 2; // [0, ( 1461 - 1 ) >> 2] == [0, 365]
        return { c, z, doc, doy };
    }

    /// True if the March-based year `z` of century `c` ends with a Feb 29th.
    /// That February belongs to the *following* calendar year, hence the +1.
    /// Inside a century only the divisible-by-4 rule applies, so this is a mask
    /// rather than the full three-way is_leap test.
    VTZ_INLINE constexpr bool march_year_is_leap( u32 c, u32 z ) noexcept {
        // z == 99 is the only case where the following year leaves the century,
        // so it is the only place `c` matters.
        u32 w = z == 99 ? c : z;
        return ( w % 4 ) == 3;
    }

    /// Shared implementation of civil_add_months and civil_add_months_clamped.
    ///
    /// Returns `days + delta` instead of decoding to (year, month, day) and
    /// re-encoding. Month lengths look too irregular for that, but in a
    /// March-based year each month starts exactly `month_start` days in,
    /// so a whole-month shift is just the difference of two of those terms plus
    /// the leap days between the two years. February needs no special case
    /// either: it is the *last* month of a March-based year, so a leap day is
    /// only ever appended to a year end, never inserted mid-year.
    ///
    /// @tparam Clamp if true, the day of the month is clamped to the last day
    ///               of the target month, so Jan 31st + 1 month becomes Feb
    ///               28th. If false, it rolls over into the following month.
    template<bool Clamp>
    VTZ_INLINE constexpr sys_days_t add_months_impl(
        sys_days_t days, i32 months ) noexcept {
        auto parts = split_century( days );
        u32  c     = parts.c;
        u32  z     = parts.z;   // Year of century - [0, 99]
        u32  doy   = parts.doy; // Day of the March-based year - [0, 365]

        // Month and 0-based day within it from one multiply: high bits give the
        // month, low bits give 535 * dom.
        u32 t   = MONTH_MAGIC_MUL * doy + MONTH_MAGIC_ADD;
        u32 mp  = t >> MONTH_MAGIC_SHIFT; // [0, 11], 0 is March
        u32 dom = ( t & MONTH_MAGIC_MASK ) / MONTH_MAGIC_MUL;

        // Months from the start of the century to the target. One value carries
        // the whole target: /12 gives the target year-of-century (which may
        // fall outside [0, 99]) and /1200 the centuries crossed. Floor
        // division, because `total` goes negative for large negative offsets.
        i32 total = i32( 12 * z + mp ) + months;
        i32 yq    = math::div_floor<12>( total );
        i32 cq    = math::div_floor<1200>( total );
        u32 mp2   = u32( total - 12 * yq ); // [0, 11]
        u32 z2    = u32( yq - 100 * cq );   // [0, 99]

        // Leap days between source and target year. Inside a century that is
        // `z / 4`; each century crossed adds 24, plus one per leap century,
        // which is what the `( c & 3 ) + cq` shift counts. `c` enters only
        // through its low two bits, so the absolute century - and hence the era
        // - drops out entirely.
        i32 leap_delta
            = 24 * cq + ( ( i32( c & 3 ) + cq ) >> 2 ) + i32( z2 >> 2 );

        // 365 days per year plus the leap days, plus where the target month
        // starts in its year, minus where we started in the century, plus the
        // day within the month we keep.
        i32 delta = 365 * yq + leap_delta + i32( month_start( mp2 ) )
                    - i32( parts.doc ) + i32( dom );

        if constexpr( Clamp )
        {
            // The 0-based last day of the target month. February is the last
            // month of a March-based year, so it is the only one whose length
            // depends on the leap rule - and its field is the top one, reached
            // only when mp2 == 11. So the leap bump folds straight into the
            // table and costs nothing for the other eleven months.
            u32 bits = MARCH_MONTH_LAST_BITS
                       - ( u32( march_year_is_leap( c + u32( cq ), z2 ) )
                           << ( 2 * 11 ) );
            u32 last = 30 - ( ( bits >> ( 2 * mp2 ) ) & 3 );
            // Step back, so that eg Jan 31st + 1 month lands on Feb 28th
            delta -= dom > last ? i32( dom - last ) : 0;
        }

        return days + delta;
    }


    /// Shared implementation of civil_add_years and civil_add_years_clamped.
    ///
    /// Adding N years moves the date by `365 * N` plus the leap days crossed,
    /// so only the leap count has to be computed. This holds because we count
    /// from March 1st (the same epoch shift as to_civil0/resolve_civil0): Feb
    /// 29th is then the *last* day of the year, so every date keeps the same
    /// offset from its year's March 1st. Feb 29th itself is the sole exception,
    /// which is what @p Clamp handles.
    ///
    /// @tparam Clamp if true, Feb 29th + N years clamps back to Feb 28th when
    ///               the target year is not a leap year. If false, it rolls
    ///               over to Mar 1st.
    template<bool Clamp>
    VTZ_INLINE constexpr sys_days_t add_years_impl(
        sys_days_t days, i32 years ) noexcept {
        auto parts = split_century( days );
        u32  c     = parts.c;
        u32  z     = parts.z; // Year of century - [0, 99]

        // Move to the target year, carrying whole centuries out
        i32 total = i32( z ) + years;
        i32 cq    = math::div_floor<100>( total );
        u32 z2    = u32( total - 100 * cq ); // [0, 99]

        // Same leap-day count as add_months_impl
        i32 leap_delta
            = 24 * cq + ( ( i32( c & 3 ) + cq ) >> 2 ) + i32( z2 >> 2 );

        // 365 days per year, plus the leap days crossed, minus where we
        // started within the century
        i32 delta = 365 * years + leap_delta - i32( z >> 2 );

        if constexpr( Clamp )
        {
            // doy == 365 happens only on Feb 29th, since it is the last day of
            // a March-based year
            delta
                -= parts.doy == 365 && !march_year_is_leap( c + u32( cq ), z2 );
        }

        return days + delta;
    }

} // namespace vtz::_civ

namespace vtz {
    /// Writes the date as YYYYMMDD. Requires 8 characters of space
    constexpr void _write_yyyymmdd_8(
        i32 year, u16 month, u16 day, char* dest ) noexcept {
        int y0  = year / 1000;
        int y1  = ( year / 100 ) % 10;
        int y2  = ( year / 10 ) % 10;
        int y3  = year % 10;
        dest[0] = char( '0' + y0 );
        dest[1] = char( '0' + y1 );
        dest[2] = char( '0' + y2 );
        dest[3] = char( '0' + y3 );
        dest[4] = month >= 10 ? '1' : '0';
        dest[5] = char( ( month < 10 ? '0' : '&' ) + month );
        dest[6] = char( '0' + day / 10 );
        dest[7] = char( '0' + day % 10 );
    }
    /// Writes the date as YYYY-MM-DD (or uses a different separator, if
    /// specified)
    ///
    /// Requires 10 characters of space
    constexpr void _write_yyyymmdd_10(
        i32 year, u16 month, u16 day, char* dest, char sep = '-' ) noexcept {
        int y0  = year / 1000;
        int y1  = ( year / 100 ) % 10;
        int y2  = ( year / 10 ) % 10;
        int y3  = year % 10;
        dest[0] = char( '0' + y0 );
        dest[1] = char( '0' + y1 );
        dest[2] = char( '0' + y2 );
        dest[3] = char( '0' + y3 );
        dest[4] = sep;
        dest[5] = month >= 10 ? '1' : '0';
        dest[6] = char( ( month < 10 ? '0' : '&' ) + month );
        dest[7] = sep;
        dest[8] = char( '0' + day / 10 );
        dest[9] = char( '0' + day % 10 );
    }

    // Writes a time as hhmmss
    constexpr void _write_hhmmss_6( u32 t, char* p ) noexcept {
        int h  = t / 3600;
        t     %= 3600;
        int m  = t / 60;
        t     %= 60;
        int s  = t;
        p[0]   = char( '0' + h / 10 );
        p[1]   = char( '0' + h % 10 );
        p[2]   = char( '0' + m / 10 );
        p[3]   = char( '0' + m % 10 );
        p[4]   = char( '0' + s / 10 );
        p[5]   = char( '0' + s % 10 );
    }

    /// Writes a time as hh:mm:ss
    constexpr void _write_hhmmss_8( u32 t, char* p ) noexcept {
        int h  = t / 3600;
        t     %= 3600;
        int m  = t / 60;
        t     %= 60;
        int s  = t;
        p[0]   = char( '0' + h / 10 );
        p[1]   = char( '0' + h % 10 );
        p[2]   = char( ':' );
        p[3]   = char( '0' + m / 10 );
        p[4]   = char( '0' + m % 10 );
        p[5]   = char( ':' );
        p[6]   = char( '0' + s / 10 );
        p[7]   = char( '0' + s % 10 );
    }
} // namespace vtz

namespace vtz {
    using std::string;
    /// Represents a date as a number of days from the epoch
    ///
    /// sys_days_t(0) is 1970-01-01, sys_days_t(1) is 1970-01-02, etc
    using sys_days_t = i32;
    /// Seconds from epoch
    using sys_seconds_t = i64;

    /// Holds seconds (unspecified if it's local or UTC)
    using sec_t = i64;

    /// Holds nanoseconds (unspecified if it's local or UTC)
    using nanos_t = i64;

    constexpr inline sys_days_t MAX_DAYS = INT32_MAX;

    constexpr sys_seconds_t days_to_seconds( i64 days ) noexcept {
        return days * 86400;
    }

    constexpr sys_seconds_t days_to_seconds(
        i64 days, i64 hr, i64 min, i64 sec ) noexcept {
        return days * 86400 + 3600 * hr + 60 * min + sec;
    }

    /// Write an offset (in seconds) as [+/-]HH[:MM[:SS]], writing the shortest
    /// amount necessary.
    ///
    /// Examples:
    /// - `write_shortest_offset(0, ...) -> "+00"`
    /// - `write_shortest_offset(7200, ...) -> "+02"`
    /// - `write_shortest_offset(7260, ...) -> "+0201"`
    /// - `write_shortest_offset(7201, ...) -> "+020001"`
    /// - `write_shortest_offset(7261, ...) -> "+020101"`
    /// - `write_shortest_offset(-7200, ...) -> "-02"`
    /// - `write_shortest_offset(-7260, ...) -> "-0201"`
    /// - `write_shortest_offset(-7201, ...) -> "-020001"`
    /// - `write_shortest_offset(-7261, ...) -> "-020101"`
    constexpr size_t write_shortest_offset( i32 offset, char* p ) noexcept {
        i32 abs_offset = offset < 0 ? -offset : offset;
        i32 hours      = abs_offset / 3600;
        i32 minutes    = ( abs_offset % 3600 ) / 60;
        i32 seconds    = abs_offset % 60;
        p[0]           = offset < 0 ? '-' : '+';
        p[1]           = char( '0' + ( hours / 10 ) );
        p[2]           = char( '0' + ( hours % 10 ) );
        if( minutes || seconds )
        {
            p[3] = char( '0' + ( minutes / 10 ) );
            p[4] = char( '0' + ( minutes % 10 ) );

            if( seconds )
            {
                p[5] = char( '0' + ( seconds / 10 ) );
                p[6] = char( '0' + ( seconds % 10 ) );
                return 7;
            }
            return 5;
        }
        return 3;
    }

    /// Holds a (year, day of year) pair
    struct year_doy {
        /// Holds the year
        i32 year;
        /// 0-based day of the year
        i32 doy;
    };

    struct alignas( u64 ) civil_ymd {
        i32 year;
        u16 month;
        u16 day;

        civil_ymd() = default;

        constexpr civil_ymd( i32 year, u16 mon, u16 day ) noexcept
        : year( year )
        , month( mon )
        , day( day ) {}

        constexpr civil_ymd( i32 year, month_t mon, u16 day ) noexcept
        : year( year )
        , month( u16( mon ) )
        , day( day ) {}

        constexpr month_t mon() const noexcept { return month_t( month ); }

        constexpr bool operator==( civil_ymd const& rhs ) const noexcept {
            return year == rhs.year && month == rhs.month && day == rhs.day;
        }

        /// Writes the date as YYYY-MM-DD. Requires 10 characters of space.
        constexpr void write( char* dest, char sep = '-' ) const noexcept {
            _write_yyyymmdd_10( year, month, day, dest, sep );
        }
        string str( char sep ) const {
            char buffer[10]{};
            write( buffer, sep );
            return std::string( buffer, 10 );
        }

        string str() const { return str( '-' ); }
    };


    inline string format_as( civil_ymd ymd ) { return ymd.str(); }


    /// Checks if the given year is a leap year as per the international
    /// standard date system
    ///
    /// This implementation permits a year 0, and the year 0 is a leap year.
    constexpr bool is_leap( i32 year ) noexcept {
        bool div4   = year % 4 == 0;
        bool div100 = year % 100 == 0;
        bool div400 = year % 400 == 0;
        // A year is a leap year if it's divisible by 4,
        // but NOT if it's divisible by 100,
        // UNLESS it's divisible by 400
        return div4 && ( !div100 || div400 );
    }


    /// Returns the last day of the month given the month, and a param
    /// specifying if the year is a
    /// leap year
    ///
    /// @param month the month (1-12)
    /// @param is_leap true if the year is a leap year
    constexpr u32 last_day_of_month_by_leap(
        u32 month, bool is_leap ) noexcept {
        u32 bits = _civ::DAYS_PER_MONTH_BITS_COMMON | ( u32( is_leap ) << 4 );
        return ( 28 | ( ( bits >> ( 2 * month ) ) & 0x3 ) );
    }


    /// Get the last day of the month
    constexpr u32 last_day_of_month( i32 year, u32 month ) noexcept {
        return last_day_of_month_by_leap( month, is_leap( year ) );
    }


    /// Preconditions: x <= 6 && y <= 6
    /// Returns: The number of days from the weekday y to the weekday x.
    /// The result is always in the range [0, 6].
    constexpr u32 weekday_difference( u32 x, u32 y ) noexcept {
        x -= y;
        return x <= 6 ? x : x + 7;
    }

    /// Return 0-based year/month/day, so month is 0-11 and day is 0-30
    constexpr civil_ymd to_civil0( sys_days_t days ) noexcept {
        days += 719468; // Shift the epoch from 1970-01-01 to 0000-03-01
        const i64 era = ( days >= 0 ? days : days - 146096 ) / 146097;
        const u32 doe = u32( days - era * 146097 ); // [0, 146096]
        const u32 yoe = ( doe - doe / 1460 + doe / 36524 - doe / 146096 )
                        / 365;                      // [0, 399]
        const i64 y   = i64( yoe ) + era * 400;
        const u32 doy = doe - ( 365 * yoe + yoe / 4 - yoe / 100 ); // [0, 365]
        const u32 mp  = ( 5 * doy + 2 ) / 153;                     // [0, 11]
        const u32 d   = doy - ( 153 * mp + 2 ) / 5;                // [0, 30]
        const u32 m   = mp < 10 ? mp + 2 : mp - 10;                // [0, 11]
        return civil_ymd{ i32( y + ( m <= 1 ) ), u16( m ), u16( d ) };
    }

    constexpr sys_days_t resolve_civil0( i32 y, u32 m, u32 d ) noexcept {
        y -= m <= 1;

        i32 era = ( y >= 0 ? y : y - 399 ) / 400;
        u32 yoe = static_cast<u32>( y - era * 400 );                // [0, 399]
        u32 doy = ( 153 * ( m > 1 ? m - 2 : m + 10 ) + 2 ) / 5 + d; // [0, 365]
        u32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; // [0, 146096]
        return era * 146097 + static_cast<i32>( doe ) - 719468;
    }

    /// Given a year, month, and day, obtain the number of days since the epoch
    ///
    /// Based on reference implementation by Howard Hinnant provided here:
    /// https://howardhinnant.github.io/date_algorithms.html#days_from_civil
    ///
    /// @param y year
    /// @param m month
    /// @param d day
    template<class IntT>
    constexpr sys_days_t resolve_civil( IntT y, u32 m, u32 d ) noexcept {
        y -= m <= 2;

        auto era_parts = vtz::math::div_floor2<400>( y );
        i64  era       = era_parts.quot;
        u32  yoe       = u32( era_parts.rem );                     // [0, 399]
        u32  doy
            = ( 153 * ( m > 2 ? m - 3 : m + 9 ) + 2 ) / 5 + d - 1; // [0, 365]
        u32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; // [0, 146096]
        return sys_days_t( era * 146097 + static_cast<i32>( doe ) - 719468 );
    }

    /// Returns a civil date (as days since epoch) based on Jan 1st of the given
    /// year
    constexpr sys_days_t resolve_civil( i32 y ) noexcept {
        auto era_parts = vtz::math::div_floor2<400>( y - 1 );
        i64  era       = era_parts.quot;
        i32  yoe       = era_parts.rem;                         // [0, 399]
        i32  doe       = yoe * 365 + yoe / 4 - yoe / 100 + 306; // [0, 146096]
        return sys_days_t( era * 146097 + doe - 719468 );
    }

    /// Resolve a civil date, expressed as (year, doy), where doy starts at 1
    constexpr sys_days_t resolve_civil_ordinal( i32 y, i32 doy ) noexcept {
        return resolve_civil( y ) + doy - 1;
    }


    constexpr sys_seconds_t resolve_civil_time(
        i32 y, u32 m, u32 d, int h, int min, int sec ) noexcept {
        return i64( resolve_civil( y, m, d ) ) * 86400 + 3600 * h + 60 * min
               + sec;
    }

    /// Given a year, month, and day, obtain the number of days since the epoch
    ///
    /// Based on reference implementation by Howard Hinnant provided here:
    /// https://howardhinnant.github.io/date_algorithms.html#days_from_civil
    ///
    /// @param y year
    /// @param m month
    /// @param d day
    constexpr sys_days_t resolve_civil( i32 y, month_t m, u32 d ) noexcept {
        return resolve_civil( y, u32( m ), d );
    }

    /// Get a date as a year, month, and day. The return value is civil_ymd,
    /// which holds a Year/Month/Day triplet
    ///
    /// Based on reference implementation by Howard Hinnant provided here:
    /// https://howardhinnant.github.io/date_algorithms.html#civil_from_days
    ///
    /// @param days Days since the epoch (The epoch being January 1st, 1970)
    constexpr civil_ymd to_civil( sys_days_t days ) noexcept {
        days += 719468; // Shift the epoch from 1970-01-01 to 0000-03-01
        const auto parts = math::div_floor2<146097>( days );
        i32        era   = parts.quot;
        u32        doe   = parts.rem; // Day within era - [0, 146096]
        const u32  yoe
            = ( doe - doe / 1460 + doe / 36524 - int( doe >= 146096 ) )
              / 365;                                               // [0, 399]
        const i32 y   = i32( yoe ) + era * 400;
        const u32 doy = doe - ( 365 * yoe + yoe / 4 - yoe / 100 ); // [0, 365]
        const u32 mp  = ( 5 * doy + 2 ) / 153;                     // [0, 11]
        const u32 d   = doy - ( 153 * mp + 2 ) / 5 + 1;            // [1, 31]
        const u32 m   = mp < 10 ? mp + 3 : mp - 9;                 // [1, 12]
        return civil_ymd{ i32( y + ( m <= 2 ) ), u16( m ), u16( d ) };
    }

    template<class I1, class Year, class Month, class Day>
    constexpr void to_civil(
        I1 days, Year& _year, Month& _mon, Day& _day ) noexcept {
        days += 719468; // Shift the epoch from 1970-01-01 to 0000-03-01
        const auto parts = math::div_floor2<146097>( days );
        i64        era   = parts.quot;
        u64        doe   = parts.rem; // Day within era - [0, 146096]
        const u64  yoe
            = ( doe - doe / 1460 + doe / 36524 - int( doe >= 146096 ) )
              / 365; // [0, 399]
        const i64 y = i64( yoe ) + era * 400;
        const u32 doy
            = u32( doe - ( 365 * yoe + yoe / 4 - yoe / 100 ) ); // [0, 365]
        const u32 mp = ( 5 * doy + 2 ) / 153;                   // [0, 11]
        const u32 d  = doy - ( 153 * mp + 2 ) / 5 + 1;          // [1, 31]
        const u32 m  = mp < 10 ? mp + 3 : mp - 9;               // [1, 12]
        _year        = Year( y + ( m <= 2 ) );
        _mon         = Month( m );
        _day         = Day( d );
    }


    /// Get the year and the day of the year. Jan 1st 2025 -> (2025, 0)
    constexpr year_doy to_civil_year_doy( sys_days_t days ) noexcept {
        days += 719468; // Shift the epoch from 1970-01-01 to 0000-03-01
        const auto parts = math::div_floor2<146097>( days );
        i32        era   = parts.quot;
        u32        doe   = parts.rem; // Day within era - [0, 146096]
        const u32  yoe
            = ( doe - doe / 1460 + doe / 36524 - int( doe >= 146096 ) )
              / 365;                                               // [0, 399]
        const i32 y   = i32( yoe ) + era * 400;
        const u32 doy = doe - ( 365 * yoe + yoe / 4 - yoe / 100 ); // [0, 365]
        bool      needs_adj  = doy >= 306;
        i32       y2         = y + int( needs_adj );
        int       doy_addend = 59 + int( is_leap( yoe ) );
        i32       doy2       = doy + ( doy >= 306 ? -306 : doy_addend );
        return year_doy{ y2, doy2 };
    }

    constexpr i32 civil_year( sys_days_t days ) noexcept {
        return to_civil( days ).year;
    }

    constexpr u16 civil_month( sys_days_t days ) noexcept {
        return to_civil( days ).month;
    }
    constexpr u16 civil_day_of_month( sys_days_t days ) noexcept {
        return to_civil( days ).day;
    }

    /// Return 0-based month (January -> 0)
    constexpr u16 civil_month0( sys_days_t days ) noexcept {
        return to_civil0( days ).month;
    }

    /// Return the 0-based day of the month (1st of month -> 0).
    /// This is the number of days since the 1st of the month.
    constexpr u16 civil_day_of_month0( sys_days_t days ) noexcept {
        return to_civil0( days ).day;
    }

    /// Return the date corresponding to the first day of the year
    /// Eg, Dec 13 2025 -> Jan 1st, 2025
    constexpr sys_days_t civil_boy( sys_days_t days ) noexcept {
        return days - to_civil_year_doy( days ).doy;
    }

    /// Return the date corresponding to the last day of the year
    /// Eg, Dec 13 2025 -> Dec 31st, 2025
    constexpr sys_days_t civil_eoy( sys_days_t days ) noexcept {
        auto _year_doy = to_civil_year_doy( days );
        // 364 for regular years, 365 for leap years
        auto last_doy = 364 + int( is_leap( _year_doy.year ) );
        return days + last_doy - _year_doy.doy;
    }

    /// Add months to the date. Eg, (Dec 13 2025) + 3 months becomes
    /// Mar 13 2026
    ///
    /// Note that the day of the month rolls over when the target month is too
    /// short: Jan 31st + 1 month becomes Mar 3rd. Use civil_add_months_clamped
    /// to get Feb 28th instead.
    constexpr sys_days_t civil_add_months(
        sys_days_t days, i32 months ) noexcept {
        return _civ::add_months_impl<false>( days, months );
    }

    /// Add years to the date. Eg, (Dec 13 2025) + 3 years becomes
    /// Dec 13 2028
    ///
    /// Note that Feb 29th + N years rolls over to Mar 1st when the target year
    /// is not a leap year. Use civil_add_years_clamped to get Feb 28th instead.
    constexpr sys_days_t civil_add_years(
        sys_days_t days, i32 years ) noexcept {
        return _civ::add_years_impl<false>( days, years );
    }

    /// Add months to the date. Eg, (Dec 13 2025) + 3 months becomes
    /// Mar 13 2026.
    ///
    /// Clamps the day of the month, so that we don't have rollover:
    /// Jan 31st + 1 month becomes Feb 28th (or Feb 29th, in leap years)
    constexpr sys_days_t civil_add_months_clamped(
        sys_days_t days, i32 months ) noexcept {
        return _civ::add_months_impl<true>( days, months );
    }


    /// Add years to the date. Eg, (Dec 13 2025) + 3 years becomes
    /// Dec 13 2028
    ///
    /// Clamps the day of the month, so that we don't have rollover:
    /// Feb 29st + 1 year becomes Feb 28th
    constexpr sys_days_t civil_add_years_clamped(
        sys_days_t days, i32 years ) noexcept {
        return _civ::add_years_impl<true>( days, years );
    }

    /// Get the beginning of the month, as days since the epoch. Eg, Dec 13 2025
    /// -> Dec 1st 2025
    constexpr sys_days_t civil_bom( sys_days_t days ) noexcept {
        return days - civil_day_of_month0( days );
    }

    constexpr sys_days_t civil_eom( sys_days_t days ) noexcept {
        auto ymd      = to_civil( days );
        auto last_dom = last_day_of_month( ymd.year, ymd.month );
        // Add as many days as needed to get to the last day of the month
        return days + ( last_dom - ymd.day );
    }

    /// Returns the weekday for the given date, expressed as a number of days
    /// since the epoch, where 0=Sun, 1=Mon, etc
    constexpr dow_t dow_from_days( sys_days_t days_from_epoch ) noexcept {
        return dow_t( ( i64( days_from_epoch ) + 0x80000002ull ) % 7 );
    }

    constexpr dow_t dow_from_days( i64 days_from_epoch ) noexcept {
        auto result = vtz::math::rem<7>( days_from_epoch ) + 4;
        auto r2     = result - 7;
        return dow_t( result >= 7 ? r2 : result );
    }

    /// Given a date (eg, 2025 Oct 11, or 2025-10-11), get the weekday as a
    /// number 0-6 where 0=Sun

    constexpr dow_t dow_from_civil( i32 year, u32 month, u32 dom ) noexcept {
        return dow_from_days( resolve_civil( year, month, dom ) );
    }


    /// Given a year, a month [1-12], and a day of the week, return the day
    /// of the last instance of the given weekday in that month.
    ///
    /// For example, to get the last Sunday in October in 2025, you would
    /// call: get_last_day_of_week_in_month( 2025, 10, 0 )
    ///
    /// (because Sun=0)
    constexpr u32 get_last_dowin_month(
        i32 year, u32 month, dow_t dow ) noexcept {
        u32  last     = last_day_of_month( year, month );
        auto dow_last = dow_from_civil( year, month, last );
        u32  day      = last - u32( dow_last - dow );
        return day;
    }


    /// Resolve a date such as '1966 Oct Sun>=10' - returns the first Sunday on
    /// or after October 10, 1966
    constexpr sys_days_t resolve_dow_ge(
        i32 year, u32 month, u32 dom, dow_t dow ) noexcept {
        sys_days_t d = resolve_civil( year, month, dom );
        // Add as many days as needed to get to the desired weekday
        d += sys_days_t( dow - dow_from_days( d ) );
        return d;
    }

    /// Resolve a date such as '2012 Apr Fri<=1' - returns the first Friday on
    /// or before April 1st, 2012
    constexpr sys_days_t resolve_dow_le(
        i32 year, u32 month, u32 dom, dow_t dow ) noexcept {
        sys_days_t d = resolve_civil( year, month, dom );
        // Subtract as many days as needed to get from the desired weekday
        d -= sys_days_t( dow_from_days( d ) - dow );
        return d;
    }


    constexpr sys_days_t resolve_last_dow(
        i32 year, u32 month, dow_t dow ) noexcept {
        // Last day of the month
        u32 last_dom = last_day_of_month( year, month );
        // sys_days_t of that date
        sys_days_t d = resolve_civil( year, month, last_dom );
        // Weekday of the last day of the month
        dow_t dow_eom = dow_from_days( d );

        // Suppose the last day of the month is a Tuesday, but we want the last
        // _Sunday_ of the month. (Tuesday - Sunday) is 2, because it takes 2
        // days to go from Sunday to Tuesday.
        //
        // So, we return (date of last day of the month) - (2 days)
        i32 delta = i32( dow_eom - dow );

        return d - delta;
    }


    constexpr civil_ymd get_ymd_dow_ge(
        i32 year, u32 month, u32 dom, dow_t dow ) noexcept {
        u32 last_dom = last_day_of_month( year, month );

        // Add as many days as needed to get to the desired weekday
        dom += u32( dow - dow_from_civil( year, month, dom ) );

        if( dom > last_dom )
        {
            month += 1;
            dom   -= last_dom;
            if( month > 12 )
            {
                month -= 12;
                year  += 1;
            }
        }

        return civil_ymd{ year, u16( month ), u16( dom ) };
    }

    constexpr civil_ymd get_ymd_dow_le(
        i32 year, u32 month, u32 dom, dow_t dow ) noexcept {
        // Add as many days as needed to get to the desired weekday
        auto dom2
            = i32( dom ) - i32( dow_from_civil( year, month, dom ) - dow );

        if( dom2 < 1 )
        {
            month -= 1;
            if( month == 0 )
            {
                month += 12;
                year  -= 1;
            }
            dom2 += last_day_of_month( year, month );
        }

        return civil_ymd{ year, u16( month ), u16( dom2 ) };
    }

    constexpr sys_seconds_t sysseconds( sys_days_t date, i32 offset ) {
        return i64( date ) * 86400 + offset;
    }

    static_assert(
        get_ymd_dow_le( 2025, 10, 3, dow_t::Sun ) == civil_ymd{ 2025, 9, 28 } );

    static_assert(
        get_ymd_dow_le( 2025, 1, 1, dow_t::Sun ) == civil_ymd{ 2024, 12, 29 } );
    static_assert(
        get_ymd_dow_le( 2025, 4, 1, dow_t::Sat ) == civil_ymd{ 2025, 3, 29 } );
    static_assert(
        get_ymd_dow_le( 2025, 4, 1, dow_t::Sun ) == civil_ymd{ 2025, 3, 30 } );
    static_assert(
        get_ymd_dow_le( 2025, 4, 1, dow_t::Mon ) == civil_ymd{ 2025, 3, 31 } );


    // 2025 Oct Sat>=11 == 2025 Oct 11
    static_assert( get_ymd_dow_ge( 2025, 10, 11, dow_t::Sat )
                   == civil_ymd{ 2025, 10, 11 } );
    // 2025 Oct Sun>=11 == 2025 Oct 12
    static_assert( get_ymd_dow_ge( 2025, 10, 11, dow_t::Sun )
                   == civil_ymd{ 2025, 10, 12 } );
    // 2025 Oct Mon>=11 == 2025 Oct 13
    static_assert( get_ymd_dow_ge( 2025, 10, 11, dow_t::Mon )
                   == civil_ymd{ 2025, 10, 13 } );


    static_assert( dow_from_civil( 2025, 10, 11 ) == dow_t::Sat );
    static_assert( get_last_dowin_month( 2025, 10, dow_t::Sun ) == 26 );
    static_assert( get_last_dowin_month( 2025, 10, dow_t::Mon ) == 27 );
    static_assert( get_last_dowin_month( 2025, 10, dow_t::Tue ) == 28 );
    static_assert( get_last_dowin_month( 2025, 10, dow_t::Wed ) == 29 );
    static_assert( get_last_dowin_month( 2025, 10, dow_t::Thu ) == 30 );
    static_assert( get_last_dowin_month( 2025, 10, dow_t::Fri ) == 31 );
    static_assert( get_last_dowin_month( 2025, 10, dow_t::Sat ) == 25 );

    /// Writes a timestamp as `YYYY-MM-DD HH:MM:SS`.
    ///
    /// Writes precisely 19 characters to the given buffer.
    constexpr void _write_timestamp( sys_seconds_t T,
        char*                                      p,
        char                                       date_sep      = '-',
        char                                       date_time_sep = ' ' ) {
        auto date_and_time = math::div_floor2<86400>( T );

        auto date = sys_days_t( date_and_time.quot );
        auto t    = u32( date_and_time.rem );

        // Write the date
        auto ymd = to_civil( date );
        _write_yyyymmdd_10( ymd.year, ymd.month, ymd.day, p, date_sep );
        p[10] = date_time_sep;
        _write_hhmmss_8( t, p + 11 );
    }

    /// Writes a timestamp as `YYYYMMDD HHMMSS`.
    ///
    /// Writes precisely 15 characters to the given buffer.
    constexpr void _write_timestamp_compact(
        sys_seconds_t T, char* p, char date_time_sep = ' ' ) {
        auto date_and_time = math::div_floor2<86400>( T );

        auto date = sys_days_t( date_and_time.quot );
        auto t    = u32( date_and_time.rem );

        // Write the date
        auto ymd = to_civil( date );
        _write_yyyymmdd_8( ymd.year, ymd.month, ymd.day, p );
        p[8] = date_time_sep;
        _write_hhmmss_6( t, p + 9 );
    }

    constexpr std::string_view write_timestamp_to_sv( sys_seconds_t T,
        char*                                                       p,
        char date_sep      = '-',
        char date_time_sep = ' ' ) {
        _write_timestamp( T, p, date_sep, date_time_sep );
        return std::string_view( p, 19 );
    }

    inline std::string utc_to_string(
        sys_seconds_t sec, char date_sep = '-', char date_time_sep = ' ' ) {
        char buff[20];
        _write_timestamp( sec, buff, date_sep, date_time_sep );
        buff[19] = 'Z';
        return std::string( buff, 20 );
    }

    template<size_t N>
    inline std::string local_to_string(
        sys_seconds_t sec, from_utc off, fix_str<N> const& abbr ) {
        char buff[20 + N];
        _write_timestamp( off.to_local( sec ), buff, '-', ' ' );
        buff[19] = ' ';
        _vtz_memcpy( buff + 20, abbr.buff_, N );
        return std::string( buff, 20 + abbr.size() );
    }

    struct _dt {
        int64_t sec;

        string str() const {
            char dest[20]{};
            return string( write_timestamp_to_sv( sec, dest ) );
        }

        constexpr static _dt civil(
            i32 y, u32 m, u32 d, int h, int min, int sec ) noexcept {
            return { resolve_civil_time( y, m, d, h, min, sec ) };
        }

        bool operator==( _dt const& rhs ) const noexcept {
            return sec == rhs.sec;
        }
    };

    inline string format_as( _dt T ) { return T.str(); }
} // namespace vtz
