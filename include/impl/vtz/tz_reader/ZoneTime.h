#pragma once

#include <vtz/impl/bit.h>
#include <vtz/tz_reader/FromUTC.h>
#include <vtz/tz_reader/RuleSave.h>

namespace vtz {
    /// Holds the stdoff and walloff for a zone at a particular point in time
    struct alignas( 8 ) ZoneTime {
        FromUTC stdoff;  ///< Zone stdoff (non-DST offset)
        FromUTC walloff; ///< Actual offset in this state

        /// Return the save implied by the zone
        constexpr RuleSave save() const noexcept {
            return walloff.off - stdoff.off;
        }

        bool operator==( ZoneTime const& rhs ) const noexcept {
            return _b8( *this ) == _b8( rhs );
        }

        bool operator!=( ZoneTime const& rhs ) const noexcept {
            return _b8( *this ) != _b8( rhs );
        }
    };
} // namespace vtz
