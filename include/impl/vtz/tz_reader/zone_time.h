#pragma once

#include <vtz/impl/bit.h>
#include <vtz/tz_reader/from_utc.h>
#include <vtz/tz_reader/zone_save.h>

namespace vtz {
    /// Holds the stdoff and walloff for a zone at a particular point in time
    struct alignas( 8 ) zone_time {
        from_utc stdoff;  ///< Zone stdoff (non-DST offset)
        from_utc walloff; ///< Actual offset in this state

        /// Return the save implied by the zone
        constexpr zone_save save() const noexcept {
            return walloff.off - stdoff.off;
        }

        bool operator==( zone_time const& rhs ) const noexcept {
            return _b8( *this ) == _b8( rhs );
        }

        bool operator!=( zone_time const& rhs ) const noexcept {
            return _b8( *this ) != _b8( rhs );
        }
    };
} // namespace vtz
