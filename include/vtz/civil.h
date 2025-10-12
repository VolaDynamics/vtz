#pragma once

#include <vtz/date_types.h>


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
    };


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
    constexpr i32 daysFromCivil( i32 y, u32 m, u32 d ) noexcept {
        y -= m <= 2;

        i32 era = ( y >= 0 ? y : y - 399 ) / 400;
        u32 yoe = static_cast<u32>( y - era * 400 );               // [0, 399]
        u32 doy
            = ( 153 * ( m > 2 ? m - 3 : m + 9 ) + 2 ) / 5 + d - 1; // [0, 365]
        u32 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy; // [0, 146096]
        return era * 146097 + static_cast<i32>( doe ) - 719468;
    }

    /// Returns the weekday for the given date, expressed as a number of days
    /// since the epoch, where 0=Sun, 1=Mon, etc
    constexpr DOW dowFromDays( i32 daysFromEpoch ) noexcept {
        return DOW( ( int64_t( daysFromEpoch ) + 0x80000002ull ) % 7 );
    }

    /// Given a date (eg, 2025 Oct 11, or 2025-10-11), get the weekday as a
    /// number 0-6 where 0=Sun

    constexpr DOW dowFromCivil( i32 year, u32 month, u32 dom ) noexcept {
        return dowFromDays( daysFromCivil( year, month, dom ) );
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
} // namespace vtz
