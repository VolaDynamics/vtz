#pragma once

#include <vtz/strings.h>
#include <vtz/tz_reader/RuleLetter.h>
#include <vtz/tz_reader/ZoneFormat.h>
#include <vtz/tz_reader/ZoneTime.h>

namespace vtz {

    struct alignas( 8 ) ZoneState : ZoneTime {
        zone_abbr abbr; ///< Zone abbreviation

        ZoneState() = default;

        constexpr ZoneState(
            FromUTC stdoff, RuleSave save, zone_abbr const& abbr ) noexcept
        : ZoneTime{ stdoff, stdoff.save( save ) }
        , abbr( abbr ) {}

        /// Create a ZoneState where stdoff==walloff.
        constexpr ZoneState(
            FromUTC stdoff, ZoneFormat const& fmt, RuleLetter letter ) noexcept
        : ZoneTime{ stdoff, stdoff }
        , abbr( fmt.format( walloff, false, letter ) ) {}

        constexpr ZoneState( FromUTC stdoff,
            FromUTC                  walloff,
            ZoneFormat const&        fmt,
            RuleLetter               letter ) noexcept
        : ZoneTime{ stdoff, walloff }
        , abbr( fmt.format( walloff, stdoff != walloff, letter ) ) {}

        constexpr ZoneState( FromUTC stdoff,
            RuleSave                 save,
            ZoneFormat const&        fmt,
            RuleLetter               letter ) noexcept
        : ZoneTime{ stdoff, stdoff.save( save ) }
        , abbr( fmt.format( walloff, stdoff != walloff, letter ) ) {}

        constexpr ZoneState(
            FromUTC stdoff, FromUTC walloff, zone_abbr const& abbr )
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
