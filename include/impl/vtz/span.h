#pragma once


#include <cstddef>
#include <type_traits>
#include <vtz/impl/bit.h>

namespace vtz {

    /// Minimal span implementation

    template<class T>
    class span {
      public:

        span() = default;

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
        VTZ_INLINE constexpr size_t size_bytes() const noexcept { return count * sizeof(T); }
        // clang-format on

        using size_type      = size_t;
        using pointer        = T*;
        using const_pointer  = T const*;
        using iterator       = T*;
        using const_iterator = T const*;
        using value_type     = std::remove_const_t<T>;

        /// Return the first element, or the given alternative
        VTZ_INLINE constexpr T const& front_or(
            T const& alternative ) const noexcept {
            return count ? *p : alternative;
        }

        /// Return the last element, or the given alternative
        VTZ_INLINE constexpr T const& back_or(
            T const& alternative ) const noexcept {
            return count ? p[count - 1] : alternative;
        }

        /// Return the value at the given index, if the index is in-bounds,
        /// or the given alternative
        VTZ_INLINE constexpr T const& value_or(
            size_t i, T const& alternative ) const noexcept {
            return i < count ? p[i] : alternative;
        }


        VTZ_INLINE constexpr span subspan( size_t offset ) const noexcept {
            return span( p + offset, count - offset );
        }

      private:

        T*     p     = nullptr;
        size_t count = 0;
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
