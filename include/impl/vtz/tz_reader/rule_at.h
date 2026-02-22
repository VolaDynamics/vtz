#pragma once

#include <vtz/civil.h>
#include <vtz/impl/bit.h>
#include <vtz/tz_reader/from_utc.h>
#include <vtz/tz_reader/zone_time.h>

#include <vtz/inplace_optional.h>

#include <string>
#include <string_view>

namespace vtz {
    using std::string;
    using std::string_view;

    class rule_at {
        i32 repr_{};

        constexpr explicit rule_at( i32 repr ) noexcept
        : repr_( repr ) {}

      public:

        enum Kind : u8 {
            /// 'AT' field corresponds to local WALL time (this is the default)
            /// suffix _may_ be 'w', but also may not be present
            LOCAL_WALL,
            /// 'AT' field corresponds to local standard time ('s' suffix)
            LOCAL_STANDARD,
            /// 'AT' field corresponds to standard time at the prime meridian
            /// suffix can be g, u, or z
            UTC,
        };

        constexpr static rule_at hhmmss(
            Kind kind, int hh, int mm = 0, int ss = 0 ) {
            int sign  = hh < 0 ? -1 : 1;
            hh       *= sign;
            return rule_at( sign * ( hh * 3600 + mm * 60 + ss ), kind );
        }

        constexpr i32  offset() const noexcept { return repr_ >> 2; }
        constexpr Kind kind() const noexcept { return Kind( repr_ & 0x3 ); }

        rule_at() = default;

        constexpr rule_at( i32 time_, Kind kind ) noexcept
        : repr_( i32( u32( time_ ) << 2 ) | i32( kind ) ) {}

        template<size_t N>
        rule_at( char const ( &arr )[N] )
        : rule_at( string_view( arr ) ) {}

        rule_at( string_view text );

        constexpr bool operator==( rule_at const& rhs ) const noexcept {
            return repr_ == rhs.repr_;
        }
        constexpr bool operator!=( rule_at const& rhs ) const noexcept {
            return repr_ != rhs.repr_;
        }

        /// Returns the timestamp (in seconds from UTC) when the 'AT' time is
        /// referring to, on the given date
        constexpr sysseconds_t resolve_at(
            sysdays_t date, from_utc stdoff, from_utc walloff ) const noexcept {
            // Time when the date starts (midnight on that date)
            i64 T = days_to_seconds( date );

            // time of day -
            i32 time_of_day = offset();
            switch( kind() )
            {
            /// Time of day represents an offset from the current local time in
            /// the zone
            case LOCAL_WALL: return T + walloff.to_utc( time_of_day );
            /// Time of day represents an offset from standard time within the
            /// zone
            case LOCAL_STANDARD: return T + stdoff.to_utc( time_of_day );
            /// Time of day represents an offset from UTC time within the zone
            case UTC: return T + time_of_day;
            }

            // Fallback - handle as UTC. We just have this here to prevent UB
            return T + time_of_day;
        }


        /// Returns the timestamp (in seconds from UTC) when the 'AT' time is
        /// referring to, on the given date
        constexpr sysseconds_t resolve_at(
            sysdays_t date, zone_time time ) const noexcept {
            return resolve_at( date, time.stdoff, time.walloff );
        }

        constexpr static rule_at from_repr( i32 repr ) noexcept {
            return rule_at( repr );
        }
    };

    template<>
    struct opt_traits<rule_at> {
        constexpr static rule_at NULL_VALUE = rule_at::from_repr( INT32_MIN );
    };

    using OptRuleAt = opt_class<rule_at>;

} // namespace vtz
