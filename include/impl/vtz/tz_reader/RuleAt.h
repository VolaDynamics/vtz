#pragma once

#include <vtz/civil.h>
#include <vtz/impl/bit.h>
#include <vtz/tz_reader/FromUTC.h>
#include <vtz/tz_reader/ZoneTime.h>

#include <vtz/inplace_optional.h>

#include <string>
#include <string_view>

namespace vtz {
    using std::string;
    using std::string_view;

    class RuleAt {
        i32 repr_{};

        constexpr explicit RuleAt( i32 repr ) noexcept
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

        constexpr static RuleAt hhmmss(
            Kind kind, int hh, int mm = 0, int ss = 0 ) {
            int sign  = hh < 0 ? -1 : 1;
            hh       *= sign;
            return RuleAt( sign * ( hh * 3600 + mm * 60 + ss ), kind );
        }

        constexpr i32  offset() const noexcept { return repr_ >> 2; }
        constexpr Kind kind() const noexcept { return Kind( repr_ & 0x3 ); }

        RuleAt() = default;

        constexpr RuleAt( i32 time_, Kind kind ) noexcept
        : repr_( i32( u32( time_ ) << 2 ) | i32( kind ) ) {}

        template<size_t N>
        RuleAt( char const ( &arr )[N] )
        : RuleAt( string_view( arr ) ) {}

        RuleAt( string_view text );

        constexpr bool operator==( RuleAt const& rhs ) const noexcept {
            return repr_ == rhs.repr_;
        }
        constexpr bool operator!=( RuleAt const& rhs ) const noexcept {
            return repr_ != rhs.repr_;
        }

        /// Returns the timestamp (in seconds from UTC) when the 'AT' time is
        /// referring to, on the given date
        constexpr sysseconds_t resolve_at(
            sysdays_t date, FromUTC stdoff, FromUTC walloff ) const noexcept {
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
            sysdays_t date, ZoneTime time ) const noexcept {
            return resolve_at( date, time.stdoff, time.walloff );
        }

        constexpr static RuleAt from_repr( i32 repr ) noexcept {
            return RuleAt( repr );
        }
    };

    template<>
    struct OptTraits<RuleAt> {
        constexpr static RuleAt NULL_VALUE = RuleAt::from_repr( INT32_MIN );
    };

    using OptRuleAt = OptClass<RuleAt>;

} // namespace vtz
