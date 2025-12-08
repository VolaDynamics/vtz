#pragma once

#include <vtz/bit.h>
#include <vtz/civil.h>
#include <vtz/strings.h>
#include <vtz/tz_reader/FromUTC.h>
#include <vtz/tz_reader/ZoneState.h>


namespace vtz {

    /// Represents the date within the year when a timezone switches from
    /// daylight time to standard time, or back, in the context of the 'rule'
    /// component of a POSIX TZ string.

    class TZDate {
        u32 repr_{};

        constexpr explicit TZDate( u32 repr ) noexcept
        : repr_( repr ) {}

      public:

        TZDate() = default;


        enum Kind {
            /// No TZDate
            None,

            /// TZ Date String of form `'n'` where `0 <= n <= 365`. It just
            /// directly measures the number of days since the start of the year
            DayOfYear,

            /// TZ Date String of form `'Jn'` where `1 <= n <= 365`. Leap days
            /// shall not be counted.
            Julian,

            /// TZ Date String of form Mm.n.d. Ex: M3.2.0 (2nd Sunday of March)
            /// or M11.1.0 (1st Sunday of November)
            ///
            /// - m is month (range [1-12])
            /// - n is week (range [1-5])
            /// - d is day of week (range [0-6], 0 is Sunday)
            ///
            /// if n==5, this means "last instance of the given day of the week,
            /// in the month"
            DayOfMonth,
        };

        constexpr bool     has_value() const noexcept { return bool( repr_ ); }
        constexpr explicit operator bool() const noexcept {
            return bool( repr_ );
        }


        /// Empty tzdate
        constexpr static TZDate none() noexcept { return {}; }

        /// TZ Date String of form `'n'` where `0 <= n <= 365`. It just
        /// directly measures the number of days since the start of the year
        constexpr static TZDate make_doy( int n ) noexcept {
            return TZDate( u32( n ) << 2 | u32( DayOfYear ) );
        }

        /// TZ Date String of form `'Jn'` where `1 <= n <= 365`. Leap days
        /// shall not be counted.
        constexpr static TZDate make_julian( int n ) noexcept {
            return TZDate( u32( n ) << 2 | u32( Julian ) );
        }

        /// TZ Date String of form Mm.n.d. Ex: M3.2.0 (2nd Sunday of March)
        /// or M11.1.0 (1st Sunday of November)
        ///
        /// - m is month (range [1-12])
        /// - n is week (range [1-5])
        /// - d is day of week (range [0-6], 0 is Sunday)
        ///
        /// if n==5, this means "last instance of the given day of the week,
        /// in the month"
        constexpr static TZDate make_dom( Mon m, u32 w, DOW d ) noexcept {
            return TZDate( ( u32( m ) & 0xf ) << 2 //
                           | u32( w & 0x7 ) << 6   //
                           | u32( d ) << 9         //
                           | u32( DayOfMonth ) );
        }

        sysdays_t resolve_date( i32 year ) const noexcept {
            switch( kind() )
            {
            case None: return resolve_civil( year );
            case Julian:
                {
                    // March 1st is 59 days into the year for NON-leap years
                    constexpr int MAR_1 = 31 + 28;
                    // Compute the day of the year. We subtract 1 because 1 <= n
                    // <= 365 for the Julian convention
                    int doy = this->dayOfYear() - 1;
                    if( doy < MAR_1 )
                    {
                        // Day is before march 1st for a non-leap year, so we
                        // can add from the beginning of the year
                        return resolve_civil( year ) + doy;
                    }
                    else
                    {
                        // We ignore leap years, so: if we're on or after March
                        // 1st, we measure from March 1st
                        return resolve_civil( year, 3, 1 ) + ( doy - MAR_1 );
                    }
                }
            // We just use dayOfYear() as a direct count from the beginning of
            // the year
            case DayOfYear: return resolve_civil( year ) + dayOfYear();
            case DayOfMonth:
                {
                    auto m = month();
                    auto w = week();
                    auto d = dow();
                    if( w == 5 )
                    {
                        // If the week is 5, return the last appearance of that
                        // weekday within the month
                        return resolve_last_dow( year, u32( m ), d );
                    }
                    return resolve_dow_ge( year, u32( m ), 1, d )
                           + ( w - 1 ) * 7;
                }
            }

            VTZ_UNREACHABLE();
        }

        constexpr Kind kind() const noexcept { return Kind( repr_ & 0x3 ); }

        /// Day of the year (for kind() != DayOfMonth)
        constexpr int dayOfYear() const noexcept { return repr_ >> 2; }

        /// month range [1-12] (when kind() == DayOfMonth)
        constexpr Mon month() const noexcept {
            return Mon( ( repr_ >> 2 ) & 0xf );
        }

        /// week range [1-5] (when kind() == DayOfMonth)
        constexpr u32 week() const noexcept { return ( repr_ >> 6 ) & 0x7; }

        /// dow range [0-6] (when kind() == DayOfMonth)
        constexpr DOW dow() const noexcept { return DOW( repr_ >> 9 ); }

        bool operator==( TZDate r ) const noexcept { return repr_ == r.repr_; }
        bool operator!=( TZDate r ) const noexcept { return repr_ != r.repr_; }
    };


    struct TZRule : TZDate {
        i32 time; //< local time when transition occurs

        using TZDate::dayOfYear;
        using TZDate::dow;
        using TZDate::has_value;
        using TZDate::kind;
        using TZDate::week;
        using TZDate::operator bool;

        sysseconds_t resolve( i32 year, FromUTC when ) const noexcept {
            return when.to_utc( resolve_date( year ) * 86400ll + time );
        }

        TZDate const& date() const noexcept { return *this; }

        bool operator==( TZRule const& rhs ) const noexcept {
            return B8{ *this } == B8{ rhs };
        }
        bool operator!=( TZRule const& rhs ) const noexcept {
            return B8{ *this } != B8{ rhs };
        }
    };


    /// Struct representation of posix TZ string
    ///
    /// See:
    /// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html#tag_08_03
    struct TZString {
        ZoneAbbr abbr1; /// Standard abbreviation
        ZoneAbbr abbr2; /// Daylight abbreviation
        FromUTC  off1;  /// Standard offset
        FromUTC  off2;  /// DST offset
        TZRule   r1;
        TZRule   r2;

        sysseconds_t resolve_dst_start( i32 year ) const noexcept {
            return r1.resolve( year, off1 );
        }
        sysseconds_t resolve_dst_end( i32 year ) const noexcept {
            return r2.resolve( year, off2 );
        }

        bool has_daylight_rules() const noexcept { return r1.has_value(); }
    };
} // namespace vtz
