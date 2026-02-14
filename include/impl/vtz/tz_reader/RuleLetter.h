#pragma once

#include <vtz/strings.h>


namespace vtz {
    struct alignas( 8 ) RuleLetter : FixStr<7> {
        using FixStr = FixStr<7>;

        using FixStr::FixStr;

        RuleLetter() = default;
        constexpr RuleLetter( FixStr const& rhs ) noexcept
        : FixStr( rhs ) {}
        constexpr RuleLetter( char const ( &arr )[2] ) noexcept
        : FixStr{ 1, { arr[0] } } {}
        constexpr RuleLetter( char const ( &arr )[3] ) noexcept
        : FixStr{ 2, { arr[0], arr[1] } } {}
        constexpr RuleLetter( char const ( &arr )[4] ) noexcept
        : FixStr{ 3, { arr[0], arr[1], arr[2] } } {}

        constexpr operator string_view() const noexcept { return sv(); }

        bool operator==( RuleLetter const& rhs ) const noexcept {
            return B8( *this ) == B8( rhs );
        }
    };
} // namespace vtz
