#pragma once

#include <cstddef>
#include <iterator>
#include <vtz/impl/bit.h>
#include <vtz/span.h>

#include <cstdio>

#if defined( _MSC_VER ) && !defined( __clang__ )

    #include <stdlib.h>
    // https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/byteswap-uint64-byteswap-ulong-byteswap-ushort?view=msvc-170
    #define _vtz_bswap16 _byteswap_ushort
    #define _vtz_bswap32 _byteswap_ulong
    #define _vtz_bswap64 _byteswap_uint64
#else
    #define _vtz_bswap16 __builtin_bswap16
    #define _vtz_bswap32 __builtin_bswap32
    #define _vtz_bswap64 __builtin_bswap64
#endif


namespace vtz::endian {
    enum class endian {
#if defined( _MSC_VER ) && !defined( __clang__ )
        little = 0,
        big    = 1,
        native = little,
#else
        little = __ORDER_LITTLE_ENDIAN__,
        big    = __ORDER_BIG_ENDIAN__,
        native = __BYTE_ORDER__,
#endif
    };


    // clang-format off

    /// Loads p[0..N] as a value of type T
    template<class T> VTZ_INLINE T _load( char const* p ) noexcept { T result; _vtz_memcpy( &result, p, sizeof( T ) ); return result; }

    /// Obtain native u16 from Big Endian u16
    VTZ_INLINE u16 _native_from_be( u16 value ) noexcept { if constexpr( endian::native == endian::big ) { return value; } else { return _vtz_bswap16( value ); } }
    /// Obtain native u32 from Big Endian u32
    VTZ_INLINE u32 _native_from_be( u32 value ) noexcept { if constexpr( endian::native == endian::big ) { return value; } else { return _vtz_bswap32( value ); } }
    /// Obtain native u64 from Big Endian u64
    VTZ_INLINE u64 _native_from_be( u64 value ) noexcept { if constexpr( endian::native == endian::big ) { return value; } else { return _vtz_bswap64( value ); } }

    // Sanity check: ensure i8 is the same size as a char
    static_assert(sizeof(i8) == sizeof(char));

    /// General case - _load_int_be_t isn't implemented for the given type
    template<class T> struct _load_int_be_t { static T load( char const* p ) = delete; };

    /// Special case - i8 is trivial
    template<> struct _load_int_be_t<i8>  { VTZ_INLINE static i8 load( char const* p ) noexcept { return i8( *p ); } };
    /// Special case - u8 is trivial
    template<> struct _load_int_be_t<u8>  { VTZ_INLINE static u8 load( char const* p ) noexcept { return u8( *p ); } };

    template<> struct _load_int_be_t<i16> { VTZ_INLINE static i16 load( char const* p ) noexcept { return i16( _native_from_be( _load<u16>( p ) ) ); } };
    template<> struct _load_int_be_t<i32> { VTZ_INLINE static i32 load( char const* p ) noexcept { return i32( _native_from_be( _load<u32>( p ) ) ); } };
    template<> struct _load_int_be_t<i64> { VTZ_INLINE static i64 load( char const* p ) noexcept { return i64( _native_from_be( _load<u64>( p ) ) ); } };
    template<> struct _load_int_be_t<u16> { VTZ_INLINE static u16 load( char const* p ) noexcept { return _native_from_be( _load<u16>( p ) ); } };
    template<> struct _load_int_be_t<u32> { VTZ_INLINE static u32 load( char const* p ) noexcept { return _native_from_be( _load<u32>( p ) ); } };
    template<> struct _load_int_be_t<u64> { VTZ_INLINE static u64 load( char const* p ) noexcept { return _native_from_be( _load<u64>( p ) ); } };

    /// Interpret p[0..N] as a int with Big Endian byte order, where
    /// N=sizeof(T). Return the corresponding native int of the given type T.
    template<class T> VTZ_INLINE T load_be( char const* p ) noexcept { return _load_int_be_t<T>::load( p ); }


    /// Iterator which uses memcpy to reinterpret raw bytes as elements of type T
    template<class T>
    class byte_iterator {
        constexpr static size_t N = sizeof( T );

