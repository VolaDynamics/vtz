#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <gtest/gtest.h>
#include <string_view>
#include <tuple>

namespace _test_vtz {
    using std::string_view;
    template<auto Mem>
    struct Field {
        string_view name;

        template<class T>
        auto operator()( T const& value ) -> decltype( value.*Mem ) {
            return value.*Mem;
        }
    };

    template<class T>
    struct StructInfo {
        constexpr static bool has = false;
    };

    inline void print_indent( std::string& s, size_t indent ) {
        s.resize( s.size() + indent, ' ' );
    }

    template<class T>
    void debug_print( std::string& s, T const& value, size_t indent = 0 );

    template<class T>
    void debug_print(
        std::string& s, std::vector<T> const& values, size_t indent = 0 );

    inline void debug_print(
        std::string& s, std::string_view sv, size_t indent = 0 ) {
        s += '"';
        for( char ch : sv )
        {
            switch( ch )
            {
            case '\0': s += "\\0"; break;
            case '\\': s += "\\\\"; break;
            case '\a': s += "\\a"; break;
            case '\b': s += "\\b"; break;
            case '\f': s += "\\f"; break;
            case '\n': s += "\\n"; break;
            case '\r': s += "\\r"; break;
            case '\t': s += "\\t"; break;
            case '\v': s += "\\v"; break;
            default:
                {
                    uint8_t value( ch );
                    if( value < 32 || value >= 127 )
                    {
                        int  d1 = value >> 4;
                        int  d0 = value & 0xf;
                        char h1
                            = d1 >= 10 ? ( d1 + ( 'A' - 10 ) ) : ( d1 + '0' );
                        char h0
                            = d0 >= 10 ? ( d0 + ( 'A' - 10 ) ) : ( d0 + '0' );

                        char buff[4]{ '\\', 'x', h1, h0 };
                        s.append( buff, 4 );
                    }
                    else { s += ch; }
                }
            }
        }
        s += '"';
    }


    inline void debug_print(
        std::string& dest, std::string const& s, size_t indent = 0 ) {
        return debug_print( dest, string_view( s ), indent );
    }

    inline void debug_print(
        std::string& dest, char const* s, size_t indent = 0 ) {
        return debug_print( dest, string_view( s ), indent );
    }

    template<class T>
    void debug_print_field(
        std::string& s, string_view name, T const& value, size_t indent ) {
        print_indent( s, indent );
        s += ".";
        s += name;
        s += " = ";
        debug_print( s, value, indent );
        s += ",\n";
    }

    template<class T>
    void debug_print(
        std::string& s, std::vector<T> const& values, size_t indent ) {
        auto begin = values.begin();
        auto end   = values.end();
        if( begin == end )
        {
            s += "[]";
            return;
        }

        s += "[\n";
        for( ; begin != end; ++begin )
        {
            auto&& value = *begin;
            print_indent( s, indent + 4 );
            debug_print( s, value, indent + 4 );
            s += ",\n";
        }
        print_indent( s, indent );
        s += "]";
    }
    template<class T>
    void debug_print( std::string& s, T const& value, size_t indent ) {
        if constexpr( StructInfo<T>::has )
        {
            StructInfo<T>::apply( [&]( auto... fields ) {
                s += StructInfo<T>::name;
                s += " {\n";
                ( debug_print_field(
                      s, fields.name, fields( value ), indent + 4 ),
                    ... );
                print_indent( s, indent );
                s += "}";
            } );
        }
        else { s += fmt::format( "{}", value ); }
    }

    template<class T>
    std::string debug_to_string( T const& value ) {
        std::string result;
        debug_print( result, value, 0 );
        return result;
    }

    struct test_printer {
        bool print_on_success = true;
        bool print_expr       = true;
        bool print_location   = true;

        template<class T>
        void info_good( string_view what, T const& value ) {
            auto bg = fmt::emphasis::bold | fmt::fg( fmt::color::green );
            auto bw = fmt::emphasis::bold;
            auto grey
                = fmt::emphasis::faint | fmt::fg( fmt::color::light_gray );
            fmt::println( "{} {}: {}",
                fmt::styled( "[info]", grey ),
                fmt::styled( what, bw ),
                fmt::styled( debug_to_string( value ), bg ) );
        }

        template<class A, class B>
        void print( char const* lhs_expr,
            char const*         rhs_expr,
            A const&            lhs,
            B const&            rhs,
            char const*         filename,
            int                 line ) {
            if( print_expr )
            {
                auto expr_style
                    = fmt::emphasis::faint | fmt::fg( fmt::color::light_gray );
                fmt::print( "{} == {}\n{}",
                    fmt::styled( lhs_expr, expr_style ),
                    fmt::styled( rhs_expr, expr_style ),
                    fmt::styled( "└── ", expr_style ) );
            }

            if( print_location ) { fmt::print( "({}:{}) ", filename, line ); }
            fmt::println( "{} == {}",
                fmt::styled( debug_to_string( lhs ),
                    fmt::emphasis::bold | fmt::fg( fmt::color::green ) ),
                fmt::styled( debug_to_string( rhs ),
                    fmt::emphasis::bold | fmt::fg( fmt::color::green ) ) );
        }
    };


    inline test_printer TEST_LOG{};


    template<class A, class B>
    static auto _eqFail( char const* lhs_expr,
        char const*                  rhs_expr,
        A const&                     lhs,
        B const&                     rhs ) {
        return testing::internal::EqFailure( lhs_expr,
            rhs_expr,
            debug_to_string( lhs ),
            debug_to_string( rhs ),
            false );
    }
} // namespace _test_vtz

#define FIELD( type, member )                                                  \
    _test_vtz::Field<&type::member> { #member }

#define STRUCT_INFO( type, ... )                                               \
    template<>                                                                 \
    struct _test_vtz::StructInfo<type> {                                       \
        constexpr static bool             has  = true;                         \
        constexpr static std::string_view name = #type;                        \
        template<class F>                                                      \
        constexpr static void apply( F&& func ) {                              \
            func( __VA_ARGS__ );                                               \
        }                                                                      \
    };                                                                         \
    template<>                                                                 \
    struct fmt::formatter<type> : fmt::formatter<std::string_view> {           \
        auto format( type const& value, format_context& ctx ) const {          \
            std::string s;                                                     \
            _test_vtz::debug_print( s, value );                                \
            return fmt::formatter<std::string_view>::format( s, ctx );         \
        }                                                                      \
    }

#undef ASSERT_EQ
#define ASSERT_EQ( lhs, rhs )                                                  \
    {                                                                          \
        auto const& _test_lhs = lhs;                                           \
        auto const& _test_rhs = rhs;                                           \
        if( _test_lhs == _test_rhs )                                           \
        {                                                                      \
            if( _test_vtz::TEST_LOG.print_on_success )                         \
                _test_vtz::TEST_LOG.print( #lhs,                               \
                    #rhs,                                                      \
                    _test_lhs,                                                 \
                    _test_rhs,                                                 \
                    __FILE_NAME__,                                             \
                    __LINE__ );                                                \
        }                                                                      \
        else                                                                   \
            return ::testing ::internal ::AssertHelper(                        \
                       ::testing ::TestPartResult ::kFatalFailure,             \
                       __FILE__,                                               \
                       __LINE__,                                               \
                       _test_vtz::_eqFail( #lhs, #rhs, _test_lhs, _test_rhs )  \
                           .failure_message() )                                \
                   = ::testing ::Message();                                    \
    }
