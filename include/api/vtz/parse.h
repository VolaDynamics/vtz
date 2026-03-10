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

    VTZ_EXPORT sys_days_t parse_d( string_view fmt, string_view date_str );


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

    VTZ_EXPORT sec_t parse_s( string_view fmt, string_view time_str );


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

    VTZ_EXPORT nanos_t parse_ns( string_view fmt, string_view time_str );


    /// Holds (seconds, nanoseconds) as a pair when both components are needed.
    ///
    /// This allows for parsing dates and times outside of the range supported
    /// by an int64_t number of nanoseconds since the epoch.

    struct parse_precise_result {
        /// Seconds since the epoch
        sys_seconds_t seconds;
        /// Nanoseconds component, range [0,999999999]
        u32 nanos;

        bool operator==( parse_precise_result const& rhs ) const noexcept {
            return seconds == rhs.seconds && nanos == rhs.nanos;
        }
        bool operator!=( parse_precise_result const& rhs ) const noexcept {
            return !( *this == rhs );
        }
    };


    /// Parse the given input as a datetime, with the time returned as an
    /// integral number of seconds and nanoseconds since the Unix Epoch.
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
    /// @return a `parse_precise_result` holding seconds and nanoseconds since
    ///         the epoch
    /// @throws if the given format specifier is invalid

    VTZ_EXPORT parse_precise_result parse_precise(
        string_view fmt, string_view time_str );


    /// Parse the given input as a datetime, with the time returned as a
    /// sys_time<Dur>
    ///
    /// Decimal digits are permitted after the number of seconds. For example,
    /// if the format string is `"%Y-%m-%d %H:%M:%S"` then `"2025-02-19
    /// 16:19:27.12878"` is interpreted as "2025-02-19 16:19:27, and 128780000
    /// nanoseconds".
    ///
    /// In the event that the time given is _more_ precise than what can be
    /// represented by the given `sys_time<Dur>`, the floor of the result is
    /// taken. So, for example, if the result has a precision of seconds, then
    /// `"2026-02-19 19:00:13.9999"` would be rounded down to `2026-02-19
    /// 19:00:13`.
    ///
    /// Similarly, if the result has a precision of days, then the result is
    /// rounded down to `2026-02-19`.
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/parse.html
    ///
    /// @param fmt format specifier describes layout of a date (eg,
    /// `"%Y-%m-%d %H:%M:%S"`)
    /// @param time_str string describing the time, eg `"2026-02-19
    /// 16:19:27.12878"`
    /// @return a sys_time<Dur> representing the parsed datetime
    /// @throws if the given format specifier is invalid

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
                        return _tp{ days( parse_d( fmt, time_str ) ) };
                    }
                    else
                    {
                        // result is multiple of a day
                        return _tp{ floor<Dur>(
                            days( parse_d( fmt, time_str ) ) ) };
                    }
                }
                else
                {
                    if constexpr( n == 1 )
                    {
                        // result is exactly a second
                        return _tp{ seconds( parse_s( fmt, time_str ) ) };
                    }
                    else
                    {
                        // result is a multiple of a second
                        return _tp{ floor<Dur>(
                            seconds( parse_s( fmt, time_str ) ) ) };
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
                        nanoseconds( parse_ns( fmt, time_str ) ) ) };
                }
            }
        }
        else
        {
            // rep is floating point

            auto parts = parse_precise( fmt, time_str );

            constexpr rep ns_div = 1000000000 / rep( d );

            rep result = rep( parts.seconds ) * rep( d ) // seconds -> 1/d
                         + rep( parts.nanos ) / ns_div   // nanos -> 1/d
                ;
            if constexpr( n == 1 )
            {
                // Duration is in terms of 1/d seconds
                return _tp{ Dur( result ) };
            }
            else
            {
                // Divide out the numerator
                return _tp{ Dur( result / rep( n ) ) };
            }
        }
    }


    /// Parse the given input as a local time. This is equivalent to parsing a
    /// sys_time, but the returned type represents a local time rather than a
    /// sys_time.

    template<class Dur>
    local_time<Dur> parse_local( string_view fmt, string_view time_str ) {
        return local_time<Dur>{
            parse<Dur>( fmt, time_str ).time_since_epoch()
        };
    }
} // namespace vtz
