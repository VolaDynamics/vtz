#pragma once


#include <type_traits>


namespace vtz {
    /// Implements an optional-like wrapper over a type, where some _value_
    /// of the type represents the 'null' value.
    template<class T, T NULL_VALUE>
    struct OptV {
        static_assert( std::is_trivial<T>::value, "Type must be trivial" );

        T data = NULL_VALUE;

        constexpr T&       operator*() noexcept { return data; }
        constexpr T const& operator*() const noexcept { return data; }
        constexpr T&       value() noexcept { return data; }
        constexpr T const& value() const noexcept { return data; }

        constexpr T const& value_or( T const& input ) const noexcept {
            return data == NULL_VALUE ? input : data;
        }

        constexpr bool has_value() const noexcept { return data != NULL_VALUE; }

        constexpr explicit operator bool() const noexcept {
            return data != NULL_VALUE;
        }

        // clang-format off
        constexpr bool operator==( OptV const& rhs ) const noexcept { return data == rhs.data; }
        constexpr bool operator!=( OptV const& rhs ) const noexcept { return data != rhs.data; }
        constexpr bool operator<=( OptV const& rhs ) const noexcept { return data <= rhs.data; }
        constexpr bool operator>=( OptV const& rhs ) const noexcept { return data >= rhs.data; }
        constexpr bool operator< ( OptV const& rhs ) const noexcept { return data <  rhs.data; }
        constexpr bool operator> ( OptV const& rhs ) const noexcept { return data >  rhs.data; }
        // clang-format on
    };

    struct NoneType {
        template<class T, T NULL_VALUE>
        constexpr operator OptV<T, NULL_VALUE>() const noexcept {
            return { NULL_VALUE };
        }
    };

    /// Holds a value, implicitly convertible to an OptV
    template<class T>
    struct SomeType {
        T value;

        template<T NULL_VALUE>
        constexpr operator OptV<T, NULL_VALUE>() const noexcept {
            return { value };
        }
    };

    constexpr inline NoneType none{};

    template<class T>
    constexpr inline SomeType<T> some( T value ) noexcept {
        return { value };
    }
} // namespace vtz
