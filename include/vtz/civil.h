#pragma once

#include <string>
#include <vtz/bit.h>
#include <vtz/date_types.h>
#include <vtz/math.h>
#include <vtz/strings.h>
#include <vtz/tz_reader/FromUTC.h>

namespace vtz::detail {
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
} // namespace vtz::detail

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
        p[0]   = '0' + h / 10;
        p[1]   = '0' + h % 10;
        p[2]   = '0' + m / 10;
        p[3]   = '0' + m % 10;
        p[4]   = '0' + s / 10;
        p[5]   = '0' + s % 10;
    }

    /// Writes a time as hh:mm:ss
    constexpr void _write_hhmmss_8( u32 t, char* p ) noexcept {
        int h  = t / 3600;
        t     %= 3600;
        int m  = t / 60;
        t     %= 60;
        int s  = t;
        p[0]   = '0' + h / 10;
        p[1]   = '0' + h % 10;
        p[2]   = ':';
        p[3]   = '0' + m / 10;
        p[4]   = '0' + m % 10;
        p[5]   = ':';
        p[6]   = '0' + s / 10;
        p[7]   = '0' + s % 10;
    }
} // namespace vtz

namespace vtz {
    using std::string;
    /// Represents a date as a number of days from the epoch
    ///
    /// sysdays_t(0) is 1970-01-01, sysdays_t(1) is 1970-01-02, etc
    using sysdays_t = i32;
    /// Seconds from epoch
    using sysseconds_t = i64;

    /// Holds seconds (unspecified if it's local or UTC)
    using sec_t = i64;

    constexpr inline sysdays_t MAX_DAYS = INT32_MAX;

    constexpr sysseconds_t daysToSeconds( i64 days ) noexcept {
        return days * 86400;
    }

    constexpr sysseconds_t daysToSeconds(
        i64 days, i64 hr, i64 min, i64 sec ) noexcept {
        return days * 86400 + 3600 * hr + 60 * min + sec;
    }

