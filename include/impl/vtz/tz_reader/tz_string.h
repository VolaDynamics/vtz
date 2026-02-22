#pragma once

#include <vtz/civil.h>
#include <vtz/impl/bit.h>
#include <vtz/strings.h>
#include <vtz/tz_reader/from_utc.h>
#include <vtz/tz_reader/zone_state.h>
#include <vtz/tz_reader/zone_transition.h>

#include <stdexcept>


namespace vtz {

    /// Represents the date within the year when a timezone switches from
    /// daylight time to standard time, or back, in the context of the 'rule'
    /// component of a POSIX TZ string.

    class tz_date {
        u32 repr_{};

        constexpr explicit tz_date( u32 repr ) noexcept
        : repr_( repr ) {}

      public:

        tz_date() = default;


        enum kind_t {
            /// No tz_date
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
        constexpr static tz_date none() noexcept { return {}; }

        /// TZ Date String of form `'n'` where `0 <= n <= 365`. It just
        /// directly measures the number of days since the start of the year
        constexpr static tz_date make_doy( int n ) noexcept {
            return tz_date( u32( n ) << 2 | u32( DayOfYear ) );
        }

        /// TZ Date String of form `'Jn'` where `1 <= n <= 365`. Leap days
        /// shall not be counted.
        constexpr static tz_date make_julian( int n ) noexcept {
            return tz_date( u32( n ) << 2 | u32( Julian ) );
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
        constexpr static tz_date make_dom(
            month_t m, u32 w, dow_t d ) noexcept {
            return tz_date( ( u32( m ) & 0xf ) << 2 //
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
                    int doy = this->day_of_year() - 1;
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
            case DayOfYear: return resolve_civil( year ) + day_of_year();
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

        constexpr kind_t kind() const noexcept { return kind_t( repr_ & 0x3 ); }

        /// Day of the year (for kind() != DayOfMonth)
        constexpr int day_of_year() const noexcept { return repr_ >> 2; }

        /// month range [1-12] (when kind() == DayOfMonth)
        constexpr month_t month() const noexcept {
            return month_t( ( repr_ >> 2 ) & 0xf );
        }

        /// week range [1-5] (when kind() == DayOfMonth)
        constexpr u32 week() const noexcept { return ( repr_ >> 6 ) & 0x7; }

        /// dow range [0-6] (when kind() == DayOfMonth)
        constexpr dow_t dow() const noexcept { return dow_t( repr_ >> 9 ); }

        bool operator==( tz_date r ) const noexcept { return repr_ == r.repr_; }
        bool operator!=( tz_date r ) const noexcept { return repr_ != r.repr_; }
    };


    struct tz_rule : tz_date {
        i32 time; //< local time when transition occurs

        using tz_date::day_of_year;
        using tz_date::dow;
        using tz_date::has_value;
        using tz_date::kind;
        using tz_date::week;
        using tz_date::operator bool;

        sysseconds_t resolve( i32 year, from_utc when ) const noexcept {
            return when.to_utc( resolve_date( year ) * 86400ll + time );
        }

        tz_date const& date() const noexcept { return *this; }

        bool operator==( tz_rule const& rhs ) const noexcept {
            return _b8{ *this } == _b8{ rhs };
        }
        bool operator!=( tz_rule const& rhs ) const noexcept {
            return _b8{ *this } != _b8{ rhs };
        }
    };


    /// Struct representation of posix TZ string
    ///
    /// See:
    /// https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html#tag_08_03
    struct tz_string {
        zone_abbr abbr1; /// Standard abbreviation
        zone_abbr abbr2; /// Daylight abbreviation
        from_utc  off1;  /// Standard offset
        from_utc  off2;  /// DST offset
        tz_rule   r1;
        tz_rule   r2;


        /// Resolve the start of dst within the year
        sysseconds_t resolve_dst( i32 year ) const noexcept {
            return r1.resolve( year, off1 );
        }


        /// Resolve the return to standard time within the year
        sysseconds_t resolve_std( i32 year ) const noexcept {
            return r2.resolve( year, off2 );
        }

        /// Zone State during standard time
        zone_state std() const noexcept {
            return zone_state{ off1, off1, abbr1 };
        }

        /// Zone State during daylight savings time
        zone_state dst() const noexcept {
            return zone_state{ off1, off2, abbr2 };
        }


        bool has_daylight_rules() const noexcept { return r1.has_value(); }


        /// Get zone transitions appearing strictly _after_ T
        vector<zone_transition> get_states(
            sysseconds_t T, sysseconds_t max ) const;
    };


    class tz_string_iter {
        using zt = zone_transition;

        sysseconds_t peek_next_time() const noexcept {
            return dst_next_ ? s.resolve_dst( year_dst )
                             : s.resolve_std( year_std );
        }

      public:

        /// Advance until the next transition time is _after_ T
        void advance_past( sysseconds_t T ) {
            while( peek_next_time() <= T ) { advance(); }
        }

        /// Advance the iterator
        void advance() noexcept {
            dst_next_ ? year_dst++ : year_std++;
            dst_next_ = !dst_next_;
        }

        /// peek at the next transition, without advancing
        zt peek() const noexcept {
            return dst_next_ ? zt{ s.resolve_dst( year_dst ), s.dst() }
                             : zt{ s.resolve_std( year_std ), s.std() };
        }

        /// Get the next zone transition
        zone_transition next() noexcept {
            auto result = peek();
            advance();
            return result;
        }

        /// Construct a tz_string_iter from the given string spec, starting at
        /// the given year. Assumes that the given string has daylight rules.
        tz_string_iter( tz_string const& s, i32 y )
        : s( s )
        , year_dst( y )
        , year_std( y )
        , dst_next_( s.resolve_dst( y ) < s.resolve_std( y ) ) {
            if( !s.has_daylight_rules() )
            {
                throw std::runtime_error( "tz_string_iter(): constructing a "
                                          "tz_string_iter for a string "
                                          "that doesn't have dst rules" );
            }
        }

        static tz_string_iter start_after(
            tz_string const& s, sysseconds_t T ) {
            auto date = sysdays_t( vtz::math::div_floor<86400>( T ) );
            auto it   = tz_string_iter( s, civil_year( date ) );
            it.advance_past( T );
            return it;
        }

      private:

        tz_string s;
        i32       year_dst;
        i32       year_std;
        bool      dst_next_;
    };
} // namespace vtz
