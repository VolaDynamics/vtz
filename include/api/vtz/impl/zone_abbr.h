#pragma once

#include <vtz/impl/bit.h>
#include <vtz/impl/fix_str.h>

namespace vtz {
    struct zone_abbr : fix_str<15> {
        bool operator==( zone_abbr const& rhs ) const noexcept {
            return _b16( *this ) == _b16( rhs );
        }
        bool operator!=( zone_abbr const& rhs ) const noexcept {
            return _b16( *this ) != _b16( rhs );
        }


        /// Construct a ZoneAbbr from a string_view. Clamp to the max length of
        /// a ZoneAbbr
        static zone_abbr from_sv( string_view sv ) noexcept {
            zone_abbr   abbr{};
            char const* p = sv.data();
            size_t      s = sv.size();
            if( s > max_size ) s = max_size;
            _vtz_memcpy( abbr.buff_, p, s );
            abbr.size_ = u8( s );
            return abbr;
        }
    };
} // namespace vtz
