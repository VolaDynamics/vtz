#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <limits>
#include <vtz/impl/macros.h>

namespace vtz::util {
    template<class T>
    struct fmt_traits {};

    template<class IntT>
    struct fmt_traits_int {
        constexpr static size_t _max_space
            = 1 + std::numeric_limits<IntT>::digits10 + std::is_signed<IntT>{};

            static size_t max_space( IntT ) noexcept { return _max_space; }

        static size_t dump( char* dest, IntT value ) {
            auto result = std::to_chars( dest, dest + _max_space, value );
            return result.ptr - dest;
        }
    };

    static_assert( fmt_traits_int<uint32_t>::_max_space == 10 );
    static_assert( fmt_traits_int<int32_t>::_max_space == 11 );

    // Dump string literal
    template<size_t N>
    struct fmt_traits<char[N]> {
        static size_t max_space( char const ( & )[N] ) { return N - 1; }
        static size_t dump( char* dest, char const ( &s )[N] ) {
            _vtz_memcpy( dest, s, N - 1 );
            return N - 1;
        }
    };

    // Dump string view
    template<>
    struct fmt_traits<std::string_view> {
        static size_t max_space( std::string_view s ) { return s.size(); }
        static size_t dump( char* dest, std::string_view s ) {
            _vtz_memcpy( dest, s.data(), s.size() );
            return s.size();
        }
    };

    // Dump individual character
    template<>
    struct fmt_traits<char> {
        static size_t max_space( char ) { return 1; }
        static size_t dump( char* dest, char ch ) { return (void)( *dest = ch ), 1; }
    };

    // clang-format off
    template<> struct fmt_traits<signed char> : fmt_traits_int<signed char> {};
    template<> struct fmt_traits<short> : fmt_traits_int<short> {};
    template<> struct fmt_traits<int> : fmt_traits_int<int> {};
    template<> struct fmt_traits<long> : fmt_traits_int<long> {};
    template<> struct fmt_traits<long long> : fmt_traits_int<long long> {};
    template<> struct fmt_traits<unsigned char> : fmt_traits_int<unsigned char> {};
    template<> struct fmt_traits<unsigned short> : fmt_traits_int<unsigned short> {};
    template<> struct fmt_traits<unsigned int> : fmt_traits_int<unsigned int> {};
    template<> struct fmt_traits<unsigned long> : fmt_traits_int<unsigned long> {};
    template<> struct fmt_traits<unsigned long long> : fmt_traits_int<unsigned long long> {};
    template<> struct fmt_traits<std::string> : fmt_traits<std::string_view> {};
    template<> struct fmt_traits<char const*> : fmt_traits<std::string_view> {};
    // clang-format on


    template<class T>
    size_t max_space( T const& e ) {
        return fmt_traits<T>::max_space( e );
    }

    template<class T>
    size_t dump( char* dest, T const& e ) {
        return fmt_traits<T>::dump( dest, e );
    }

    template<class... T>
    std::string join( T const&... tt ) {
        constexpr static size_t BUFF_SIZE = 16384;

        size_t space = ( max_space( tt ) + ... + 0 );

        if( space <= BUFF_SIZE )
        {
            char  stack_buff[BUFF_SIZE];
            char* p = stack_buff;
            ( ( p += dump( p, tt ) ), ... );
            return std::string( stack_buff, size_t( p - stack_buff ) );
        }
        else
        {
            auto  result = std::string( space, '\0' );
            char* p      = result.data();
            ( ( p += dump( p, tt ) ), ... );
            result.resize( size_t( p - result.data() ) );
            return result;
        }
    }


} // namespace vtz::util
