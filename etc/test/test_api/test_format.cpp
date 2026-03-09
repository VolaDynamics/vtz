#include <chrono>
#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vtz/format.h>
#include <vtz/types.h>
#include <vtz/tz.h>

#include <date/date.h>

#include "guard_page_buffer.h"

#include "test_helper.h"

using namespace vtz;
using sv = std::string_view;

using namespace std::string_view_literals;

namespace {

    // Reference formatters using the Hinnant date library

    auto const& c_locale = std::locale::classic();

    template<class Dur>
    std::string ref_fmt( char const* fmt, vtz::sys_time<Dur> T ) {
        // On linux, we encounter a bug in `date` where doesn't correctly handle
        // years before 1000 CE in the expansion of `%c`
        //
        // Eg, if the input is 0001-01-01 00:00:00, date will format `%c` as
        // `Mon Jan  1 00:00:00 1`, when it should be formatted as `%a %b %e
        // %H:%M:%S %Y`, per the docs for strftime.
        //
        // '%Y' always has at least 4 digits, so using the correct format, this
        // would result in `Mon Jan  1 00:00:00 0001`
        //
        // See: https://man7.org/linux/man-pages/man3/strftime.3.html
        if( fmt == "%c"sv ) { fmt = "%a %b %e %H:%M:%S %Y"; }

        #if _WIN32
        // date::format() does not support '%r' on windows, so we expand it manually.
        //
        // from strftime(3):
        //
        //        %r     The time in a.m. or p.m. notation.  (SU) (The specific
        //               format used in the current locale can be obtained by
        //               calling nl_langinfo(3) with T_FMT_AMPM as an argument.)
        //               (In the POSIX locale this is equivalent to %I:%M:%S %p.)

        if( fmt == "%r"sv ) { fmt = "%I:%M:%S %p"; }
        #endif

        return date::format( c_locale, fmt, T );
    }

    std::string ref_time( const char* fmt, int64_t t ) {
        auto tp = date::sys_seconds{ std::chrono::seconds{ t } };
        return ref_fmt( fmt, tp );
    }

    std::string ref_date( const char* fmt, int32_t d ) {
        auto tp = date::sys_days{ date::days{ d } };
        return ref_fmt( fmt, tp );
    }

    // Reference formatter for sub-second precision. Constructs a sys_time
    // whose duration period matches the requested precision so that Hinnant
    // date outputs the correct number of fractional digits.

    template<int64_t Denom>
    std::string ref_precise( const char* fmt, int64_t t, int64_t frac ) {
        using D = std::chrono::duration<int64_t, std::ratio<1, Denom>>;
        auto tp = date::sys_time<D>{ std::chrono::duration_cast<D>(
                                         std::chrono::seconds{ t } )
                                     + D{ frac } };
        return date::format( c_locale, fmt, tp );
    }

    std::string ref_time(
        const char* fmt, int64_t t, u32 nanos, int precision ) {
        static constexpr int64_t pow10[] = { 1,
            10,
            100,
            1000,
            10000,
            100000,
            1000000,
            10000000,
            100000000,
            1000000000 };
        int64_t                  frac
            = ( precision == 0 ) ? 0 : int64_t( nanos ) / pow10[9 - precision];
        switch( precision )
        {
        case 0: return ref_precise<1>( fmt, t, 0 );
        case 1: return ref_precise<10>( fmt, t, frac );
        case 2: return ref_precise<100>( fmt, t, frac );
        case 3: return ref_precise<1000>( fmt, t, frac );
        case 4: return ref_precise<10000>( fmt, t, frac );
        case 5: return ref_precise<100000>( fmt, t, frac );
        case 6: return ref_precise<1000000>( fmt, t, frac );
        case 7: return ref_precise<10000000>( fmt, t, frac );
        case 8: return ref_precise<100000000>( fmt, t, frac );
        case 9: return ref_precise<1000000000>( fmt, t, frac );
        default:
            throw std::runtime_error( "ref_time(): precision must be 0..=9" );
        }
    }


    // Helpers that test all vtz function variants + guard-page truncation

