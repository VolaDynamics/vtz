#pragma once


#include <cstddef>
#include <type_traits>
#include <vtz/bit.h>

namespace vtz {

    /// Minimal span implementation

    template<class T>
    class span {
      public:

        VTZ_INLINE constexpr span( T* p, size_t size ) noexcept
        : p( p )
        , count( size ) {}

        VTZ_INLINE constexpr span( T* begin, T* end ) noexcept
        : span( begin, size_t( end - begin ) ) {}

        template<size_t N>
        VTZ_INLINE constexpr span( T ( &arr )[N] ) noexcept
        : span( arr, N ) {}

        // clang-format off
        template<class Range>
        VTZ_INLINE constexpr span( Range&& range ) noexcept( noexcept( span( range.data(), range.size() ) ) )
        : span( range.data(), range.size() ) {}
        // clang-format on

        // clang-format off
        VTZ_INLINE constexpr T*     data() const noexcept { return p; }
        VTZ_INLINE constexpr T*     begin() const noexcept { return p; }
        VTZ_INLINE constexpr T*     end() const noexcept { return p + count; }
        VTZ_INLINE constexpr size_t size() const noexcept { return count; }
        VTZ_INLINE constexpr bool   empty() const noexcept { return count == 0; }
        VTZ_INLINE constexpr T&     operator[]( ptrdiff_t i ) const noexcept { return p[i]; }
        VTZ_INLINE constexpr T&     front() const noexcept { return *p; }
        VTZ_INLINE constexpr T&     back() const noexcept { return p[count - 1]; }
        // clang-format on

        using size_type      = size_t;
        using pointer        = T*;
        using const_pointer  = T const*;
        using iterator       = T*;
        using const_iterator = T const*;
        using value_type     = std::remove_const_t<T>;

      private:

        T*     p;
        size_t count;
    };


    // Template deduction guides for span<T>

    template<class T, size_t N>
    span( T ( &arr )[N] ) -> span<T>;

    template<class T>
    span( T*, size_t ) -> span<T>;

    template<class T>
    span( T*, T* ) -> span<T>;

    template<class Range>
    span( Range&& r ) -> span<std::remove_pointer_t<decltype( r.data() )>>;
} // namespace vtz
