#pragma once


#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>

#include <vtz/impl/chrono_types.h>
#include <vtz/impl/export.h>
#include <vtz/impl/tz_impl.h>
#include <vtz/types.h>


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

    VTZ_EXPORT std::string format_d( string_view fmt, sysdays_t days );


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

    VTZ_EXPORT size_t format_to_d(
        string_view format, sysdays_t days, char* buff, size_t count );


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

    VTZ_EXPORT std::string format_s( string_view fmt, sysseconds_t t );


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

    VTZ_EXPORT size_t format_to_s(
        string_view fmt, sysseconds_t t, char* buff, size_t count );


    /// Formats a UTC time with sub-second precision to a string.
    ///
    /// The time is interpreted as UTC. `%Z` will produce "UTC", and `%z`
    /// will produce "+00".
    ///
    /// The fractional component is placed immediately after the seconds
    /// component. If precision is 0, no fractional component is written.
    /// If precision is less than 9, the nanoseconds value is truncated.
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing time, eg "%Y-%m-%d %H:%M:%S"
    /// @param t seconds since 1970-01-01 00:00:00 UTC
    /// @param nanos nanosecond component, in the range `[0, 999999999]`
    /// @param precision number of fractional digits to write (0-9)
    /// @throws if the given format specifier is invalid

    VTZ_EXPORT std::string format_precise_s(
        string_view fmt, sysseconds_t t, u32 nanos, int precision );


    /// Formats a UTC time with sub-second precision to a string, with time
    /// given as a `std::chrono::sys_time`.
    ///
    /// The time is interpreted as UTC. `%Z` will produce "UTC", and `%z`
    /// will produce "+00".
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing time, eg "%Y-%m-%d %H:%M:%S"
    /// @param t time since 1970-01-01 00:00:00 UTC
    /// @param precision number of fractional digits to write (0-9)
    /// @throws if the given format specifier is invalid

    template<class Dur>
    std::string format_precise(
        string_view fmt, sys_time<Dur> t, int precision ) {
        auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
        auto nanos
            = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
        return format_precise_s(
            fmt, sec.count(), u32( nanos.count() ), precision );
    }


    /// Formats a UTC time with sub-second precision to the given buffer.
    /// Output is truncated if it would exceed `count`.
    ///
    /// The time is interpreted as UTC. `%Z` will produce "UTC", and `%z`
    /// will produce "+00".
    ///
    /// The fractional component is placed immediately after the seconds
    /// component. If precision is 0, no fractional component is written.
    /// If precision is less than 9, the nanoseconds value is truncated.
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing time, eg "%Y-%m-%d %H:%M:%S"
    /// @param t seconds since 1970-01-01 00:00:00 UTC
    /// @param nanos nanosecond component, in the range `[0, 999999999]`
    /// @param precision number of fractional digits to write (0-9)
    /// @return number of characters written to the buffer.
    /// @throws if the given format specifier is invalid

    VTZ_EXPORT size_t format_precise_to_s( string_view fmt,
        sysseconds_t                                   t,
        u32                                            nanos,
        int                                            precision,
        char*                                          buff,
        size_t                                         count );


    /// Formats a UTC time with sub-second precision to the given buffer,
    /// with time given as a `std::chrono::sys_time`. Output is truncated
    /// if it would exceed `count`.
    ///
    /// The time is interpreted as UTC. `%Z` will produce "UTC", and `%z`
    /// will produce "+00".
    ///
    /// For format specifiers, see:
    /// https://en.cppreference.com/w/cpp/chrono/c/strftime.html
    ///
    /// @param fmt format string describing time, eg "%Y-%m-%d %H:%M:%S"
    /// @param t time since 1970-01-01 00:00:00 UTC
    /// @param precision number of fractional digits to write (0-9)
    /// @return number of characters written to the buffer.
    /// @throws if the given format specifier is invalid

    template<class Dur>
    size_t format_precise_to( string_view fmt,
        sys_time<Dur>                     t,
        int                               precision,
        char*                             buff,
        size_t                            count ) {
        auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
        auto nanos
            = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
        return format_precise_to_s(
            fmt, sec.count(), u32( nanos.count() ), precision, buff, count );
    }


    /// Formats a UTC time to a string, automatically selecting the
    /// appropriate precision for the given duration type. If the duration
    /// is coarser than or equal to seconds, no fractional component is
    /// written. Otherwise, the minimum number of fractional digits needed
    /// to represent the duration is used (up to 9 for nanoseconds).
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

    template<class Dur>
    std::string format( string_view fmt, sys_time<Dur> t ) {
        constexpr int prec = detail::get_necessary_precision<Dur>();
        using period       = typename Dur::period;
        constexpr auto n   = period::num;
        constexpr auto d   = period::den;

        if constexpr( prec == 0 )
        {
            if constexpr( d == 1
                          && n % 86400 == 0 ) // result is a multiple of a day
            {
                // format days
                return format_d( fmt, days( t.time_since_epoch() ).count() );
            }
            else
            {
                // format seconds
                return format_s( fmt, seconds( t.time_since_epoch() ).count() );
            }
        }
        else
        {
            auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
            auto nanos
                = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
            return format_precise_s(
                fmt, sec.count(), u32( nanos.count() ), prec );
        }
    }


    /// Formats a UTC time to the given buffer, automatically selecting the
    /// appropriate precision for the given duration type. If the duration
    /// is coarser than or equal to seconds, no fractional component is
    /// written. Otherwise, the minimum number of fractional digits needed
    /// to represent the duration is used (up to 9 for nanoseconds).
    /// Output is truncated if it would exceed `count`.
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

    template<class Dur>
    size_t format_to(
        string_view fmt, sys_time<Dur> t, char* buff, size_t count ) {
        constexpr int prec = detail::get_necessary_precision<Dur>();
        using period       = typename Dur::period;
        constexpr auto n   = period::num;
        constexpr auto d   = period::den;


        if constexpr( prec == 0 )
        {
            if constexpr( d == 1
                          && n % 86400 == 0 ) // result is a multiple of a day
            {
                return format_to_d(
                    fmt, days( t.time_since_epoch() ).count(), buff, count );
            }
            else
            {
                return format_to_s(
                    fmt, seconds( t.time_since_epoch() ).count(), buff, count );
            }
        }
        else
        {
            auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
            auto nanos
                = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
            return format_precise_to_s(
                fmt, sec.count(), u32( nanos.count() ), prec, buff, count );
        }
    }
} // namespace vtz
