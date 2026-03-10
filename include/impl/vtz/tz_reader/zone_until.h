#pragma once

#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/inplace_optional.h>
#include <vtz/tz_reader/rule_at.h>

namespace vtz {
    struct zone_until {
        sys_days_t date;
        rule_at   at; ///< Time of day when it ends


        /// Return the date referred to by this zone_until
        constexpr sys_days_t resolve_date() const noexcept { return date; }

        /// Return the time this 'until' refers to
        constexpr sys_seconds_t resolve( zone_time time ) const noexcept {
            return at.resolve_at( date, time );
        }

        bool operator==( zone_until const& rhs ) const noexcept {
            return _b8( *this ) == _b8( rhs );
        }

        constexpr bool has_value() const noexcept { return date != MAX_DAYS; }

        constexpr static zone_until none() noexcept { return { MAX_DAYS, {} }; }
    };
} // namespace vtz
