#pragma once

#include <vtz/strings.h>


namespace vtz {
    struct alignas( 8 ) rule_letter : fix_str<7> {
        using FixStr = fix_str<7>;

        using FixStr::FixStr;

        rule_letter() = default;
        constexpr rule_letter( FixStr const& rhs ) noexcept
        : FixStr( rhs ) {}
        constexpr rule_letter( char const ( &arr )[2] ) noexcept
        : FixStr{ 1, { arr[0] } } {}
        constexpr rule_letter( char const ( &arr )[3] ) noexcept
        : FixStr{ 2, { arr[0], arr[1] } } {}
        constexpr rule_letter( char const ( &arr )[4] ) noexcept
        : FixStr{ 3, { arr[0], arr[1], arr[2] } } {}

        constexpr operator string_view() const noexcept { return sv(); }

        bool operator==( rule_letter const& rhs ) const noexcept {
            return _b8( *this ) == _b8( rhs );
        }
    };
} // namespace vtz
