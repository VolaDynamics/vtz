#pragma once


#include <cstddef>
#include <string_view>

#include <type_traits>
#include <vtz/impl/chrono_types.h>
#include <vtz/impl/export.h>
#include <vtz/types.h>

#include <vtz/impl/tz_impl.h>

namespace vtz {
    using std::string_view;


    /// Parse the given input as a date, with the date returned as an integral
    /// number of days since the Unix Epoch.
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/parse.html
    ///
    /// @param fmt format specifier describes layout of a date (eg,
    /// `"%Y-%m-%d"`)
    /// @param date_str string describing date, eg `"2026-02-19"`
    /// @return the number of days since the epoch, as an int
    /// @throws if the given format specifier is invalid

    VTZ_EXPORT sysdays_t parse_date_d( string_view fmt, string_view date_str );


    /// Parse the given input as a date, with the date returned as a
    /// `std::chrono::sys_days`
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/parse.html
    ///
    /// @param fmt format specifier describes layout of a date (eg,
    /// `"%Y-%m-%d"`)
    /// @param date_str string describing date, eg `"2026-02-19"`
    /// @return a `std::chrono::sys_days` value representing the date
    /// @throws if the given format specifier is invalid

    inline sys_days parse_sys_days( string_view fmt, string_view date_str ) {
        return sys_days( days( parse_date_d( fmt, date_str ) ) );
    }


    /// Parse the given input as a datetime, with the time returned as an
    /// integral number of seconds since the Unix Epoch.
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/parse.html
    ///
    /// @param fmt format specifier describes layout of a date (eg,
    /// `"%Y-%m-%d %H:%M:%S"`)
    /// @param time_str string describing time, eg `"2026-02-19 16:19:27"`
    /// @return the number of seconds since the epoch, as a 64-bit int
    /// @throws if the given format specifier is invalid

    VTZ_EXPORT sec_t parse_time_s( string_view fmt, string_view time_str );


    /// Parse the given input as a datetime, with the time returned as a
    /// `std::chrono::sys_seconds`
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/parse.html
    ///
    /// @param fmt format specifier describes layout of a date (eg,
    /// `"%Y-%m-%d %H:%M:%S"`)
    /// @param time_str string describing the time, eg `"2026-02-19 16:19:27"`
    /// @return a `std::chrono::sys_seconds` value representing the time
    /// @throws if the given format specifier is invalid

    inline sys_seconds parse_sys_seconds(
        string_view fmt, string_view time_str ) {
        return sys_seconds( seconds( parse_time_s( fmt, time_str ) ) );
    }


    /// Parse the given input as a datetime, with the time returned as an
    /// integral number of nanoseconds since the Unix Epoch.
    ///
    /// Decimal digits are permitted after the number of seconds. For example,
    /// if the format string is `"%Y-%m-%d %H:%M:%S"` then `"2025-02-19
    /// 16:19:27.12878"` is interpreted as "2025-02-19 16:19:27, and 128780000
    /// nanoseconds".
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/parse.html
    ///
    /// @param fmt format specifier describes layout of a date (eg,
    /// `"%Y-%m-%d %H:%M:%S"`)
    /// @param time_str string describing the time, eg `"2026-02-19
    /// 16:19:27.12878"`
    /// @return the number of nanoseconds since the epoch, as a 64-bit int
    /// @throws if the given format specifier is invalid

    VTZ_EXPORT nanos_t parse_time_ns( string_view fmt, string_view time_str );


    /// Parse the given input as a datetime, with the time returned as an
    /// integral number of nanoseconds since the Unix Epoch.
    ///
    /// Decimal digits are permitted after the number of seconds. For example,
    /// if the format string is `"%Y-%m-%d %H:%M:%S"` then `"2025-02-19
    /// 16:19:27.12878"` is interpreted as "2025-02-19 16:19:27, and 128780000
    /// nanoseconds".
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/parse.html
    ///
    /// @param fmt format specifier describes layout of a date (eg,
    /// `"%Y-%m-%d %H:%M:%S"`)
    /// @param time_str string describing the time, eg `"2026-02-19
    /// 16:19:27.12878"`
    /// @return a `std::chrono::sys_time<nanoseconds>` value representing the
    /// time
    /// @throws if the given format specifier is invalid

