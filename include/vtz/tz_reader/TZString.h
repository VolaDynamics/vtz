#pragma once

#include "vtz/civil.h"
#include <vtz/strings.h>
#include <vtz/tz_reader/FromUTC.h>
#include <vtz/tz_reader/ZoneState.h>


namespace vtz {

    /// Represents the date within the year when a timezone switches from
    /// daylight time to standard time, or back, in the context of the 'rule'
    /// component of a POSIX TZ string.

    class TZDate {
        u16 repr_;

        constexpr explicit TZDate( u16 repr ) noexcept
        : repr_( repr ) {}

      public:

        enum Kind {

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

        sysdays_t resolve_date( i32 year ) const noexcept {
            switch( kind() )
            {
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
                        return resolve_last_dow( year, m, d );
                    }
                    return resolve_dow_ge( year, m, 1, d ) + ( w - 1 ) * 7;
                }
            }

            // Fallback - the kind is invalid
            return resolve_civil( year );
        }

        constexpr Kind kind() const noexcept { return Kind( repr_ & 0x3 ); }

        /// Day of the year (for kind() != DayOfMonth)
        constexpr int dayOfYear() const noexcept { return repr_ >> 2; }

        /// month range [1-12] (when kind() == DayOfMonth)
        constexpr u32 month() const noexcept { return ( repr_ >> 2 ) & 0xf; }

        /// week range [0-6] (when kind() == DayOfMonth)
        constexpr u32 week() const noexcept { return ( repr_ >> 6 ) & 0x7; }

        /// dow range [0-6] (when kind() == DayOfMonth)
        constexpr DOW dow() const noexcept { return DOW( repr_ >> 9 ); }
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
        TZDate   d1; /// Start of daylight savings time
        TZDate   d2; /// End of daylight savings time
        i32      t1; /// Time intraday when transition happens (start of daylight savings time), as seconds
        i32      t2; /// Time intraday when transition happens (end of daylight savings time), as seconds
    };
} // namespace vtz
