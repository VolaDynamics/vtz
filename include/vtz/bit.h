#pragma once

#include <cstdint>
#include <type_traits>
#include <cstddef>

#if __has_builtin( __builtin_memcpy )
    #define _vtz_memcpy __builtin_memcpy
#else
    #include <cstring>
    #define _vtz_memcpy memcpy
#endif

namespace vtz {
    using u8  = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    using i8  = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;

    using uint  = unsigned;
    using usize = size_t;

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
            return !( a == rhs.a && b == rhs.b );
        }
    };
} // namespace vtz


namespace vtz::_impl {
    // clang-format off
    /// Load 1 byte from the input and return the result as a u32
    constexpr u32 _load1( char const* src ) noexcept { return u8( src[0] ); }
    /// Load 2 bytes from the input and return the result as a u32.
    constexpr u32 _load2( char const* src ) noexcept { return u8( src[0] ) | ( u8( src[1] ) << 8 ); }
    /// Load 3 bytes from the input and return the result as a u32.
    constexpr u32 _load3( char const* src ) noexcept { return u8( src[0] ) | ( u8( src[1] ) << 8 ) | ( u8( src[2] ) << 16 ); }
    /// Load 4 bytes from the input and return the result as a u32.
    constexpr u32 _load4( char const* src ) noexcept { return u8( src[0] ) | ( u8( src[1] ) << 8 ) | ( u8( src[2] ) << 16 ) | ( u8( src[3] ) << 24 ); }
    // clang-format on
} // namespace vtz::_impl
