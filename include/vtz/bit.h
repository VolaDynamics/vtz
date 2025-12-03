#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#if __has_builtin( __builtin_memcpy )
    #define _vtz_memcpy __builtin_memcpy
#else
    #include <cstring>
    #define _vtz_memcpy memcpy
#endif

#if _MSC_VER
    #define VTZ_INLINE            __forceinline
    #define VTZ_IS_LIKELY( expr ) bool( expr )
    #define VTZ_UNREACHABLE() __assume(0)
#else
    #define VTZ_INLINE            [[gnu::always_inline]] inline
    #define VTZ_IS_LIKELY( expr ) __builtin_expect( bool( expr ), 1 )
    #define VTZ_UNREACHABLE() __builtin_unreachable()
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

    /// blog2: computes `floor(log2(x))`, returns an integer
    ///
    /// This is a pure-C++ fallback implementation
    constexpr int _blog2_fallback( u64 x ) noexcept {
        int r = 0;
        int p = 0;
        // clang-format off
        p = r + 32; r = x >> p ? p : r;
        p = r + 16; r = x >> p ? p : r;
        p = r + 8;  r = x >> p ? p : r;
        p = r + 4;  r = x >> p ? p : r;
        p = r + 2;  r = x >> p ? p : r;
        p = r + 1;  r = x >> p ? p : r;
        // clang-format on
        return r;
    }


    /// blog2: computes `floor(log2(x))`, returns an integer.
    ///
    /// Uses __builtin_clzll if available
    constexpr int _blog2( u64 x ) noexcept {
#if __has_builtin( __builtin_clzll )
        return 63 - __builtin_clzll( x );
#else
        return _blog2_fallback( x );
#endif
    }


    template<class To, class From>
    To bit_cast( From const& from ) noexcept {
        static_assert( std::is_trivially_copyable_v<From>
                       && std::is_trivially_copyable_v<To> );
        static_assert( sizeof( From ) == sizeof( To ) );
        To dest;
        _vtz_memcpy( &dest, &from, sizeof( From ) );
        return dest;
    }

    struct B8 {
        uint64_t a;

        B8() = default;
        template<class Src>
        explicit B8( Src const& s )
        : B8( bit_cast<B8>( s ) ) {}

        constexpr bool operator==( B8 rhs ) const noexcept {
            return a == rhs.a;
        }
        constexpr bool operator!=( B8 rhs ) const noexcept {
            return a != rhs.a;
        }
    };

    struct B16 {
        uint64_t a, b;

        B16() = default;
        template<class Src>
        explicit B16( Src const& s )
        : B16( bit_cast<B16>( s ) ) {}

        constexpr bool operator==( B16 const& rhs ) const noexcept {
            return a == rhs.a && b == rhs.b;
        }
        constexpr bool operator!=( B16 const& rhs ) const noexcept {
            return !( a == rhs.a && b == rhs.b );
        }
    };

    struct B24 {
        uint64_t a, b, c;

        B24() = default;
        template<class Src>
        explicit B24( Src const& s )
        : B24( bit_cast<B24>( s ) ) {}

        constexpr bool operator==( B24 const& rhs ) const noexcept {
            return a == rhs.a && b == rhs.b && c == rhs.c;
        }
        constexpr bool operator!=( B24 const& rhs ) const noexcept {
            return !( a == rhs.a && b == rhs.b && c == rhs.c );
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