      public:

        using self = byte_iterator;

        char const* p = nullptr; ///< Underlying pointer to bytes

        // MUTATING OPERATIONS
        VTZ_INLINE self& operator++() noexcept { return ( p += N ), *this; }
        VTZ_INLINE self& operator--() noexcept { return ( p -= N ), *this; }
        VTZ_INLINE self  operator++( int ) noexcept { auto old = *this; p += N; return old; }
        VTZ_INLINE self  operator--( int ) noexcept { auto old = *this; p -= N; return old; }
        VTZ_INLINE self& operator+=( ptrdiff_t n ) noexcept { return p += n * N, *this; }
        VTZ_INLINE self& operator-=( ptrdiff_t n ) noexcept { return p -= n * N, *this; }

        // CONST OPERATIONS
        VTZ_INLINE self      operator +( ptrdiff_t n ) const noexcept { return { p + n * N }; }
        VTZ_INLINE self      operator -( ptrdiff_t n ) const noexcept { return { p - n * N }; }
        VTZ_INLINE ptrdiff_t operator -( self const& rhs ) const noexcept { return ( p - rhs.p ) / N; }
        VTZ_INLINE bool      operator==( self const& rhs ) const noexcept { return p == rhs.p; }
        VTZ_INLINE bool      operator!=( self const& rhs ) const noexcept { return p != rhs.p; }
        VTZ_INLINE bool      operator <( self const& rhs ) const noexcept { return p  < rhs.p; }
        VTZ_INLINE bool      operator >( self const& rhs ) const noexcept { return p  > rhs.p; }
        VTZ_INLINE bool      operator<=( self const& rhs ) const noexcept { return p <= rhs.p; }
        VTZ_INLINE bool      operator>=( self const& rhs ) const noexcept { return p >= rhs.p; }
        VTZ_INLINE T         operator *() const noexcept { return _load<T>( p ); }
        VTZ_INLINE T         operator[]( ptrdiff_t i ) const noexcept { return _load<T>( p + i * N ); }

        using difference_type   = ptrdiff_t;
        using value_type        = T;
        using pointer           = void;
        using reference         = T;
        using iterator_category = std::random_access_iterator_tag;
    };


    /// Provides iterator over bytes representing sequence of big-endian ints
    template<class T>
    class be_iterator {
        constexpr static size_t N         = sizeof( T );
        constexpr static int    _shr_bits = N == 1   ? 0
                                            : N == 2 ? 1
                                            : N == 4 ? 2
                                            : N == 8 ? 3
                                                     : -1;
        static_assert( _shr_bits != -1, "be_iterator<T>: sizeof(T) must be 1, 2, 4, or 8" );

      public:

        char const* p = nullptr; ///< Underlying pointer to bytes

        // MUTATING OPERATIONS
        VTZ_INLINE be_iterator& operator++() noexcept { return ( p += N ), *this; }
        VTZ_INLINE be_iterator& operator--() noexcept { return ( p -= N ), *this; }
        VTZ_INLINE be_iterator  operator++( int ) noexcept { auto old = *this; p += N; return old; }
        VTZ_INLINE be_iterator  operator--( int ) noexcept { auto old = *this; p -= N; return old; }
        VTZ_INLINE be_iterator& operator+=( ptrdiff_t n ) noexcept { return p += n * N, *this; }
        VTZ_INLINE be_iterator& operator-=( ptrdiff_t n ) noexcept { return p -= n * N, *this; }