    /// Write an offset (in seconds) as [+/-]HH[:MM[:SS]], writing the shortest
    /// amount necessary.
    ///
    /// Examples:
    /// - `writeShortestOffset(0, ...) -> "+00"`
    /// - `writeShortestOffset(7200, ...) -> "+02"`
    /// - `writeShortestOffset(7260, ...) -> "+0201"`
    /// - `writeShortestOffset(7201, ...) -> "+020001"`
    /// - `writeShortestOffset(7261, ...) -> "+020101"`
    /// - `writeShortestOffset(-7200, ...) -> "-02"`
    /// - `writeShortestOffset(-7260, ...) -> "-0201"`
    /// - `writeShortestOffset(-7201, ...) -> "-020001"`
    /// - `writeShortestOffset(-7261, ...) -> "-020101"`
    constexpr size_t writeShortestOffset( i32 offset, char* p ) noexcept {
        i32 absOffset = offset < 0 ? -offset : offset;
        i32 hours     = absOffset / 3600;
        i32 minutes   = ( absOffset % 3600 ) / 60;
        i32 seconds   = absOffset % 60;
        p[0]          = offset < 0 ? '-' : '+';
        p[1]          = '0' + ( hours / 10 );
        p[2]          = '0' + ( hours % 10 );
        if( minutes || seconds )
        {
            p[3] = '0' + ( minutes / 10 );
            p[4] = '0' + ( minutes % 10 );

            if( seconds )
            {
                p[5] = '0' + ( seconds / 10 );
                p[6] = '0' + ( seconds % 10 );
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

    struct alignas( u64 ) YMD {
        i32 year;
        u16 month;
        u16 day;

        constexpr YMD( i32 year, u16 mon, u16 day ) noexcept
        : year( year )
        , month( mon )
        , day( day ) {}

        constexpr YMD( i32 year, Mon mon, u16 day ) noexcept
        : year( year )
        , month( u16( mon ) )
        , day( day ) {}

        constexpr Mon mon() const noexcept { return Mon( month ); }

        constexpr bool operator==( YMD const& rhs ) const noexcept {
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


    inline string format_as( YMD ymd ) { return ymd.str(); }


    /// Checks if the given year is a leap year as per the international
    /// standard date system
    ///
    /// This implementation permits a year 0, and the year 0 is a leap year.
    constexpr bool isLeap( i32 year ) noexcept {
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
    constexpr u32 lastDayOfMonthByLeap( u32 month, bool is_leap ) noexcept {
        u32 bits = detail::DAYS_PER_MONTH_BITS_COMMON | ( u32( is_leap ) << 4 );
        return ( 28 | ( ( bits >> ( 2 * month ) ) & 0x3 ) );
    }


    /// Get the last day of the month
    constexpr u32 lastDayOfMonth( i32 year, u32 month ) noexcept {
        return lastDayOfMonthByLeap( month, isLeap( year ) );
    }


    /// Preconditions: x <= 6 && y <= 6
    /// Returns: The number of days from the weekday y to the weekday x.
    /// The result is always in the range [0, 6].
    constexpr u32 weekdayDifference( u32 x, u32 y ) noexcept {
        x -= y;
        return x <= 6 ? x : x + 7;
    }

    /// Return 0-based year/month/day, so month is 0-11 and day is 0-30
    constexpr YMD toCivil0( sysdays_t days ) noexcept {
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
        return YMD{ i32( y + ( m <= 1 ) ), u16( m ), u16( d ) };
    }

    constexpr sysdays_t resolveCivil0( i32 y, u32 m, u32 d ) noexcept {
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
    constexpr sysdays_t resolveCivil( i32 y, u32 m, u32 d ) noexcept {
        y -= m <= 2;

        auto eraParts = vtz::math::divFloor2<400>( y );
        i64  era      = eraParts.quot;
        u32  yoe      = u32( eraParts.rem );                       // [0, 399]
        u32  doy
            = ( 153 * ( m > 2 ? m - 3 : m + 9 ) + 2 ) / 5 + d - 1; // [0, 365]
        u32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; // [0, 146096]
        return era * 146097 + static_cast<i32>( doe ) - 719468;
    }

    /// Returns a civil date (as days since epoch) based on Jan 1st of the given
    /// year
    constexpr sysdays_t resolveCivil( i32 y ) noexcept {
        auto eraParts = vtz::math::divFloor2<400>( y - 1 );
        i64  era      = eraParts.quot;
        i32  yoe      = eraParts.rem;                          // [0, 399]
        i32  doe      = yoe * 365 + yoe / 4 - yoe / 100 + 306; // [0, 146096]
        return era * 146097 + doe - 719468;
    }

    constexpr sysseconds_t resolveCivilTime(
        i32 y, u32 m, u32 d, int h, int min, int sec ) noexcept {
        return i64( resolveCivil( y, m, d ) ) * 86400 + 3600 * h + 60 * min
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
    constexpr sysdays_t resolveCivil( i32 y, Mon m, u32 d ) noexcept {
        return resolveCivil( y, u32( m ), d );
    }

    /// Get a date as a year, month, and day. The return value is YMD, which
    /// holds a Year/Month/Day triplet
    ///
    /// Based on reference implementation by Howard Hinnant provided here:
    /// https://howardhinnant.github.io/date_algorithms.html#civil_from_days
    ///
    /// @param days Days since the epoch (The epoch being January 1st, 1970)
    constexpr YMD toCivil( sysdays_t days ) noexcept {
        days += 719468; // Shift the epoch from 1970-01-01 to 0000-03-01
        const i64 era = ( days >= 0 ? days : days - 146096 ) / 146097;
        const u32 doe = u32( days - era * 146097 ); // [0, 146096]
        const u32 yoe = ( doe - doe / 1460 + doe / 36524 - doe / 146096 )
                        / 365;                      // [0, 399]
        const i64 y   = i64( yoe ) + era * 400;
        const u32 doy = doe - ( 365 * yoe + yoe / 4 - yoe / 100 ); // [0, 365]
        const u32 mp  = ( 5 * doy + 2 ) / 153;                     // [0, 11]
        const u32 d   = doy - ( 153 * mp + 2 ) / 5 + 1;            // [1, 31]
        const u32 m   = mp < 10 ? mp + 3 : mp - 9;                 // [1, 12]
        return YMD{ i32( y + ( m <= 2 ) ), u16( m ), u16( d ) };
    }


    /// Get the year and the day of the year. Jan 1st 2025 -> (2025, 0)
    constexpr year_doy toCivilYearDOY( sysdays_t days ) noexcept {
        days += 719468; // Shift the epoch from 1970-01-01 to 0000-03-01
        const auto parts = math::divFloor2<146097>( days );
        i32        era   = parts.quot;
        u32        doe   = parts.rem; // Day within era - [0, 146096]
        const u32  yoe
            = ( doe - doe / 1460 + doe / 36524 - int( doe >= 146096 ) )
              / 365;                                               // [0, 399]
        const i32 y   = i32( yoe ) + era * 400;
        const u32 doy = doe - ( 365 * yoe + yoe / 4 - yoe / 100 ); // [0, 365]
        bool      needsAdj  = doy >= 306;
        i32       y2        = y + int( needsAdj );
        int       doyAddend = 59 + int( isLeap( yoe ) );
        i32       doy2      = doy + ( doy >= 306 ? -306 : doyAddend );
        return year_doy{ y2, doy2 };
    }

    constexpr i32 civilYear( sysdays_t days ) noexcept {
        return toCivilYearDOY( days ).year;
    }

    constexpr u16 civilMonth( sysdays_t days ) noexcept {
        return toCivil( days ).month;
    }
    constexpr u16 civilDayOfMonth( sysdays_t days ) noexcept {
        return toCivil( days ).day;
    }

    /// Return 0-based month (January -> 0)
    constexpr u16 civilMonth0( sysdays_t days ) noexcept {
        return toCivil0( days ).month;
    }

    /// Return the 0-based day of the month (1st of month -> 0).
    /// This is the number of days since the 1st of the month.
    constexpr u16 civilDayOfMonth0( sysdays_t days ) noexcept {
        return toCivil0( days ).day;
    }

    /// Return the date corresponding to the first day of the year
    /// Eg, Dec 13 2025 -> Jan 1st, 2025
    constexpr sysdays_t civilBOY( sysdays_t days ) noexcept {
        return days - toCivilYearDOY( days ).doy;
    }

    /// Return the date corresponding to the last day of the year
    /// Eg, Dec 13 2025 -> Dec 31st, 2025
    constexpr sysdays_t civilEOY( sysdays_t days ) noexcept {
        auto _year_doy = toCivilYearDOY( days );
        // 364 for regular years, 365 for leap years
        auto lastDOY = 364 + int( isLeap( _year_doy.year ) );
        return days + lastDOY - _year_doy.doy;
    }

    /// Add months to the date. Eg, (Dec 13 2025) + 3 months becomes
    /// Mar 13 2026
    constexpr sysdays_t civilAddMonths( sysdays_t days, i32 months ) noexcept {
        auto ymd   = toCivil0( days );
        auto m0    = ymd.month;
        auto parts = math::divFloor2<12>( m0 + months );
        return resolveCivil0( ymd.year + parts.quot, parts.rem, ymd.day );
    }


    /// Add years to the date. Eg, (Dec 13 2025) + 3 years becomes
    /// Dec 13 2028
    constexpr sysdays_t civilAddYears( sysdays_t days, i32 years ) noexcept {
        auto ymd = toCivil0( days );
        return resolveCivil0( ymd.year + years, ymd.month, ymd.day );
    }

    /// Get the beginning of the month, as days since the epoch. Eg, Dec 13 2025
    /// -> Dec 1st 2025
    constexpr sysdays_t civilBOM( sysdays_t days ) noexcept {
        return days - civilDayOfMonth0( days );
    }

    constexpr sysdays_t civilEOM( sysdays_t days ) noexcept {
        auto ymd     = toCivil( days );
        auto lastDOM = lastDayOfMonth( ymd.year, ymd.month );
        // Add as many days as needed to get to the last day of the month
        return days + ( lastDOM - ymd.day );
    }

    /// Returns the weekday for the given date, expressed as a number of days
    /// since the epoch, where 0=Sun, 1=Mon, etc
    constexpr DOW dowFromDays( sysdays_t daysFromEpoch ) noexcept {
        return DOW( ( i64( daysFromEpoch ) + 0x80000002ull ) % 7 );
    }

    /// Given a date (eg, 2025 Oct 11, or 2025-10-11), get the weekday as a
    /// number 0-6 where 0=Sun

    constexpr DOW dowFromCivil( i32 year, u32 month, u32 dom ) noexcept {
        return dowFromDays( resolveCivil( year, month, dom ) );
    }


    /// Given a year, a month [1-12], and a day of the week, return the day
    /// of the last instance of the given weekday in that month.
    ///
    /// For example, to get the last Sunday in October in 2025, you would
    /// call: getLastDayOfWeekInMonth( 2025, 10, 0 )
    ///
    /// (because Sun=0)
    constexpr u32 getLastDOWInMonth( i32 year, u32 month, DOW dow ) noexcept {
        u32  last    = lastDayOfMonth( year, month );
        auto dowLast = dowFromCivil( year, month, last );
        u32  day     = last - ( dowLast - dow );
        return day;
    }


    /// Resolve a date such as '1966 Oct Sun>=10' - returns the first Sunday on
    /// or after October 10, 1966
    constexpr sysdays_t resolveDOW_GE(
        i32 year, u32 month, u32 dom, DOW dow ) noexcept {
        sysdays_t d = resolveCivil( year, month, dom );
        // Add as many days as needed to get to the desired weekday
        d += dow - dowFromDays( d );
        return d;
    }

    /// Resolve a date such as '2012 Apr Fri<=1' - returns the first Friday on
    /// or before April 1st, 2012
    constexpr sysdays_t resolveDOW_LE(
        i32 year, u32 month, u32 dom, DOW dow ) noexcept {
        sysdays_t d = resolveCivil( year, month, dom );
        // Subtract as many days as needed to get from the desired weekday
        d -= dowFromDays( d ) - dow;
        return d;
    }


    constexpr sysdays_t resolveLastDOW(
        i32 year, u32 month, DOW dow ) noexcept {
        // Last day of the month
        u32 lastDOM = lastDayOfMonth( year, month );
        // sysdays_t of that date
        sysdays_t d = resolveCivil( year, month, lastDOM );
        // Weekday of the last day of the month
        DOW dowEOM = dowFromDays( d );

        // Suppose the last day of the month is a Tuesday, but we want the last
        // _Sunday_ of the month. (Tuesday - Sunday) is 2, because it takes 2
        // days to go from Sunday to Tuesday.
        //
        // So, we return (date of last day of the month) - (2 days)
        i32 delta = i32( dowEOM - dow );

        return d - delta;
    }


    constexpr YMD getYMD_DOW_GE(
        i32 year, u32 month, u32 dom, DOW dow ) noexcept {
        u32 lastDOM = lastDayOfMonth( year, month );

        // Add as many days as needed to get to the desired weekday
        dom += dow - dowFromCivil( year, month, dom );

        if( dom > lastDOM )
        {
            month += 1;
            dom   -= lastDOM;
            if( month > 12 )
            {
                month -= 12;
                year  += 1;
            }
        }

        return YMD{ year, u16( month ), u16( dom ) };
    }

    constexpr YMD getYMD_DOW_LE(
        i32 year, u32 month, u32 dom, DOW dow ) noexcept {
        // Add as many days as needed to get to the desired weekday
        auto dom2 = i32( dom ) - i32( dowFromCivil( year, month, dom ) - dow );

        if( dom2 < 1 )
        {
            month -= 1;
            if( month == 0 )
            {
                month += 12;
                year  -= 1;
            }
            dom2 += lastDayOfMonth( year, month );
        }

        return YMD{ year, u16( month ), u16( dom2 ) };
    }

    constexpr sysseconds_t sysseconds( sysdays_t date, i32 offset ) {
        return i64( date ) * 86400 + offset;
    }

    static_assert(
        getYMD_DOW_LE( 2025, 10, 3, DOW::Sun ) == YMD{ 2025, 9, 28 } );

    static_assert(
        getYMD_DOW_LE( 2025, 1, 1, DOW::Sun ) == YMD{ 2024, 12, 29 } );
    static_assert(
        getYMD_DOW_LE( 2025, 4, 1, DOW::Sat ) == YMD{ 2025, 3, 29 } );
    static_assert(
        getYMD_DOW_LE( 2025, 4, 1, DOW::Sun ) == YMD{ 2025, 3, 30 } );
    static_assert(
        getYMD_DOW_LE( 2025, 4, 1, DOW::Mon ) == YMD{ 2025, 3, 31 } );


    // 2025 Oct Sat>=11 == 2025 Oct 11
    static_assert(
        getYMD_DOW_GE( 2025, 10, 11, DOW::Sat ) == YMD{ 2025, 10, 11 } );
    // 2025 Oct Sun>=11 == 2025 Oct 12
    static_assert(
        getYMD_DOW_GE( 2025, 10, 11, DOW::Sun ) == YMD{ 2025, 10, 12 } );
    // 2025 Oct Mon>=11 == 2025 Oct 13
    static_assert(
        getYMD_DOW_GE( 2025, 10, 11, DOW::Mon ) == YMD{ 2025, 10, 13 } );


    static_assert( dowFromCivil( 2025, 10, 11 ) == DOW::Sat );
    static_assert( getLastDOWInMonth( 2025, 10, DOW::Sun ) == 26 );
    static_assert( getLastDOWInMonth( 2025, 10, DOW::Mon ) == 27 );
    static_assert( getLastDOWInMonth( 2025, 10, DOW::Tue ) == 28 );
    static_assert( getLastDOWInMonth( 2025, 10, DOW::Wed ) == 29 );
    static_assert( getLastDOWInMonth( 2025, 10, DOW::Thu ) == 30 );
    static_assert( getLastDOWInMonth( 2025, 10, DOW::Fri ) == 31 );
    static_assert( getLastDOWInMonth( 2025, 10, DOW::Sat ) == 25 );

    /// Writes a timestamp as `YYYY-MM-DD HH:MM:SS`.
    ///
    /// Writes precisely 19 characters to the given buffer.
    constexpr void _write_timestamp(
        sysseconds_t T, char* p, char dateSep = '-', char dateTimeSep = ' ' ) {
        auto dateAndTime = math::divFloor2<86400>( T );

        auto date = sysdays_t( dateAndTime.quot );
        auto t    = u32( dateAndTime.rem );

        // Write the date
        auto ymd = toCivil( date );
        _write_yyyymmdd_10( ymd.year, ymd.month, ymd.day, p, dateSep );
        p[10] = dateTimeSep;
        _write_hhmmss_8( t, p + 11 );
    }

    /// Writes a timestamp as `YYYYMMDD HHMMSS`.
    ///
    /// Writes precisely 15 characters to the given buffer.
    constexpr void _write_timestamp_compact(
        sysseconds_t T, char* p, char dateTimeSep = ' ' ) {
        auto dateAndTime = math::divFloor2<86400>( T );

        auto date = sysdays_t( dateAndTime.quot );
        auto t    = u32( dateAndTime.rem );

        // Write the date
        auto ymd = toCivil( date );
        _write_yyyymmdd_8( ymd.year, ymd.month, ymd.day, p );
        p[8] = dateTimeSep;
        _write_hhmmss_6( t, p + 9 );
    }

    constexpr std::string_view writeTimestampToSV(
        sysseconds_t T, char* p, char dateSep = '-', char dateTimeSep = ' ' ) {
        _write_timestamp( T, p, dateSep, dateTimeSep );
        return std::string_view( p, 19 );
    }

    inline std::string utcToString(
        sysseconds_t sec, char dateSep = '-', char dateTimeSep = ' ' ) {
        char buff[20];
        _write_timestamp( sec, buff, dateSep, dateTimeSep );
        buff[19] = 'Z';
        return std::string( buff, 20 );
    }

    template<size_t N>
    inline std::string localToString(
        sysseconds_t sec, FromUTC off, FixStr<N> const& abbr ) {
        char buff[20 + N];
        _write_timestamp( off.toLocal( sec ), buff, '-', ' ' );
        buff[19] = ' ';
        _vtz_memcpy( buff + 20, abbr.buff_, N );
        return std::string( buff, 20 + abbr.size() );
    }

    struct DT {
        int64_t sec;

        string str() const {
            char dest[20]{};
            return string( writeTimestampToSV( sec, dest ) );
        }

        constexpr static DT civil(
            i32 y, u32 m, u32 d, int h, int min, int sec ) noexcept {
            return { resolveCivilTime( y, m, d, h, min, sec ) };
        }

        bool operator==( DT const& rhs ) const noexcept {
            return sec == rhs.sec;
        }
    };

    inline string format_as( DT T ) { return T.str(); }
} // namespace vtz
