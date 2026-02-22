#pragma once

#include <vtz/impl/bit.h>

#include <string>
#include <string_view>

namespace vtz {
    using std::string;
    using std::string_view;

    struct zone_save {
        i32 save = 0;

        constexpr zone_save() = default;
        constexpr zone_save( i32 save ) noexcept
        : save( save ) {}

        template<size_t N>
        zone_save( char const ( &arr )[N] )
        : zone_save( string_view( arr ) ) {}

        zone_save( string_view text );

        static auto hh_mm( int sign, i32 hour, i32 min ) noexcept -> zone_save {
            return { sign * ( 3600 * hour + 60 * min ) };
        }
        static auto hh_mm_ss( int sign, i32 hour, i32 min, i32 sec ) noexcept
            -> zone_save {
            return { sign * ( 3600 * hour + 60 * min + sec ) };
        }

        bool operator==( zone_save rhs ) const noexcept {
            return save == rhs.save;
        }
    };
} // namespace vtz
