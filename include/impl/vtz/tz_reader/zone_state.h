#pragma once

#include <vtz/strings.h>
#include <vtz/tz_reader/rule_letter.h>
#include <vtz/tz_reader/zone_format.h>
#include <vtz/tz_reader/zone_time.h>

namespace vtz {

    struct alignas( 8 ) zone_state : zone_time {
        zone_abbr abbr; ///< Zone abbreviation

        zone_state() = default;

        constexpr zone_state(
            from_utc stdoff, zone_save save, zone_abbr const& abbr ) noexcept
        : zone_time{ stdoff, stdoff.save( save ) }
        , abbr( abbr ) {}

        /// Create a zone_state where stdoff==walloff.
        constexpr zone_state( from_utc stdoff,
            zone_format const&         fmt,
            rule_letter                letter ) noexcept
        : zone_time{ stdoff, stdoff }
        , abbr( fmt.format( walloff, false, letter ) ) {}

        constexpr zone_state( from_utc stdoff,
            from_utc                   walloff,
            zone_format const&         fmt,
            rule_letter                letter ) noexcept
        : zone_time{ stdoff, walloff }
        , abbr( fmt.format( walloff, stdoff != walloff, letter ) ) {}

        constexpr zone_state( from_utc stdoff,
            zone_save                  save,
            zone_format const&         fmt,
            rule_letter                letter ) noexcept
        : zone_time{ stdoff, stdoff.save( save ) }
        , abbr( fmt.format( walloff, stdoff != walloff, letter ) ) {}

        constexpr zone_state(
            from_utc stdoff, from_utc walloff, zone_abbr const& abbr )
        : zone_time{ stdoff, walloff }
        , abbr( abbr ) {}

        bool operator==( zone_state const& rhs ) const noexcept {
            return _b24( *this ) == _b24( rhs );
        }
        bool operator!=( zone_state const& rhs ) const noexcept {
            return _b24( *this ) != _b24( rhs );
        }
    };

} // namespace vtz
