#pragma once

#include <string>
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
    using std::string;
    /// Represents a date as a number of days from the epoch
    ///
    /// sysdays_t(0) is 1970-01-01, sysdays_t(1) is 1970-01-02, etc
    using sysdays_t = i32;
    /// Seconds from epoch
    using sysseconds_t = i64;

    constexpr inline sysdays_t MAX_DAYS = INT32_MAX;

    constexpr sysseconds_t daysToSeconds( sysdays_t days ) noexcept {
        return i64( days ) * 86400;
    }

    /// Write an offset (in seconds) as [+/-]HH[:MM[:SS]], writing the shortest
    /// amount necessary.
    ///
    /// Examples:
    /// - `writeShortestOffset(0, ...) -> "+00"`
    /// - `writeShortestOffset(7200, ...) -> "+02"`
    /// - `writeShortestOffset(7260, ...) -> "+0201"`
    /// - `writeShortestOffset(7201, ...) -> "+0200:01"`
    /// - `writeShortestOffset(7261, ...) -> "+0201:01"`
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

    struct YMD {
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

        i32 era = ( y >= 0 ? y : y - 399 ) / 400;
        u32 yoe = static_cast<u32>( y - era * 400 );               // [0, 399]
        u32 doy
            = ( 153 * ( m > 2 ? m - 3 : m + 9 ) + 2 ) / 5 + d - 1; // [0, 365]
        u32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; // [0, 146096]
        return era * 146097 + static_cast<i32>( doe ) - 719468;
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
        days          += 719468;
        const i64 era  = ( days >= 0 ? days : days - 146096 ) / 146097;
        const u32 doe  = u32( days - era * 146097 ); // [0, 146096]
        const u32 yoe  = ( doe - doe / 1460 + doe / 36524 - doe / 146096 )
                        / 365;                       // [0, 399]
        const i64 y   = i64( yoe ) + era * 400;
        const u32 doy = doe - ( 365 * yoe + yoe / 4 - yoe / 100 ); // [0, 365]
        const u32 mp  = ( 5 * doy + 2 ) / 153;                     // [0, 11]
        const u32 d   = doy - ( 153 * mp + 2 ) / 5 + 1;            // [1, 31]
        const u32 m   = mp < 10 ? mp + 3 : mp - 9;                 // [1, 12]
        return YMD{ i32( y + ( m <= 2 ) ), u16( m ), u16( d ) };
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
    constexpr void writeTimestamp(
        sysseconds_t T, char* p, char dateSep = '-', char dateTimeSep = ' ' ) {
        auto dateAndTime = math::divFloor2<86400>( T );

        auto date = sysdays_t( dateAndTime.quot );
        auto t    = u32( dateAndTime.rem );

        // Write the date
        toCivil( date ).write( p, dateSep );

        int h  = t / 3600;
        t     %= 3600;
        int m  = t / 60;
        t     %= 60;
        int s  = t;
        p[10]  = dateTimeSep;
        p[11]  = '0' + h / 10;
        p[12]  = '0' + h % 10;
        p[13]  = ':';
        p[14]  = '0' + m / 10;
        p[15]  = '0' + m % 10;
        p[16]  = ':';
        p[17]  = '0' + s / 10;
        p[18]  = '0' + s % 10;
    }

    constexpr std::string_view writeTimestampToSV(
        sysseconds_t T, char* p, char dateSep = '-', char dateTimeSep = ' ' ) {
        writeTimestamp( T, p, dateSep, dateTimeSep );
        return std::string_view( p, 19 );
    }

    inline std::string utcToString(
        sysseconds_t sec, char dateSep = '-', char dateTimeSep = ' ' ) {
        char buff[20];
        writeTimestamp( sec, buff, dateSep, dateTimeSep );
        buff[19] = 'Z';
        return std::string( buff, 20 );
    }

    template<size_t N>
    inline std::string localToString(
        sysseconds_t sec, FromUTC off, FixStr<N> const& abbr ) {
        char buff[20 + N];
        writeTimestamp( off.toLocal( sec ), buff, '-', ' ' );
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
