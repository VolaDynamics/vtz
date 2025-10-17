#pragma once

#include <cstdint>
#include <type_traits>

#if __has_builtin( __builtin_memcpy )
    #define _vtz_memcpy __builtin_memcpy
#else
    #include <cstring>
    #define _vtz_memcpy memcpy
#endif

namespace vtz {
    template<class To, class From>
    To bit_cast( From const& from ) noexcept {
        static_assert( std::is_trivially_copyable_v<From>
                       && std::is_trivially_copyable_v<To> );
        static_assert( sizeof( From ) == sizeof( To ) );
        To dest;
        _vtz_memcpy( &dest, &from, sizeof( From ) );
        return dest;
    }

    struct B16 {
        uint64_t a;
        uint64_t b;

        B16() = default;
        template<class Src>
        explicit B16( Src const& s )
        : B16( bit_cast<B16>( s ) ) {
            static_assert( alignof( Src ) == alignof( B16 ),
                "We recommend ensuring your type is properly aligned!" );
        }

        constexpr bool operator==( B16 rhs ) const noexcept {
            return a == rhs.a && b == rhs.b;
        }
        constexpr bool operator!=( B16 rhs ) const noexcept {
            return a != rhs.a || b != rhs.b;
        }
    };
} // namespace vtz
