#pragma once


#include <cstddef>
#include <string_view>

#include <vtz/impl/chrono_types.h>
#include <vtz/impl/export.h>
#include <vtz/types.h>


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
} // namespace vtz
