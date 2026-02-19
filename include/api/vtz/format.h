#pragma once


#include <string>
#include <cstddef>
#include <string_view>

#include <vtz/impl/export.h>
#include <vtz/types.h>
#include <vtz/impl/chrono_types.h>


namespace vtz {
    using std::string_view;


    /// Formats a date to a string, measured as days since the unix epoch.
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing date, eg "%Y-%m-%d"
    /// @param days days since 1970-01-01 (the Unix Epoch)
    /// @throws if the given format specifier is invalid

    VTZ_EXPORT std::string format_date_d( string_view fmt, sysdays_t days );


    /// Formats a date to a string, with date given as a
    /// `std::chrono::sys_days`.
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing date, eg "%Y-%m-%d"
    /// @param days days since 1970-01-01 (the Unix Epoch)
    /// @throws if the given format specifier is invalid

    inline std::string format_date( string_view fmt, sys_days days ) {
        return format_date_d( fmt, days.time_since_epoch().count() );
    }


    /// Formats a date to the given buffer, with date given as as days
    /// since the unix epoch. Output is truncated if it would exceed `count`.
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing date, eg "%Y-%m-%d"
    /// @param days days since 1970-01-01 (the Unix Epoch)
    /// @return number of characters written to the buffer.
    /// @throws if the given format specifier is invalid

    VTZ_EXPORT size_t format_date_to_d(
        string_view format, sysdays_t days, char* buff, size_t count );


    /// Formats a date to the given buffer, with date given as a
    /// `std::chrono::sys_days`. Output is truncated if it would exceed `count`.
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing date, eg "%Y-%m-%d"
    /// @param days days since 1970-01-01 (the Unix Epoch)
    /// @return number of characters written to the buffer.
    /// @throws if the given format specifier is invalid

    inline size_t format_date_to(
        string_view fmt, sys_days days, char* buff, size_t count ) {
        return format_date_to_d(
            fmt, days.time_since_epoch().count(), buff, count );
    }


    /// Formats a UTC time to a string, with time given as seconds since the
    /// Unix Epoch.
    ///
    /// The time is interpreted as UTC. `%Z` will produce "UTC", and `%z`
    /// will produce "+00".
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing time, eg "%Y-%m-%d %H:%M:%S"
    /// @param t seconds since 1970-01-01 00:00:00 UTC
    /// @throws if the given format specifier is invalid

    VTZ_EXPORT std::string format_time_s( string_view fmt, sysseconds_t t );


    /// Formats a UTC time to a string, with time given as a
    /// `std::chrono::sys_seconds`.
    ///
    /// The time is interpreted as UTC. `%Z` will produce "UTC", and `%z`
    /// will produce "+00".
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing time, eg "%Y-%m-%d %H:%M:%S"
    /// @param t time since 1970-01-01 00:00:00 UTC
    /// @throws if the given format specifier is invalid

    inline std::string format_time( string_view fmt, sys_seconds t ) {
        return format_time_s( fmt, t.time_since_epoch().count() );
    }


    /// Formats a UTC time to the given buffer, with time given as seconds
    /// since the Unix Epoch. Output is truncated if it would exceed `count`.
    ///
    /// The time is interpreted as UTC. `%Z` will produce "UTC", and `%z`
    /// will produce "+00".
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing time, eg "%Y-%m-%d %H:%M:%S"
    /// @param t seconds since 1970-01-01 00:00:00 UTC
    /// @return number of characters written to the buffer.
    /// @throws if the given format specifier is invalid

    VTZ_EXPORT size_t format_time_to_s(
        string_view fmt, sysseconds_t t, char* buff, size_t count );


    /// Formats a UTC time to the given buffer, with time given as a
    /// `std::chrono::sys_seconds`. Output is truncated if it would exceed
    /// `count`.
    ///
    /// The time is interpreted as UTC. `%Z` will produce "UTC", and `%z`
    /// will produce "+00".
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing time, eg "%Y-%m-%d %H:%M:%S"
    /// @param t time since 1970-01-01 00:00:00 UTC
    /// @return number of characters written to the buffer.
    /// @throws if the given format specifier is invalid

    inline size_t format_time_to(
        string_view fmt, sys_seconds t, char* buff, size_t count ) {
        return format_time_to_s(
            fmt, t.time_since_epoch().count(), buff, count );
    }
} // namespace vtz
