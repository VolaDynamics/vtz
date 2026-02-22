#pragma once

#include <vtz/strings.h>
#include <vtz/tz_reader/rule_letter.h>
#include <vtz/tz_reader/zone_format.h>
#include <vtz/tz_reader/zone_time.h>

namespace vtz {

    struct alignas( 8 ) ZoneState : ZoneTime {
        zone_abbr abbr; ///< Zone abbreviation

        ZoneState() = default;

        constexpr ZoneState(
            from_utc stdoff, zone_save save, zone_abbr const& abbr ) noexcept
        : ZoneTime{ stdoff, stdoff.save( save ) }
        , abbr( abbr ) {}

        /// Create a ZoneState where stdoff==walloff.
        constexpr ZoneState(
            from_utc stdoff, ZoneFormat const& fmt, RuleLetter letter ) noexcept
        : ZoneTime{ stdoff, stdoff }
        , abbr( fmt.format( walloff, false, letter ) ) {}

        constexpr ZoneState( from_utc stdoff,
            from_utc                  walloff,
            ZoneFormat const&         fmt,
            RuleLetter                letter ) noexcept
        : ZoneTime{ stdoff, walloff }
        , abbr( fmt.format( walloff, stdoff != walloff, letter ) ) {}

        constexpr ZoneState( from_utc stdoff,
            zone_save                 save,
            ZoneFormat const&         fmt,
            RuleLetter                letter ) noexcept
        : ZoneTime{ stdoff, stdoff.save( save ) }
        , abbr( fmt.format( walloff, stdoff != walloff, letter ) ) {}

        constexpr ZoneState(
            from_utc stdoff, from_utc walloff, zone_abbr const& abbr )
        : ZoneTime{ stdoff, walloff }
        , abbr( abbr ) {}

        bool operator==( ZoneState const& rhs ) const noexcept {
            return _b24( *this ) == _b24( rhs );
        }
        bool operator!=( ZoneState const& rhs ) const noexcept {
            return _b24( *this ) != _b24( rhs );
        }
    };

} // namespace vtz
