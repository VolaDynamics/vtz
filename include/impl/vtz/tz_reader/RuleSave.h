#pragma once

#include <vtz/impl/bit.h>

#include <string>
#include <string_view>

namespace vtz {
    using std::string;
    using std::string_view;

    struct RuleSave {
        i32 save = 0;

        constexpr RuleSave() = default;
        constexpr RuleSave( i32 save ) noexcept
        : save( save ) {}

        template<size_t N>
        RuleSave( char const ( &arr )[N] )
        : RuleSave( string_view( arr ) ) {}

        RuleSave( string_view text );

        static auto HHMM( int sign, i32 hour, i32 min ) noexcept -> RuleSave {
            return { sign * ( 3600 * hour + 60 * min ) };
        }
        static auto HHMMSS( int sign, i32 hour, i32 min, i32 sec ) noexcept
            -> RuleSave {
            return { sign * ( 3600 * hour + 60 * min + sec ) };
        }

        bool operator==( RuleSave rhs ) const noexcept {
            return save == rhs.save;
        }
    };
    string format_as( RuleSave );
} // namespace vtz
