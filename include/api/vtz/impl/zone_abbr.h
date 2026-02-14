#pragma once

#include <vtz/impl/fix_str.h>
#include <vtz/impl/bit.h>

namespace vtz {
    struct ZoneAbbr : FixStr<15> {
        bool operator==( ZoneAbbr const& rhs ) const noexcept {
            return B16( *this ) == B16( rhs );
        }
        bool operator!=( ZoneAbbr const& rhs ) const noexcept {
            return B16( *this ) != B16( rhs );
        }


        /// Construct a ZoneAbbr from a string_view. Clamp to the max length of
        /// a ZoneAbbr
        static ZoneAbbr from_sv( string_view sv ) noexcept {
            ZoneAbbr    abbr{};
            char const* p = sv.data();
            size_t      s = sv.size();
            if( s > max_size ) s = max_size;
            _vtz_memcpy( abbr.buff_, p, s );
            abbr.size_ = u8( s );
            return abbr;
        }
    };
} // namespace vtz
