#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <vtz/impl/chrono_types.h>
#include <vtz/impl/export.h>
#include <vtz/impl/math.h>
#include <vtz/impl/zone_abbr.h>
#include <vtz/types.h>

#include <vtz/impl/bit.h>
#include <vtz/impl/tz_impl.h>
#include <vtz/impl/tz_tables.h>


namespace vtz {
    using std::string;

    class VTZ_EXPORT time_zone
    : private off_tables
    , private abbr_table
    , private stdoff_table
    , private trans_table {
      public:

        /// Constructs a time_zone representing UTC (stdoff of 0, walloff of 0,
        /// abbreviation is UTC, etc)
        static time_zone utc();

        time_zone( string_view name, zone_states const& states );

        using abbr_table::abbrev_s;
        using abbr_table::abbrev_string_s;
        using abbr_table::abbrev_to_s;
        using off_tables::offset_s;
        using off_tables::to_local_ns;
        using off_tables::to_local_s;
        using off_tables::to_sys_ns;
        using off_tables::to_sys_s;
        using stdoff_table::stdoff_s;

        /// For a given system time T, represented as "offsets from UTC", return
        /// the timezone's current offset from UTC, in seconds.
        template<class Dur>
        seconds offset( sys_time<Dur> t ) const {
            return seconds( offset_s( _raw_time( t ) ) );
        }

        /// Return the name of the zone
        string_view name() const noexcept { return name_; }

        /// Return the current save, in seconds
        i32 save_s( sysseconds_t t ) const noexcept {
            return i32( offset_s( t ) - stdoff_s( t ) );
        }

        /// Convert the given time (sys_seconds) to local seconds
        local_seconds to_local( sys_seconds s ) const {
            return _local( to_local_s( s.time_since_epoch().count() ) );
        }

        /// Convert the given time (local seconds) to UTC
        sys_seconds to_sys( local_seconds s, choose z ) const {
            return _sys( to_sys_s( s.time_since_epoch().count(), z ) );
        }

        /// Convert the given time (sys_seconds) to local seconds
        template<class Dur>
        auto to_local( sys_time<Dur> t ) const
            -> local_time<std::common_type_t<Dur, seconds>> {
            // This is the time we're actually looking up, in seconds
            sys_seconds t2 = std::chrono::floor<seconds>( t );

            // This is the local time (with the unit being seconds)
            local_seconds local_t
                = _local( to_local_s( t2.time_since_epoch().count() ) );

            // if t2 is less precise than t, we chopped off some unit of time
            // (eg, some number of nanoseconds)
            //
            // We need to add it back.
            auto delta = t - t2;

            return local_t + delta;
        }

        /// Convert the given time (local seconds) to UTC
        template<class Dur>
        auto to_sys( local_time<Dur> t, choose z ) const
            -> sys_time<std::common_type_t<Dur, seconds>> {
            // Time we're looking up (must be seconds)
            local_seconds t2 = std::chrono::floor<seconds>( t );

            // This is the corresponding system time (with the unit being in
            // seconds)
            sys_seconds sys_t
                = _sys( to_sys_s( t2.time_since_epoch().count(), z ) );

            // if t2 is less precise than t, we chopped off some unit of time
            // (eg, some number of nanoseconds)
            //
            // We need to add it back.
            auto delta = t - t2;

            return sys_t + delta;
        }

        sys_info get_info_sys_s( sysseconds_t t ) const {
            auto range = sys_range_s( t );

            auto off  = offset_s( t );
            auto save = off - stdoff_s( t );

            return sys_info{
                sys_seconds( seconds( range.begin ) ),
                sys_seconds( seconds( range.end ) ),
                seconds( off ),
                std::chrono::floor<minutes>( seconds( save ) ),
                abbrev_string_s( t ),
            };
        }

        local_info get_info_local_s( sec_t t ) const {
            sysseconds_t tt[2];
            int          result = lookup_local( t, tt );
            if( result == local_info::unique )
            {
                return local_info{
                    result,
                    get_info_sys_s( tt[0] ),
                    sys_info{},
                };
            }
            return local_info{
                result,
                get_info_sys_s( tt[0] ),
                get_info_sys_s( tt[1] ),
            };
        }

        template<class Dur>
        local_info get_info( local_time<Dur> input ) const {
            return get_info_local_s( _raw_time( input ) );
        }

        template<class Dur>
        sys_info get_info( sys_time<Dur> input ) const {
            return get_info_sys_s( _raw_time( input ) );
        }

        /// Returns a number [0,86400) representing the current time of day
        /// (local time)
        u32 local_time_of_day_s( sysseconds_t s ) const noexcept {
            return u32( math::rem<86400>( to_local_s( s ) ) );
        }

        /// Returns the date of the given systime, as number of days since the
        /// epoch
        i64 local_date_s( sysseconds_t t ) const noexcept {
            return math::div_floor<86400>( to_local_s( t ) );
        }

        /// Given nanoseconds since the epoch, return the current date (as days
        /// since the epoch)
        i64 local_date_ns( nanos_t nanos ) const noexcept {
            return math::div_floor<86400000000000ll>( to_local_ns( nanos ) );
        }

        template<class Dur>
        auto local_date( sys_time<Dur> t ) const -> local_days {
            return local_days(
                days( local_date_s( std::chrono::floor<seconds>( t )
                        .time_since_epoch()
                        .count() ) ) );
        }

        // clang-format off

        /// Formats a time to the given buffer. For format specifiers, see: https://en.cppreference.com/w/cpp/chrono/c/strftime.html
        size_t format_to_s( string_view format, sysseconds_t t, char* buff, size_t count ) const;
        /// Formats a time to the given buffer. For format specifiers, see: https://en.cppreference.com/w/cpp/chrono/c/strftime.html
        size_t format_to(   string_view format, sys_seconds  t, char* buff, size_t count ) const { return format_to_s( format, t.time_since_epoch().count(), buff, count ); }
        /// Formats a time with std::strftime specifiers. See: https://en.cppreference.com/w/cpp/chrono/c/strftime.html
        string format_s(    string_view format, sysseconds_t t ) const;
        /// Formats a time with std::strftime specifiers. See: https://en.cppreference.com/w/cpp/chrono/c/strftime.html
        string format(      string_view format, sys_seconds  t ) const { return format_s( format, t.time_since_epoch().count() ); }


        /// Formats as `%Y-%m-%d %H:%M:%S %Z`. Example: `2025-11-06 17:50:00 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        size_t format_to_s( sysseconds_t t, char* buff, size_t count, char date_sep = '-', char date_time_sep = ' ', char abbrev_sep = ' ' ) const noexcept;
        /// Formats as `%Y-%m-%d %H:%M:%S %Z`. Example: `2025-11-06 17:50:00 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        size_t format_to(   sys_seconds  t, char* buff, size_t count, char date_sep = '-', char date_time_sep = ' ', char abbrev_sep = ' ' ) const { return format_to_s( t.time_since_epoch().count(), buff, count, date_sep, date_time_sep, abbrev_sep ); }
        /// Formats as `%Y-%m-%d %H:%M:%S %Z`. Example: `2025-11-06 17:50:00 EST`
        string format_s(    sysseconds_t t, char date_sep = '-', char date_time_sep = ' ', char abbrev_sep = ' ' ) const;
        /// Formats as `%Y-%m-%d %H:%M:%S %Z`. Example: `2025-11-06 17:50:00 EST`
        string format(      sys_seconds  t, char date_sep = '-', char date_time_sep = ' ', char abbrev_sep = ' ' ) const { return format_s( t.time_since_epoch().count(), date_sep, date_time_sep, abbrev_sep ); }


        /// Formats as `%Y%m%d %H%M%S %Z`. Example: `20251106 175000 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        size_t format_compact_to_s( sysseconds_t t, char* p, size_t count, char date_time_sep = ' ', char abbrev_sep = ' ' ) const noexcept;
        /// Formats as `%Y%m%d %H%M%S %Z`. Example: `20251106 175000 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        size_t format_compact_to(   sys_seconds  t, char* p, size_t count, char date_time_sep = ' ', char abbrev_sep = ' ' ) const { return format_compact_to_s( t.time_since_epoch().count(), p, count, date_time_sep, abbrev_sep ); }
        /// Formats as `%Y%m%d %H%M%S %Z`. Example: `20251106 175000 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        string format_compact_s(    sysseconds_t t, char date_time_sep = ' ', char abbrev_sep = ' ' ) const;
        /// Formats as `%Y%m%d %H%M%S %Z`. Example: `20251106 175000 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        string format_compact(      sys_seconds  t, char date_time_sep = ' ', char abbrev_sep = ' ' ) const { return format_compact_s( t.time_since_epoch().count(), date_time_sep, abbrev_sep ); }


        /// Formats as `%Y-%m-%d[date_time_sep]%H:%M:%S%z`, with the option to use an alternative date separator. Example: 2025-11-06T17:50:00-05
        size_t format_iso8601_to_s( sysseconds_t t, char* buff, size_t count, char date_sep = '-', char date_time_sep = 'T' ) const noexcept;
        /// Formats as `%Y-%m-%d[date_time_sep]%H:%M:%S%z`, with the option to use an alternative date separator. Example: 2025-11-06T17:50:00-05
        size_t format_iso8601_to(   sys_seconds  t, char* buff, size_t count, char date_sep = '-', char date_time_sep = 'T' ) const { return format_iso8601_to_s( t.time_since_epoch().count(), buff, count, date_sep, date_time_sep );}
        /// Formats as `%Y-%m-%d[date_time_sep]%H:%M:%S%z`, with the option to use an alternative date separator. Example: 2025-11-06T17:50:00-05
        string format_iso8601_s(    sysseconds_t t, char date_sep = '-', char date_time_sep = 'T' ) const;
        /// Formats as `%Y-%m-%d[date_time_sep]%H:%M:%S%z`, with the option to use an alternative date separator. Example: 2025-11-06T17:50:00-05
        string format_iso8601(      sys_seconds  t, char date_sep = '-', char date_time_sep = 'T' ) const { return format_iso8601_s( t.time_since_epoch().count(), date_sep, date_time_sep );}


        /// Formats a time (expressed as seconds and nanoseconds) to the given buffer.
        /// For format specifiers, see: https://en.cppreference.com/w/cpp/chrono/c/strftime.html
        ///
        /// Preconditions: nanos is in the range `[0, 999999999]`, and precision<=9.
        /// The fractional component will formatted as a decimal followed by the given digits,
        /// and it will be placed immediately after the 'seconds' component.
        ///
        /// When formatting, if precision is less than 9, the value is truncated. Eg, if nanos
        /// is 999999999, but precision is 3, the fractional component is `.999`.
        ///
        /// If precision is 0, no fractional component is written, and the decimal point is omitted.
        ///
        /// Examples:
        ///
        /// - `"%Y%m%d %H:%M:%S-%Z"` with `t=1762442685`, `nanos=029380000` and `prec=4` would produce "20251106 08:24:45.0293-MST" for America/Denver
        /// - `"%F %T %Z"` with `t=1762442685`, `nanos=029380000` and `precision=4` would produce "2025-11-06 08:24:45.0293 MST" for America/Denver
        ///
        /// In the latter example, `%T` expands to `%H:%M:%S`.
        size_t format_precise_to_s( string_view format, sysseconds_t t, u32 nanos, int precision, char* buff, size_t count ) const;

        /// Format the given time, with the requested precision (up to 9 digits)
        template<class Dur>
        size_t format_precise_to( string_view format, sys_time<Dur> t, int precision, char* buff, size_t count ) const {
            auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
            auto nanos = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
            return format_precise_to_s( format, sec.count(), u32( nanos.count() ), precision, buff, count );
        }

        /// Formats a time (expressed as seconds and nanoseconds) to the given buffer. For more information,
        /// see the equivalent version of `format_to_s`.
        string format_precise_s( string_view format, sysseconds_t t, u32 nanos, int precision ) const;

        /// Format the given time, with the requested precision (up to 9 digits)
        template<class Dur>
        string format_precise( string_view format, sys_time<Dur> t, int precision ) const {
            auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
            auto nanos = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
            return format_precise_s( format, sec.count(), u32( nanos.count() ), precision );
        }

        /// Format the given time. If the input time has a precision higher than
        /// seconds, or it's fractional (or floating-point), then up to 9 digits
        /// will be included after the seconds component, as necessary to
        /// represent the input time.
        template<class Dur>
        size_t format_to( string_view format, sys_time<Dur> t, char* buff, size_t count ) const {
            constexpr int prec = detail::get_necessary_precision<Dur>();

            // If the required amount of precision is 0, format w/ seconds.
            // Otherwise, compute the necessary amount of precision, and format
            // with that.
            if constexpr( prec == 0 )
            {
                return format_to_s( format, seconds( t.time_since_epoch() ).count(), buff, count );
            }
            else
            {
                auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
                auto nanos = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
                return format_precise_to_s( format, sec.count(), u32( nanos.count() ), prec, buff, count );
            }
        }

        /// Format the given time. If the input time has a precision higher than
        /// seconds, or it's fractional (or floating-point), then up to 9 digits
        /// will be included after the seconds component, as necessary to
        /// represent the input time.
        template<class Dur>
        string format( string_view format, sys_time<Dur> t ) const {
            constexpr int prec = detail::get_necessary_precision<Dur>();

            if constexpr( prec == 0 )
            {
                return format_s( format, seconds( t.time_since_epoch() ).count() );
            }
            else
            {
                auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
                auto nanos = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
                return format_precise_s( format, sec.count(), u32( nanos.count() ), prec );
            }
        }

        // clang-format on


        struct _impl;

      private:

        static sec_t _raw_time( local_seconds s ) {
            return s.time_since_epoch().count();
        }
        static sec_t _raw_time( sys_seconds s ) {
            return s.time_since_epoch().count();
        }
        template<class Dur>
        static sec_t _raw_time( local_time<Dur> s ) {
            return std::chrono::floor<seconds>( s ).time_since_epoch().count();
        }
        template<class Dur>
        static sec_t _raw_time( sys_time<Dur> s ) {
            return std::chrono::floor<seconds>( s ).time_since_epoch().count();
        }

        static local_seconds _local( sec_t s ) {
            return local_seconds( seconds( s ) );
        }
        static sys_seconds _sys( sec_t s ) {
            return sys_seconds( seconds( s ) );
        }

        string name_;
    };


    VTZ_EXPORT std::string tzdb_version();

    /// Set the path to use for the timezone database. Will throw an exception
    /// if the timezone database is already loaded.
    VTZ_EXPORT void set_install( std::string path );

    VTZ_EXPORT std::string get_install();

    VTZ_EXPORT time_zone const* locate_zone( string_view name );

    VTZ_EXPORT time_zone const* current_zone();


    /// Sets the current zone used by vtz. This does NOT change the system
    /// timezone, it only sets the current zone returned by vtz. It's primarily
    /// useful if your application uses vtz, and you wish to use something other
    /// than the system timezone.
    ///
    /// Throws an exception if the given zone name could not be found within the
    /// timezone database.
    VTZ_EXPORT time_zone const* set_current_zone_for_application(
        string_view name );


    /// VTZ operates under the assumption that the current timezone will not
    /// change while the application is running.
    ///
    /// In order to override this behavior, you can call this function to
    /// atomically reload the current zone. The new current zone is returned.
    VTZ_EXPORT time_zone const* reload_current_zone();
} // namespace vtz


#ifdef VTZ_DATE_COMPAT
namespace date = vtz;
#endif
