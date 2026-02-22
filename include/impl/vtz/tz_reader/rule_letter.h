#pragma once

#include <vtz/strings.h>


namespace vtz {
    struct alignas( 8 ) rule_letter : fix_str<7> {
        using base = fix_str<7>;

        using base::base;

        rule_letter() = default;
        constexpr rule_letter( base const& rhs ) noexcept
        : base( rhs ) {}
        constexpr rule_letter( char const ( &arr )[2] ) noexcept
        : base{ 1, { arr[0] } } {}
        constexpr rule_letter( char const ( &arr )[3] ) noexcept
        : base{ 2, { arr[0], arr[1] } } {}
        constexpr rule_letter( char const ( &arr )[4] ) noexcept
        : base{ 3, { arr[0], arr[1], arr[2] } } {}

        constexpr operator string_view() const noexcept { return sv(); }

        bool operator==( rule_letter const& rhs ) const noexcept {
            return _b8( *this ) == _b8( rhs );
        }
    };
} // namespace vtz