        // CONST OPERATIONS
        VTZ_INLINE be_iterator  operator +( ptrdiff_t n ) const noexcept { return { p + n * N }; }
        VTZ_INLINE be_iterator  operator -( ptrdiff_t n ) const noexcept { return { p - n * N }; }
        VTZ_INLINE ptrdiff_t    operator -( be_iterator const& rhs ) const noexcept { return ( p - rhs.p ) >> _shr_bits; }
        VTZ_INLINE bool         operator==( be_iterator const& rhs ) const noexcept { return p == rhs.p; }
        VTZ_INLINE bool         operator!=( be_iterator const& rhs ) const noexcept { return p != rhs.p; }
        VTZ_INLINE bool         operator <( be_iterator const& rhs ) const noexcept { return p  < rhs.p; }
        VTZ_INLINE bool         operator >( be_iterator const& rhs ) const noexcept { return p  > rhs.p; }
        VTZ_INLINE bool         operator<=( be_iterator const& rhs ) const noexcept { return p <= rhs.p; }
        VTZ_INLINE bool         operator>=( be_iterator const& rhs ) const noexcept { return p >= rhs.p; }
        VTZ_INLINE T            operator *() const noexcept { return load_be<T>( p ); }
        VTZ_INLINE T            operator[]( ptrdiff_t i ) const noexcept { return load_be<T>( p + i * N ); }

        using difference_type   = ptrdiff_t;
        using value_type        = T;
        using pointer           = void;
        using reference         = T;
        using iterator_category = std::random_access_iterator_tag;
    };

    template<class T> be_iterator<T> operator+( ptrdiff_t n, be_iterator<T> const& rhs ) noexcept { return rhs.operator+( n ); }

    /// Represents a span over big-endian ints, eg ints read from a file.
    /// Operations such as operator[] return a native int.
    template<class T>
    class span_be {
        char const* p = nullptr;
        size_t      n = 0;

        constexpr static size_t N = sizeof( T );

      public:

        using iterator   = be_iterator<T>;
        using value_type = T;

        span_be() = default;
        span_be( char const* p, size_t count ) noexcept : p( p ), n( count ) {}

        /// Interpret p[i..i+N] as a int with Big Endian byte order, and load it
        /// as a native int
        VTZ_INLINE T        operator[]( ptrdiff_t i ) const noexcept { return load_be<T>( p + i * N ); }

        /// If i is within range, return the value at index i, otherwise
        /// return the given alternative
        VTZ_INLINE T        value_or( size_t i, T const& alt ) const noexcept { return i < n ? operator[](i) : T( alt ); }

        VTZ_INLINE size_t   size()  const noexcept { return n; }
        VTZ_INLINE bool     empty() const noexcept { return n == 0; }
        VTZ_INLINE T        front() const noexcept { return load_be<T>( p ); }
        VTZ_INLINE T        back()  const noexcept { return operator[]( n - 1 ); }
        VTZ_INLINE iterator begin() const noexcept { return { p }; }
        VTZ_INLINE iterator end()   const noexcept { return { p + N * n }; }

        VTZ_INLINE size_t           size_bytes() const noexcept { return n * N; }
        VTZ_INLINE span<char const> bytes()      const noexcept { return span<char const>( p, size_bytes() ); }
    };


    /// Imposes a view over some bytes b such that we can treat it as containing elements T, even if T is not aligned.
    /// Elements are accessed via _load<T>, which uses memcpy to reinterpret the bytes.
    template<class T>
    class span_bytes {
        char const* p = nullptr;
        size_t      n = 0;

        constexpr static size_t N = sizeof( T );

      public:

        using iterator   = byte_iterator<T>;
        using value_type = T;

        span_bytes() = default;
        span_bytes( char const* p, size_t count ) noexcept : p( p ), n( count ) {}

        /// Interpret p[i..i+N] as a int with Big Endian byte order, and load it
        /// as a native int
        VTZ_INLINE T        operator[]( ptrdiff_t i ) const noexcept { return _load<T>( p + i * N ); }

        VTZ_INLINE size_t   size()  const noexcept { return n; }
        VTZ_INLINE bool     empty() const noexcept { return n == 0; }
        VTZ_INLINE T        front() const noexcept { return _load<T>( p ); }
        VTZ_INLINE T        back()  const noexcept { return operator[]( n - 1 ); }
        VTZ_INLINE iterator begin() const noexcept { return { p }; }
        VTZ_INLINE iterator end()   const noexcept { return { p + N * n }; }

        VTZ_INLINE size_t           size_bytes() const noexcept { return n * N; }
        VTZ_INLINE span<char const> bytes()      const noexcept { return span<char const>( p, size_bytes() ); }
    };
    // clang-format on
} // namespace vtz::endian
