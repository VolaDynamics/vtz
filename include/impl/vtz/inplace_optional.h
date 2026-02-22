#pragma once


#include <type_traits>


namespace vtz {
    /// Implements an optional-like wrapper over a type, where some _value_
    /// of the type represents the 'null' value.
    template<class T, T NULL_VALUE>
    struct opt_val {
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
        constexpr bool operator==( opt_val const& rhs ) const noexcept { return data == rhs.data; }
        constexpr bool operator!=( opt_val const& rhs ) const noexcept { return data != rhs.data; }
        constexpr bool operator<=( opt_val const& rhs ) const noexcept { return data <= rhs.data; }
        constexpr bool operator>=( opt_val const& rhs ) const noexcept { return data >= rhs.data; }
        constexpr bool operator< ( opt_val const& rhs ) const noexcept { return data <  rhs.data; }
        constexpr bool operator> ( opt_val const& rhs ) const noexcept { return data >  rhs.data; }
        // clang-format on
    };

    template<class T>
    struct opt_traits;

    template<class T>
    struct opt_class {
        constexpr static T NULL_VALUE = opt_traits<T>::NULL_VALUE;

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
        constexpr bool operator==( opt_class const& rhs ) const noexcept { return data == rhs.data; }
        constexpr bool operator!=( opt_class const& rhs ) const noexcept { return data != rhs.data; }
        constexpr bool operator<=( opt_class const& rhs ) const noexcept { return data <= rhs.data; }
        constexpr bool operator>=( opt_class const& rhs ) const noexcept { return data >= rhs.data; }
        constexpr bool operator< ( opt_class const& rhs ) const noexcept { return data <  rhs.data; }
        constexpr bool operator> ( opt_class const& rhs ) const noexcept { return data >  rhs.data; }
        // clang-format on
    };

    template<class T>
    struct trivial_opt {
        static_assert( std::is_trivial<T>{} );
        T    data;
        bool good;

        constexpr T&       operator*() noexcept { return data; }
        constexpr T const& operator*() const noexcept { return data; }
        constexpr T&       value() noexcept { return data; }
        constexpr T const& value() const noexcept { return data; }

        constexpr T const& value_or( T const& input ) const noexcept {
            return good ? data : input;
        }

        constexpr bool has_value() const noexcept { return good; }

        constexpr explicit operator bool() const noexcept { return good; }
    };

    struct none_type {
        template<class T>
        constexpr operator opt_class<T>() const noexcept {
            return { opt_traits<T>::NULL_VALUE };
        }
        template<class T>
        constexpr operator trivial_opt<T>() const noexcept {
            return {};
        }

        template<class T, T NULL_VALUE>
        constexpr operator opt_val<T, NULL_VALUE>() const noexcept {
            return { NULL_VALUE };
        }
    };


    /// Holds a value, implicitly convertible to an OptV
    template<class T>
    struct some_type {
        T value;

        constexpr operator opt_class<T>() const noexcept { return { value }; }
        constexpr operator trivial_opt<T>() const noexcept {
            return { value, true };
        }

        template<T NULL_VALUE>
        constexpr operator opt_val<T, NULL_VALUE>() const noexcept {
            return { value };
        }
    };

    constexpr inline none_type none{};

    template<class T>
    constexpr inline some_type<T> some( T value ) noexcept {
        return { value };
    }


} // namespace vtz
