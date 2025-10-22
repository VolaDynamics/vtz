#pragma once

#include <vtz/inplace_optional.h>
#include <vtz/date_types.h>
#include <vtz/civil.h>
#include <vtz/tz_reader/RuleAt.h>

namespace vtz {
        struct ZoneUntil {
        OptV<u16, 0> year; ///< year
        OptMon       mon;  ///< Month
        OptV<u8, 0>  day;  ///< Day of the month (1-31)
        OptRuleAt    at;   ///< Time of day when it ends


        /// Return the date referred to by this ZoneUntil
        constexpr sysdays_t resolveDate() const noexcept {
            return resolveCivil(
                *year, mon.value_or( Mon::Jan ), day.value_or( 1 ) );
        }

        /// Return the time this 'until' refers to
        constexpr sysseconds_t resolve( ZoneTime time ) const noexcept {
            // Choose a default 'at' of 12:00 noon
            // TODO: check if this is correct
            constexpr RuleAt DEFAULT_AT = RuleAt( 0, RuleAt::LOCAL_WALL );

            return at.value_or( DEFAULT_AT ).resolveAt( resolveDate(), time );
        }

        u64 _repr() const noexcept {
            u64 result{};
            static_assert( sizeof( ZoneUntil ) == sizeof( u64 ) );
            _vtz_memcpy( &result, this, sizeof( u64 ) );
            return result;
        }

        bool operator==( ZoneUntil const& rhs ) const noexcept {
            return _repr() == rhs._repr();
        }

        constexpr bool has_value() const noexcept { return year.has_value(); }
    };

    string format_as( ZoneUntil );
}
