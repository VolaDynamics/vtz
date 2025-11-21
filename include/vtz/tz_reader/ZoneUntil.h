#pragma once

#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/inplace_optional.h>
#include <vtz/tz_reader/RuleAt.h>

namespace vtz {
    struct ZoneUntil {
        sysdays_t date;
        RuleAt    at; ///< Time of day when it ends


        /// Return the date referred to by this ZoneUntil
        constexpr sysdays_t resolve_date() const noexcept { return date; }

        /// Return the time this 'until' refers to
        constexpr sysseconds_t resolve( ZoneTime time ) const noexcept {
            return at.resolve_at( date, time );
        }

        bool operator==( ZoneUntil const& rhs ) const noexcept {
            return B8( *this ) == B8( rhs );
        }

        constexpr bool has_value() const noexcept { return date != MAX_DAYS; }

        constexpr static ZoneUntil none() noexcept { return { MAX_DAYS, {} }; }
    };

    string format_as( ZoneUntil );
} // namespace vtz