    void check_format_time(
        const char* fmt, int64_t t, const std::string& expected ) {
        auto t_sys = sys_seconds( seconds( t ) );

        ASSERT_EQ( format_s( fmt, t ), expected ) << "fmt='" << fmt << "'";
        ASSERT_EQ( format( fmt, t_sys ), expected );

        // format_precise with precision=0 should match
        ASSERT_EQ( format_precise_s( fmt, t, 0, 0 ), expected );
        ASSERT_EQ( format_precise( fmt, t_sys, 0 ), expected );

        // format_to_s / format_to
        {
            char   buf[256];
            size_t n = format_to_s( fmt, t, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }
        {
            char   buf[256];
            size_t n = format_to( fmt, t_sys, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }

        // format_precise_to with precision=0
        {
            char   buf[256];
            size_t n = format_precise_to_s( fmt, t, 0, 0, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }
        {
            char   buf[256];
            size_t n = format_precise_to( fmt, t_sys, 0, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }

        // truncation with guard pages
        for( size_t sz = 0; sz <= expected.size(); sz++ )
        {
            auto prefix = sv( expected ).substr( 0, sz );
            auto gpb    = guard_page_buffer( sz );

            size_t n1 = format_to_s( fmt, t, gpb.data(), sz );
            ASSERT_EQ( n1, sz );
            ASSERT_EQ( sv( gpb.data(), n1 ), prefix );

            size_t n2 = format_to( fmt, t_sys, gpb.data(), sz );
            ASSERT_EQ( n2, sz );
            ASSERT_EQ( sv( gpb.data(), n2 ), prefix );

            size_t n3 = format_precise_to_s( fmt, t, 0, 0, gpb.data(), sz );
            ASSERT_EQ( n3, sz );
            ASSERT_EQ( sv( gpb.data(), n3 ), prefix );

            size_t n4 = format_precise_to( fmt, t_sys, 0, gpb.data(), sz );
            ASSERT_EQ( n4, sz );
            ASSERT_EQ( sv( gpb.data(), n4 ), prefix );
        }
    }

    void check_format_time(
        char const* fmt, sys_seconds t_sys, std::string const& expected ) {
        return check_format_time(
            fmt, t_sys.time_since_epoch().count(), expected );
    }

    void check_format_precise( const char* fmt,
        int64_t                            t,
        u32                                nanos,
        int                                precision,
        const std::string&                 expected ) {
        using sys_nanos = sys_time<nanoseconds>;
        auto tp         = sys_nanos{ seconds{ t } + nanoseconds{ nanos } };

        ASSERT_EQ( format_precise_s( fmt, t, nanos, precision ), expected );
        ASSERT_EQ( format_precise( fmt, tp, precision ), expected );

        // buffer variants
        {
            char   buf[256];
            size_t n = format_precise_to_s(
                fmt, t, nanos, precision, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }
        {
            char   buf[256];
            size_t n
                = format_precise_to( fmt, tp, precision, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }

        // truncation with guard pages
        for( size_t sz = 0; sz <= expected.size(); sz++ )
        {
            auto prefix = sv( expected ).substr( 0, sz );
            auto gpb    = guard_page_buffer( sz );

            size_t n1 = format_precise_to_s(
                fmt, t, nanos, precision, gpb.data(), sz );
            ASSERT_EQ( n1, sz );
            ASSERT_EQ( sv( gpb.data(), n1 ), prefix );

            size_t n2 = format_precise_to( fmt, tp, precision, gpb.data(), sz );
            ASSERT_EQ( n2, sz );
            ASSERT_EQ( sv( gpb.data(), n2 ), prefix );
        }
    }

    // sanity checks for detail::get_necessary_precision
    using namespace std::chrono;
    static_assert( detail::get_necessary_precision<hours>() == 0 );
    static_assert( detail::get_necessary_precision<minutes>() == 0 );
    static_assert( detail::get_necessary_precision<seconds>() == 0 );
    static_assert( detail::get_necessary_precision<milliseconds>() == 3 );
    static_assert( detail::get_necessary_precision<microseconds>() == 6 );
    static_assert( detail::get_necessary_precision<nanoseconds>() == 9 );
    using tenths = duration<int64_t, std::ratio<1, 10>>;
    static_assert( detail::get_necessary_precision<tenths>() == 1 );
    using thirds = duration<int64_t, std::ratio<1, 3>>;
    static_assert( detail::get_necessary_precision<thirds>() == 9 );
    using sevenths = duration<int64_t, std::ratio<1, 7>>;
    static_assert( detail::get_necessary_precision<sevenths>() == 9 );
    using seven_thirds = duration<int64_t, std::ratio<7, 3>>;
    static_assert( detail::get_necessary_precision<seven_thirds>() == 9 );

    // Helper for testing vtz::format / vtz::format_to with an arbitrary
    // duration type. Constructs a sys_time<Dur> by flooring a nanosecond
    // time point, then verifies that vtz::format produces the same output
    // as our ref_time at the auto-selected precision.

    template<class Dur>
    void check_format_generic( const char* fmt, int64_t t, u32 nanos ) {
        using sys_ns = sys_time<nanoseconds>;
        auto tp_ns   = sys_ns{ seconds{ t } + nanoseconds{ nanos } };
        auto tp      = sys_time<Dur>{ std::chrono::floor<Dur>(
            tp_ns.time_since_epoch() ) };

        constexpr int prec = detail::get_necessary_precision<Dur>();

        // decompose the same way vtz::format does internally
        auto sec = std::chrono::floor<seconds>( tp.time_since_epoch() );
        auto ns
            = std::chrono::floor<nanoseconds>( tp.time_since_epoch() - sec );

        auto expected = ref_time( fmt, sec.count(), u32( ns.count() ), prec );

        ASSERT_EQ( vtz::format( fmt, tp ), expected );

        // buffer variant
        {
            char   buf[256];
            size_t n = vtz::format_to( fmt, tp, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }

        // truncation with guard pages
        for( size_t sz = 0; sz <= expected.size(); sz++ )
        {
            auto prefix = sv( expected ).substr( 0, sz );
            auto gpb    = guard_page_buffer( sz );

            size_t n = vtz::format_to( fmt, tp, gpb.data(), sz );
            ASSERT_EQ( n, sz );
            ASSERT_EQ( sv( gpb.data(), n ), prefix );
        }
    }

    void check_format_date(
        const char* fmt, int32_t d, const std::string& expected ) {
        auto d_sys = sys_days( days( d ) );

        ASSERT_EQ( format_d( fmt, d ), expected );
        ASSERT_EQ( format( fmt, d_sys ), expected );

        // format_date_to_d
        {
            char   buf[256];
            size_t n = format_to_d( fmt, d, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }

        // format_date_to
        {
            char   buf[256];
            size_t n = format_to( fmt, d_sys, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }

        // truncation with guard pages
        for( size_t sz = 0; sz <= expected.size(); sz++ )
        {
            auto prefix = sv( expected ).substr( 0, sz );
            auto gpb    = guard_page_buffer( sz );

            size_t n1 = format_to_d( fmt, d, gpb.data(), sz );
            ASSERT_EQ( n1, sz );
            ASSERT_EQ( sv( gpb.data(), n1 ), prefix );

            size_t n2 = format_to( fmt, d_sys, gpb.data(), sz );
            ASSERT_EQ( n2, sz );
            ASSERT_EQ( sv( gpb.data(), n2 ), prefix );
        }
    }


    // Input values

    int64_t TIME_VALUES[] = {
        0,
        86399,        // 1970-01-01 23:59:59
        946684800,    // 2000-01-01
        1234567890,   // 2009-02-13 23:31:30
        1709294400,   // 2024-03-01 12:00:00 (leap year)
        1735689599,   // 2024-12-31 23:59:59 (leap year)
        1740000000,   // 2025-02-19 21:20:00
        1762442685,   // 2025-11-06 15:24:45
        2147483647,   // 2038-01-19 03:14:07 (i32 max)
        2147483648,   // 2038-01-19 03:14:08 (i32 max + 1)
        4102444800,   // 2100-01-01 (not a leap year)
        32503680000,  // 3000-01-01
        -62135596800, // 0001-01-01 (year 1)
        -62167219200, // 0000-01-01 (year 0)
    };

    int32_t DATE_VALUES[] = {
        0,       // 1970-01-01
        -1,      // 1969-12-31
        -365,    // 1969-01-01
        365,     // 1971-01-01
        10000,   // 1997-05-19
        18262,   // 2020-01-01
        19723,   // 2024-01-01
        20454,   // 2026-01-01
        -719162, // 0001-01-01 (year 1)
        -719528, // 0000-01-01 (year 0)
        -100000, // 1696-03-17
        100000,  // 2243-10-17
        730000,  // 3968-09-03
    };


    // Format specifiers
    // %z is excluded: vtz intentionally differs from Hinnant date by shortening
    // instances such as "+0500" to "+05"

    const char* TIME_FMTS[] = {
        "%Z", // timezone abbreviation, eg 'UTC'
        "%Y", // full year, e.g. "2025"
        "%y", // last 2 digits of year, e.g. "25"
        "%C", // century, e.g. "20"
        "%m", // month [01,12]
        "%d", // day of month, zero-padded [01,31]
        "%e", // day of month, space-padded [ 1,31]
        "%j", // day of year [001,366]
        "%w", // weekday, Sunday=0 [0,6]
        "%u", // weekday, Monday=1 ISO [1,7]
        "%a", // abbreviated weekday name
        "%A", // full weekday name
        "%b", // abbreviated month name
        "%h", // abbreviated month name (equivalent to %b)
        "%B", // full month name
        "%F", // ISO date, equivalent to %Y-%m-%d
        "%D", // equivalent to %m/%d/%y
        "%H", // hour, 24-hour clock [00,23]
        "%I", // hour, 12-hour clock [01,12]
        "%p", // AM/PM
        "%M", // minute [00,59]
        "%S", // second [00,60]
        "%R", // equivalent to %H:%M
        "%r", // equivalent to %I:%M:%S %p
        "%T", // equivalent to %H:%M:%S
        "%X", // preferred time representation (C locale: %H:%M:%S)
        "%c", // preferred date and time (C locale: %a %b %e %H:%M:%S %Y)
        "%Y-%m-%d %H:%M:%S",
        "%F %T",
        "%H:%M on %F",
        "%Y%m%d",
        "%d/%m/%Y",
        "%%",    // literal '%'
        "%n",    // newline
        "%t",    // tab
        "%%Y",   // literal '%' followed by 'Y'
        "hello", // no specifiers
    };

    const char* DATE_FMTS[] = {
        "%Y", // full year, e.g. "2025"
        "%y", // last 2 digits of year, e.g. "25"
        "%C", // century, e.g. "20"
        "%m", // month [01,12]
        "%d", // day of month, zero-padded [01,31]
        "%e", // day of month, space-padded [ 1,31]
        "%j", // day of year [001,366]
        "%w", // weekday, Sunday=0 [0,6]
        "%u", // weekday, Monday=1 ISO [1,7]
        "%a", // abbreviated weekday name
        "%A", // full weekday name
        "%b", // abbreviated month name
        "%h", // abbreviated month name (equivalent to %b)
        "%B", // full month name
        "%F", // ISO date, equivalent to %Y-%m-%d
        "%D", // equivalent to %m/%d/%y
        "%x", // preferred date representation (C locale: %m/%d/%y)
        "%Y-%m-%d",
        "%d/%m/%Y",
        "%Y%m%d",
        "%%",    // literal '%'
        "%n",    // newline
        "%t",    // tab
        "%%Y",   // literal '%' followed by 'Y'
        "hello", // no specifiers
    };

    // Time values safe for nanosecond-precision time points (|t| * 1e9
    // must fit in int64_t, so |t| < ~9.2e9). Large values from TIME_VALUES
    // are excluded to avoid overflow.
    int64_t PRECISE_TIME_VALUES[] = {
        0,
        86399,       // 1970-01-01 23:59:59
        946684800,   // 2000-01-01
        1234567890,  // 2009-02-13 23:31:30
        1709294400,  // 2024-03-01 12:00:00 (leap year)
        1735689599,  // 2024-12-31 23:59:59 (leap year)
        1740000000,  // 2025-02-19 21:20:00
        1762442685,  // 2025-11-06 15:24:45
        2147483647,  // 2038-01-19 03:14:07 (i32 max)
        2147483648,  // 2038-01-19 03:14:08 (i32 max + 1)
        4102444800,  // 2100-01-01
        9223372035,  // 2262-04-11 23:47:16 (max datetime representable with
                     // nanosecond timestamp, minus 1)
        -9223372036, // 1677-09-21 00:12:44 (min datetime representable with
                     // nanosecond timestamp)
    };

    u32 NANOS_VALUES[] = {
        0,
        123456789, // exercises all 9 fractional digits
        500000000, // half-second
        999999999, // maximum
    };

    // Formats containing %S or %T, which carry the fractional component
    const char* PRECISE_FMTS[] = {
        "%S",    // bare seconds
        "%T",    // HH:MM:SS
        "%F %T", // full datetime, fraction at end
        "%S %F", // fraction before other specifiers
        "%Y-%m-%d %H:%M:%S",
    };

} // namespace


TEST( vtz_format, format_time ) {
    // cross-product: every value x every format, verified against Hinnant date
    for( auto t : TIME_VALUES )
        for( auto fmt : TIME_FMTS )
            check_format_time( fmt, t, ref_time( fmt, t ) );

    // %z: vtz produces "+00", Hinnant produces "+0000"
    check_format_time( "%z", 0, "+00" );
    check_format_time( "%z", 1740000000, "+00" );
    check_format_time( "%F %T%z", 1740000000, "2025-02-19 21:20:00+00" );

    // %Z
    check_format_time( "%Z", 0, "UTC" );
    check_format_time( "%Z", 1740000000, "UTC" );
    check_format_time( "%F %T %Z", 1740000000, "2025-02-19 21:20:00 UTC" );

    // negative year: vtz produces "-1", Hinnant produces "-0001"
    check_format_time( "%Y-%m-%d", -62184499200, "-1-06-15" );
    check_format_time( "%Y", -62184499200, "-1" );

    // 5-digit year
    check_format_time( "%F %T", 253402300800, "10000-01-01 00:00:00" );
    check_format_time( "%Y", 253402300800, "10000" );
}


TEST( vtz_format, format_date ) {
    // cross-product: every value x every format, verified against Hinnant date
    for( auto d : DATE_VALUES )
        for( auto fmt : DATE_FMTS )
            check_format_date( fmt, d, ref_date( fmt, d ) );

    // negative year: vtz produces "-1", Hinnant produces "-0001"
    check_format_date( "%F", -719893, "-1-01-01" );
    check_format_date( "%Y", -719893, "-1" );

    // 5-digit year
    check_format_date( "%F", 2932897, "10000-01-01" );
    check_format_date( "%Y", 2932897, "10000" );
    check_format_date( "%F", 35804721, "99999-12-31" );
}


TEST( vtz_format, format_precise ) {
    // cross-product: every time value x nanos x precision x format
    for( auto t : PRECISE_TIME_VALUES )
        for( auto nanos : NANOS_VALUES )
            for( int prec = 0; prec <= 9; prec++ )
                for( auto fmt : PRECISE_FMTS )
                    check_format_precise(
                        fmt, t, nanos, prec, ref_time( fmt, t, nanos, prec ) );
}


TEST( vtz_format, format_generic ) {
    using namespace std::chrono;

    // non-decimal sub-second durations (precision defaults to 9)
    using thirds   = duration<int64_t, std::ratio<1, 3>>;
    using sevenths = duration<int64_t, std::ratio<1, 7>>;

    // longer than a second, non-integer number of seconds (precision 9)
    using seven_thirds = duration<int64_t, std::ratio<7, 3>>;

    for( auto t : PRECISE_TIME_VALUES )
        for( auto nanos : NANOS_VALUES )
            for( auto fmt : PRECISE_FMTS )
            {
                // common cases
                check_format_generic<seconds>( fmt, t, nanos );
                check_format_generic<milliseconds>( fmt, t, nanos );
                check_format_generic<microseconds>( fmt, t, nanos );
                check_format_generic<nanoseconds>( fmt, t, nanos );

                // coarser than seconds
                check_format_generic<minutes>( fmt, t, nanos );
                check_format_generic<hours>( fmt, t, nanos );

                // non-decimal sub-second
                check_format_generic<thirds>( fmt, t, nanos );
                check_format_generic<sevenths>( fmt, t, nanos );

                // pathological: longer than a second, non-integer
                check_format_generic<seven_thirds>( fmt, t, nanos );
            }
}


TEST( vtz_format, c_locale_sanity_test ) {
    // Part 1: Hardcoded sanity checks for %a, %A, %b, %B
    //
    // Weekday names — Jan 1 2024 is a Monday; test 7 consecutive days
    check_format_time( "%a", _get_time( 2024, 1, 1 ), "Mon" );
    check_format_time( "%a", _get_time( 2024, 1, 2 ), "Tue" );
    check_format_time( "%a", _get_time( 2024, 1, 3 ), "Wed" );
    check_format_time( "%a", _get_time( 2024, 1, 4 ), "Thu" );
    check_format_time( "%a", _get_time( 2024, 1, 5 ), "Fri" );
    check_format_time( "%a", _get_time( 2024, 1, 6 ), "Sat" );
    check_format_time( "%a", _get_time( 2024, 1, 7 ), "Sun" );

    check_format_time( "%A", _get_time( 2024, 1, 1 ), "Monday" );
    check_format_time( "%A", _get_time( 2024, 1, 2 ), "Tuesday" );
    check_format_time( "%A", _get_time( 2024, 1, 3 ), "Wednesday" );
    check_format_time( "%A", _get_time( 2024, 1, 4 ), "Thursday" );
    check_format_time( "%A", _get_time( 2024, 1, 5 ), "Friday" );
    check_format_time( "%A", _get_time( 2024, 1, 6 ), "Saturday" );
    check_format_time( "%A", _get_time( 2024, 1, 7 ), "Sunday" );

    // Month names — 1st of each month in 2024
    check_format_time( "%b", _get_time( 2024, 1, 1 ), "Jan" );
    check_format_time( "%b", _get_time( 2024, 2, 1 ), "Feb" );
    check_format_time( "%b", _get_time( 2024, 3, 1 ), "Mar" );
    check_format_time( "%b", _get_time( 2024, 4, 1 ), "Apr" );
    check_format_time( "%b", _get_time( 2024, 5, 1 ), "May" );
    check_format_time( "%b", _get_time( 2024, 6, 1 ), "Jun" );
    check_format_time( "%b", _get_time( 2024, 7, 1 ), "Jul" );
    check_format_time( "%b", _get_time( 2024, 8, 1 ), "Aug" );
    check_format_time( "%b", _get_time( 2024, 9, 1 ), "Sep" );
    check_format_time( "%b", _get_time( 2024, 10, 1 ), "Oct" );
    check_format_time( "%b", _get_time( 2024, 11, 1 ), "Nov" );
    check_format_time( "%b", _get_time( 2024, 12, 1 ), "Dec" );

    check_format_time( "%B", _get_time( 2024, 1, 1 ), "January" );
    check_format_time( "%B", _get_time( 2024, 2, 1 ), "February" );
    check_format_time( "%B", _get_time( 2024, 3, 1 ), "March" );
    check_format_time( "%B", _get_time( 2024, 4, 1 ), "April" );
    check_format_time( "%B", _get_time( 2024, 5, 1 ), "May" );
    check_format_time( "%B", _get_time( 2024, 6, 1 ), "June" );
    check_format_time( "%B", _get_time( 2024, 7, 1 ), "July" );
    check_format_time( "%B", _get_time( 2024, 8, 1 ), "August" );
    check_format_time( "%B", _get_time( 2024, 9, 1 ), "September" );
    check_format_time( "%B", _get_time( 2024, 10, 1 ), "October" );
    check_format_time( "%B", _get_time( 2024, 11, 1 ), "November" );
    check_format_time( "%B", _get_time( 2024, 12, 1 ), "December" );

    // Composite formats with a time component
    check_format_time( "%a, %d %b %Y %T",
        _get_time( 2024, 2, 29, 14, 30, 0 ),
        "Thu, 29 Feb 2024 14:30:00" );
    check_format_time( "%A %B %d, %Y %R",
        _get_time( 2024, 9, 1, 8, 5, 0 ),
        "Sunday September 01, 2024 08:05" );
    check_format_time(
        "%B %e, %Y", _get_time( 2024, 12, 25 ), "December 25, 2024" );

    // Part 2: Every day of 2024 (leap year, 366 days), checked against
    // date::format
    constexpr unsigned days_in_month[]
        = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    const char* fmts[] = { "%a",
        "%A",
        "%b",
        "%B",
        "%h",
        "%C",
        "%D",
        "%x",
        "%X",
        "%c",
        "%p",
        "%a %b %d %Y" };
    for( unsigned m = 1; m <= 12; m++ )
        for( unsigned d = 1; d <= days_in_month[m - 1]; d++ )
        {
            auto t = _get_time( 2024, m, d );
            for( auto fmt : fmts )
                check_format_time(
                    fmt, t, ref_time( fmt, t.time_since_epoch().count() ) );
        }
}


TEST( vtz_format, gnu_extensions ) {
    // %k — hour (24h) space-padded
    check_format_time( "%k", _get_time( 2024, 1, 1, 0, 0, 0 ), " 0" );
    check_format_time( "%k", _get_time( 2024, 1, 1, 5, 0, 0 ), " 5" );
    check_format_time( "%k", _get_time( 2024, 1, 1, 9, 0, 0 ), " 9" );
    check_format_time( "%k", _get_time( 2024, 1, 1, 10, 0, 0 ), "10" );
    check_format_time( "%k", _get_time( 2024, 1, 1, 13, 0, 0 ), "13" );
    check_format_time( "%k", _get_time( 2024, 1, 1, 23, 0, 0 ), "23" );

    // %l — hour (12h) space-padded
    check_format_time( "%l", _get_time( 2024, 1, 1, 0, 0, 0 ), "12" );
    check_format_time( "%l", _get_time( 2024, 1, 1, 1, 0, 0 ), " 1" );
    check_format_time( "%l", _get_time( 2024, 1, 1, 9, 0, 0 ), " 9" );
    check_format_time( "%l", _get_time( 2024, 1, 1, 10, 0, 0 ), "10" );
    check_format_time( "%l", _get_time( 2024, 1, 1, 12, 0, 0 ), "12" );
    check_format_time( "%l", _get_time( 2024, 1, 1, 13, 0, 0 ), " 1" );
    check_format_time( "%l", _get_time( 2024, 1, 1, 23, 0, 0 ), "11" );

    // %p — AM/PM
    check_format_time( "%p", _get_time( 2024, 1, 1, 0, 0, 0 ), "AM" );
    check_format_time( "%p", _get_time( 2024, 1, 1, 11, 59, 59 ), "AM" );
    check_format_time( "%p", _get_time( 2024, 1, 1, 12, 0, 0 ), "PM" );
    check_format_time( "%p", _get_time( 2024, 1, 1, 23, 59, 59 ), "PM" );

    // %P — am/pm (lowercase)
    check_format_time( "%P", _get_time( 2024, 1, 1, 0, 0, 0 ), "am" );
    check_format_time( "%P", _get_time( 2024, 1, 1, 11, 59, 59 ), "am" );
    check_format_time( "%P", _get_time( 2024, 1, 1, 12, 0, 0 ), "pm" );
    check_format_time( "%P", _get_time( 2024, 1, 1, 23, 59, 59 ), "pm" );

    // %r — 12-hour clock time with AM/PM, equivalent to %I:%M:%S %p
    check_format_time( "%r", _get_time( 2024, 1, 1, 0, 0, 0 ), "12:00:00 AM" );
    check_format_time( "%r", _get_time( 2024, 1, 1, 1, 30, 0 ), "01:30:00 AM" );
    check_format_time(
        "%r", _get_time( 2024, 1, 1, 11, 59, 59 ), "11:59:59 AM" );
    check_format_time( "%r", _get_time( 2024, 1, 1, 12, 0, 0 ), "12:00:00 PM" );
    check_format_time( "%r", _get_time( 2024, 1, 1, 13, 0, 0 ), "01:00:00 PM" );
    check_format_time(
        "%r", _get_time( 2024, 1, 1, 23, 59, 59 ), "11:59:59 PM" );

    // %s — seconds since epoch. Use raw epoch seconds as input so the
    // expected output is trivially the same number.
    check_format_time( "%s", 0, "0" );
    check_format_time( "%s", 1, "1" );
    check_format_time( "%s", 946684800, "946684800" );
    check_format_time( "%s", 1740000000, "1740000000" );
    check_format_time( "%s", -1, "-1" );
    check_format_time( "%s", -62167219200, "-62167219200" );
    check_format_time( "%s", 999999999999ll, "999999999999" );

    // Check '%s' with fractional component
    check_format_precise( "%s", 4000000000ll, 123456789, 3, "4000000000.123" );
    check_format_precise(
        "%s", 4000000000ll, 123456789, 6, "4000000000.123456" );
    check_format_precise(
        "%s", 4000000000ll, 123456789, 9, "4000000000.123456789" );

    // composite formats
    check_format_time(
        "%l:%M %P", _get_time( 2024, 7, 4, 15, 30, 0 ), " 3:30 pm" );
    check_format_time( "%k:%M on %F",
        _get_time( 2024, 12, 25, 8, 0, 0 ),
        " 8:00 on 2024-12-25" );
}


TEST( vtz_format, fractional_placement ) {
    // Verify that every compound specifier containing a seconds component
    // places the fractional digits immediately after the seconds digits,
    // not at the end of the compound expansion.
    //
    // Time: 2024-03-01 14:05:09 UTC  (a Friday)
    auto    t_sys = _get_time( 2024, 3, 1, 14, 5, 9 );
    int64_t ts    = t_sys.time_since_epoch().count();
    u32     nanos = 123456789;

    // precision = 3 -> ".123"
    check_format_precise( "%S", ts, nanos, 3, "09.123" );
    check_format_precise( "%T", ts, nanos, 3, "14:05:09.123" );
    check_format_precise( "%X", ts, nanos, 3, "14:05:09.123" );
    check_format_precise( "%r", ts, nanos, 3, "02:05:09.123 PM" );
    check_format_precise( "%c", ts, nanos, 3, "Fri Mar  1 14:05:09.123 2024" );
    check_format_precise( "%s", ts, nanos, 3, "1709301909.123" );

    // precision = 6 -> ".123456"
    check_format_precise( "%S", ts, nanos, 6, "09.123456" );
    check_format_precise( "%T", ts, nanos, 6, "14:05:09.123456" );
    check_format_precise( "%X", ts, nanos, 6, "14:05:09.123456" );
    check_format_precise( "%r", ts, nanos, 6, "02:05:09.123456 PM" );
    check_format_precise(
        "%c", ts, nanos, 6, "Fri Mar  1 14:05:09.123456 2024" );
    check_format_precise( "%s", ts, nanos, 6, "1709301909.123456" );

    // precision = 9 -> ".123456789"
    check_format_precise( "%S", ts, nanos, 9, "09.123456789" );
    check_format_precise( "%T", ts, nanos, 9, "14:05:09.123456789" );
    check_format_precise( "%X", ts, nanos, 9, "14:05:09.123456789" );
    check_format_precise( "%r", ts, nanos, 9, "02:05:09.123456789 PM" );
    check_format_precise(
        "%c", ts, nanos, 9, "Fri Mar  1 14:05:09.123456789 2024" );
    check_format_precise( "%s", ts, nanos, 9, "1709301909.123456789" );

    // Also verify AM (hour < 12):
    // Time: 2024-03-01 09:30:45 UTC
    auto    t_am  = _get_time( 2024, 3, 1, 9, 30, 45 );
    int64_t ts_am = t_am.time_since_epoch().count();
    check_format_precise( "%r", ts_am, nanos, 3, "09:30:45.123 AM" );
    check_format_precise( "%r", ts_am, nanos, 9, "09:30:45.123456789 AM" );

    // Check the generic sys_time<milliseconds> template path
    {
        using sys_ms = sys_time<milliseconds>;
        auto tp      = sys_ms{ seconds{ ts } + milliseconds{ 123 } };
        ASSERT_EQ( vtz::format( "%T", tp ), "14:05:09.123" );
        ASSERT_EQ( vtz::format( "%r", tp ), "02:05:09.123 PM" );
        ASSERT_EQ( vtz::format( "%c", tp ), "Fri Mar  1 14:05:09.123 2024" );
        ASSERT_EQ( vtz::format( "%s", tp ), std::to_string( ts ) + ".123" );
    }

    // Check the local_time<milliseconds> template path (unzoned)
    {
        using local_ms = local_time<milliseconds>;
        auto tp        = local_ms{ seconds{ ts } + milliseconds{ 123 } };
        ASSERT_EQ( vtz::format( "%T", tp ), "14:05:09.123" );
        ASSERT_EQ( vtz::format( "%r", tp ), "02:05:09.123 PM" );
        ASSERT_EQ( vtz::format( "%c", tp ), "Fri Mar  1 14:05:09.123 2024" );
        ASSERT_EQ( vtz::format( "%s", tp ), std::to_string( ts ) + ".123" );
    }
}


TEST( vtz_format, local_time_unzoned_tz_specifiers ) {
    auto    t   = _get_time( 2024, 3, 1, 14, 5, 9 );
    int64_t ts  = t.time_since_epoch().count();
    auto    sys = sys_seconds{ seconds{ ts } };

    // sys_time: %Z -> "UTC", %z -> "+00"
    ASSERT_EQ( vtz::format( "%Z", sys ), "UTC" );
    ASSERT_EQ( vtz::format( "%z", sys ), "+00" );
    ASSERT_EQ( vtz::format( "%F %T %Z", sys ), "2024-03-01 14:05:09 UTC" );
    ASSERT_EQ( vtz::format( "%F %T%z", sys ), "2024-03-01 14:05:09+00" );

    // local_time: %Z -> "-00", %z -> "-00"
    auto local = local_seconds{ seconds{ ts } };
    ASSERT_EQ( vtz::format( "%Z", local ), "-00" );
    ASSERT_EQ( vtz::format( "%z", local ), "-00" );
    ASSERT_EQ( vtz::format( "%F %T %Z", local ), "2024-03-01 14:05:09 -00" );
    ASSERT_EQ( vtz::format( "%F %T%z", local ), "2024-03-01 14:05:09-00" );
}


TEST( vtz_format, huge_times ) {
    ASSERT_EQ( vtz::format_s( "%F %T %Z", INT64_MIN ),
        "-292277022657-01-27 08:29:52 UTC" );
    ASSERT_EQ( vtz::format_s( "%F %T %Z", INT64_MAX ),
        "292277026596-12-04 15:30:07 UTC" );

    ASSERT_EQ(
        vtz::format_s( "%c", INT64_MIN ), "Sun Jan 27 08:29:52 -292277022657" );
    ASSERT_EQ(
        vtz::format_s( "%c", INT64_MAX ), "Sun Dec  4 15:30:07 292277026596" );

    // America/New_York is at UTC-05 in December, so we subtract 5 hours
    // relative to the UTC time
    auto ny = vtz::locate_zone( "America/New_York" );
    ASSERT_EQ( ny->format_s( "%c %Z", INT64_MAX ),
        "Sun Dec  4 10:30:07 292277026596 EST" );

    // The year -292277022657 is before the introduction of standard time.
    // So, we use the initial zone stdoff, which is UTC+001730
    // - it's 17 minutes and 30 seconds ahead of UTC time
    auto ams = vtz::locate_zone( "Europe/Amsterdam" );
    ASSERT_EQ( ams->format_s( "%c %Z", INT64_MIN ),
        "Sun Jan 27 08:47:22 -292277022657 LMT" );
}