    inline sys_time<nanoseconds> parse_sys_nanoseconds(
        string_view fmt, string_view time_str ) {
        return sys_time<nanoseconds>(
            nanoseconds( parse_time_ns( fmt, time_str ) ) );
    }


    struct parse_precise_result {
        sysseconds_t seconds;
        u32          nanos;
    };

    VTZ_EXPORT parse_precise_result parse_precise(
        string_view fmt, string_view time_str );

    template<class Dur>
    sys_time<Dur> parse( string_view fmt, string_view time_str ) {
        using rep        = typename Dur::rep;
        using period     = typename Dur::period;
        constexpr auto n = period::num;
        constexpr auto d = period::den;

        using _tp = sys_time<Dur>;

        using std::chrono::floor;
        if constexpr( !std::is_floating_point<rep>() )
        {
            if constexpr( d == 1 )
            {
                if constexpr( n % 86400 == 0 ) // result is a multiple of a day
                {
                    if constexpr( n == 86400 )
                    {
                        // result is exactly a day
                        return _tp{ days( parse_date_d( fmt, time_str ) ) };
                    }
                    else
                    {
                        // result is multiple of a day
                        return _tp{ floor<Dur>(
                            days( parse_date_d( fmt, time_str ) ) ) };
                    }
                }
                else
                {
                    if constexpr( n == 1 )
                    {
                        // result is exactly a second
                        return _tp{ seconds( parse_time_s( fmt, time_str ) ) };
                    }
                    else
                    {
                        // result is a multiple of a second
                        return _tp{ floor<Dur>(
                            seconds( parse_time_s( fmt, time_str ) ) ) };
                    }
                }
            }
            else
            {
                if constexpr( 1000000000 % d == 0 )
                {
                    // result can be expressed as a whole number of nanoseconds
                    // So, we put the result in terms of (1/d)

                    auto parts = parse_precise( fmt, time_str );

                    constexpr u32 nanos_div = u32( 1000000000u / d );
                    // If a whole number of nanoseconds fit into the
                    // denominator, we want to put both seconds and nanoseconds
                    // in terms of (1/den)
                    int64_t t = parts.seconds * d         // seconds -> 1/d
                                + parts.nanos / nanos_div // nanos -> 1/d
                        ;
                    if constexpr( n == 1 )
                    {
                        // Duration is in terms of 1/d seconds, which is t
                        return _tp{ Dur( t ) };
                    }
                    else
                    {
                        // Duration is in terms of n/d, so we need to divide out
                        // n
                        return _tp{ Dur( math::div_floor<n>( t ) ) };
                    }
                }
                else
                {
                    // result can NOT be expressed as a whole number of
                    // nanoseconds (or a whole number of seconds, for that
                    // matter)
                    //
                    // Best thing to do is (1) convert to nanoseconds

                    return _tp{ floor<Dur>(
                        nanoseconds( parse_time_ns( fmt, time_str ) ) ) };
                }
            }
        }
        else
        {
            // rep is floating point

            auto parts = parse_precise( fmt, time_str );

            constexpr rep ns_div = 1000000000 / rep( d );
            // If a whole number of nanoseconds fit into the
            // denominator, we want to put both seconds and nanoseconds
            // in terms of (1/den)
            rep result = rep( parts.seconds ) * rep( d ) // seconds -> 1/d
                         + rep( parts.nanos ) / ns_div   // nanos -> 1/d
                ;
            if constexpr( n == 1 )
            {
                // Duration is in terms of 1/d seconds, which is t
                return _tp{ Dur( result ) };
            }
            else
            {
                // Divide out the numerator
                return _tp{ Dur( result / rep( n ) ) };
            }
        }
    }
} // namespace vtz
