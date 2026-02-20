#include <gtest/gtest.h>
#include <string>
#include <string_view>
#include <vtz/format.h>

#include <date/date.h>

#include "guard_page_buffer.h"

using namespace vtz;
using sv = std::string_view;


namespace {

    // Reference formatters using the Hinnant date library

    std::string ref_time( const char* fmt, int64_t t ) {
        auto tp = date::sys_seconds{ std::chrono::seconds{ t } };
        return date::format( fmt, tp );
    }

    std::string ref_date( const char* fmt, int32_t d ) {
        auto tp = date::sys_days{ date::days{ d } };
        return date::format( fmt, tp );
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
        return date::format( fmt, tp );
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
        default: __builtin_unreachable();
        }
    }


    // Helpers that test all vtz function variants + guard-page truncation

    void check_format_time(
        const char* fmt, int64_t t, const std::string& expected ) {
        auto t_sys = sys_seconds( seconds( t ) );

        ASSERT_EQ( format_time_s( fmt, t ), expected );
        ASSERT_EQ( format_time( fmt, t_sys ), expected );

        // format_precise with precision=0 should match
        ASSERT_EQ( format_precise_s( fmt, t, 0, 0 ), expected );
        ASSERT_EQ( format_precise( fmt, t_sys, 0 ), expected );

        // format_time_to_s / format_time_to
        {
            char   buf[256];
            size_t n = format_time_to_s( fmt, t, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }
        {
            char   buf[256];
            size_t n = format_time_to( fmt, t_sys, buf, sizeof( buf ) );
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

            size_t n1 = format_time_to_s( fmt, t, gpb.data(), sz );
            ASSERT_EQ( n1, sz );
            ASSERT_EQ( sv( gpb.data(), n1 ), prefix );

            size_t n2 = format_time_to( fmt, t_sys, gpb.data(), sz );
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

    void check_format_date(
        const char* fmt, int32_t d, const std::string& expected ) {
        auto d_sys = sys_days( days( d ) );

        ASSERT_EQ( format_date_d( fmt, d ), expected );
        ASSERT_EQ( format_date( fmt, d_sys ), expected );

        // format_date_to_d
        {
            char   buf[256];
            size_t n = format_date_to_d( fmt, d, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }

        // format_date_to
        {
            char   buf[256];
            size_t n = format_date_to( fmt, d_sys, buf, sizeof( buf ) );
            ASSERT_EQ( sv( buf, n ), expected );
        }

        // truncation with guard pages
        for( size_t sz = 0; sz <= expected.size(); sz++ )
        {
            auto prefix = sv( expected ).substr( 0, sz );
            auto gpb    = guard_page_buffer( sz );

            size_t n1 = format_date_to_d( fmt, d, gpb.data(), sz );
            ASSERT_EQ( n1, sz );
            ASSERT_EQ( sv( gpb.data(), n1 ), prefix );

            size_t n2 = format_date_to( fmt, d_sys, gpb.data(), sz );
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
        "%m", // month [01,12]
        "%d", // day of month, zero-padded [01,31]
        "%e", // day of month, space-padded [ 1,31]
        "%j", // day of year [001,366]
        "%w", // weekday, Sunday=0 [0,6]
        "%u", // weekday, Monday=1 ISO [1,7]
        "%F", // ISO date, equivalent to %Y-%m-%d
        "%H", // hour, 24-hour clock [00,23]
        "%I", // hour, 12-hour clock [01,12]
        "%M", // minute [00,59]
        "%S", // second [00,60]
        "%R", // equivalent to %H:%M
        "%T", // equivalent to %H:%M:%S
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
        "%m", // month [01,12]
        "%d", // day of month, zero-padded [01,31]
        "%e", // day of month, space-padded [ 1,31]
        "%j", // day of year [001,366]
        "%w", // weekday, Sunday=0 [0,6]
        "%u", // weekday, Monday=1 ISO [1,7]
        "%F", // ISO date, equivalent to %Y-%m-%d
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
