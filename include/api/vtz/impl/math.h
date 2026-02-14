#pragma once

#include <vtz/impl/bit.h>

namespace vtz::math {
    template<class Int>
    struct div_t {
        Int quot;
        Int rem;

        constexpr bool operator==( div_t const& rhs ) const noexcept {
            return quot == rhs.quot && rem == rhs.rem;
        }
        constexpr bool operator!=( div_t const& rhs ) const noexcept {
            return !( *this == rhs );
        }
    };
    template<class Int>
    div_t( Int, Int ) -> div_t<Int>;

    /// Computes the quotient and remainder using floor division.
    ///
    /// The remainder will always be positive after this operation
    template<auto Divisor, class T>
    VTZ_INLINE constexpr div_t<T> div_floor2( T k ) noexcept {
        static_assert( Divisor > 0 );
        auto m = k % Divisor;
        auto q = k / Divisor;

        bool m_neg = m < 0;

        return {
            q - m_neg,
            m + ( m_neg ? Divisor : 0 ),
        };
    }

    /// Returns the (positive) remainder after division by the given divisor
    template<auto Divisor, class T>
    VTZ_INLINE constexpr T rem( T k ) noexcept {
        auto m    = k % Divisor;
        bool m_neg = m < 0;
        return T( m + ( m_neg ? Divisor : 0 ) );
    }


    /// Computes k / Divisor using floor division
    ///
    /// This is floor(k / Divisor).
    template<i32 Divisor>
    VTZ_INLINE constexpr i32 div_floor( i32 k ) noexcept {
        static_assert( Divisor > 0 );
        bool is_neg = k < 0;
        return ( ( k + is_neg ) / Divisor ) - is_neg;
    }

    /// Computes k / Divisor using floor division
    ///
    /// This is floor(k / Divisor).
    template<i64 Divisor>
    VTZ_INLINE constexpr i64 div_floor( i64 k ) noexcept {
        static_assert( Divisor > 0 );
        bool is_neg = k < 0;
        return ( ( k + is_neg ) / Divisor ) - is_neg;
    }
} // namespace vtz::math
