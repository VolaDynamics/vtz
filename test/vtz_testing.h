#include "gtest/gtest.h"
#include <vtz/strings.h>

#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <ankerl/unordered_dense.h>

#include <gtest/gtest.h>
#include <string_view>
#include <tuple>

#define DECLARE_STRINGLIKE( type )                                                                 \
    namespace _test_vtz {                                                                          \
        template<>                                                                                 \
        struct DebugTraits<type> {                                                                 \
            constexpr static bool stringlike = true;                                               \
        };                                                                                         \
    }

namespace _test_vtz {
    template<class T>
    struct DebugTraits {
        constexpr static bool stringlike = false;
    };
    template<size_t N>
    struct DebugTraits<char const ( & )[N]> {
        constexpr static bool stringlike = true;
    };
} // namespace _test_vtz

DECLARE_STRINGLIKE( std::string );
DECLARE_STRINGLIKE( std::string_view );
DECLARE_STRINGLIKE( char const* );

namespace _test_vtz {
    constexpr auto BOLD_GREEN = fmt::emphasis::bold | fmt::fg( fmt::color::green );
    constexpr auto BOLD_RED   = fmt::emphasis::bold | fmt::fg( fmt::color::red );
    constexpr auto FAINT_GRAY = fmt::emphasis::faint | fmt::fg( fmt::color::light_gray );


    struct CountAssertionsNOOP {
        constexpr void inc() const noexcept {}
        constexpr void incFailed() const noexcept {};
    };

    struct CountAssertions {
        std::string_view context;
        size_t           count  = 0;
        size_t           failed = 0;

        void inc() noexcept { ++count; }
        void incFailed() noexcept { ++failed; }

        ~CountAssertions() {
            if( failed )
            {
                fmt::println( "{}\n└── {} Assertions Checked - {}",
                    fmt::styled( context, FAINT_GRAY ),
                    count,
                    fmt::styled( fmt::format( "{} assertions failed" ), BOLD_RED ) );
            }
            else
            {
                fmt::println( "{}\n└── {} Assertions Checked! (0 failed)",
                    fmt::styled( context, FAINT_GRAY ),
                    fmt::styled( count, BOLD_GREEN ) );
            }
        }
    };

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

    inline void printIndent( std::string& s, size_t indent ) { s.resize( s.size() + indent, ' ' ); }

    template<class T>
    void debugPrint( std::string& s, T const& value, size_t indent = 0 );

    template<class T>
    void debugPrint( std::string& s, std::vector<T> const& values, size_t indent = 0 );

    template<class K, class V>
    void debugPrint(
        std::string& s, ankerl::unordered_dense::map<K, V> const& values, size_t indent );


    inline void debugPrint( std::string& dest, std::string const& s, size_t indent = 0 ) {
        return debugPrint( dest, string_view( s ), indent );
    }

    inline void debugPrint( std::string& dest, char const* s, size_t indent = 0 ) {
        return debugPrint( dest, string_view( s ), indent );
    }

    template<class T>
    void debugPrintField( std::string& s, string_view name, T const& value, size_t indent ) {
        printIndent( s, indent );
        s += ".";
        s += name;
        s += " = ";
        debugPrint( s, value, indent );
        s += ",\n";
    }

    template<class T>
    void debugPrint( std::string& s, std::vector<T> const& values, size_t indent ) {
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
            printIndent( s, indent + 4 );
            debugPrint( s, value, indent + 4 );
            s += ",\n";
        }
        printIndent( s, indent );
        s += "]";
    }


    template<class K, class V>
    void debugPrint(
        std::string& s, ankerl::unordered_dense::map<K, V> const& values, size_t indent ) {
        auto begin = values.begin();
        auto end   = values.end();
        if( begin == end )
        {
            s += "{}";
            return;
        }

        s += "{\n";
        for( ; begin != end; ++begin )
        {
            auto&& [k, v] = *begin;
            printIndent( s, indent + 4 );
            debugPrint( s, k, indent + 4 );
            s += ": ";
            debugPrint( s, v, indent + 4 );
            s += ",\n";
        }
        printIndent( s, indent );
        s += "}";
    }


    template<class T>
    void debugPrint( std::string& s, T const& value, size_t indent ) {
        if constexpr( StructInfo<T>::has )
        {
            StructInfo<T>::apply( [&]( auto... fields ) {
                s += StructInfo<T>::name;
                s += " {\n";
                ( debugPrintField( s, fields.name, fields( value ), indent + 4 ), ... );
                printIndent( s, indent );
                s += "}";
            } );
        }
        else if constexpr( DebugTraits<T>::stringlike )
            s += vtz::escapeString( std::string_view( value ) );
        else
            s += fmt::format( "{}", value );
    }

    template<class T>
    std::string debugToString( T const& value ) {
        std::string result;
        debugPrint( result, value, 0 );
        return result;
    }

    struct TestPrinter {
        bool print_on_success = true;
        bool print_expr       = true;
        bool print_location   = true;

        template<class T>
        void logGood( string_view what, T const& value ) {
            auto bg = fmt::emphasis::bold | fmt::fg( fmt::color::green );
            auto bw = fmt::emphasis::bold;
            ;
            fmt::println( "{} {}: {}",
                fmt::styled( "[info]", FAINT_GRAY ),
                fmt::styled( what, bw ),
                fmt::styled( debugToString( value ), bg ) );
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
                auto expr_style = fmt::emphasis::faint | fmt::fg( fmt::color::light_gray );
                fmt::print( "{} == {}\n{}",
                    fmt::styled( lhs_expr, expr_style ),
                    fmt::styled( rhs_expr, expr_style ),
                    fmt::styled( "└── ", expr_style ) );
            }

            if( print_location ) { fmt::print( "({}:{}) ", filename, line ); }
            fmt::println( "{} == {}",
                fmt::styled(
                    debugToString( lhs ), fmt::emphasis::bold | fmt::fg( fmt::color::green ) ),
                fmt::styled(
                    debugToString( rhs ), fmt::emphasis::bold | fmt::fg( fmt::color::green ) ) );
        }
    };


    inline TestPrinter TEST_LOG{};


    template<class A, class B>
    static auto _eqFail( char const* lhs_expr, char const* rhs_expr, A const& lhs, B const& rhs ) {
        return testing::internal::EqFailure(
            lhs_expr, rhs_expr, debugToString( lhs ), debugToString( rhs ), false );
    }

    template<class LHS, class RHS, class TestCountAssertions>
    bool _doAssertEq( LHS const& _test_lhs,
        RHS const&               _test_rhs,
        char const*              lhs,
        char const*              rhs,
        bool                     print_on_success,
        char const*              filename,
        int                      lineno,
        TestCountAssertions&&    _test_count_assertions ) {
        _test_count_assertions.inc();
        if( _test_lhs == _test_rhs )
        {
            if( print_on_success )
                _test_vtz::TEST_LOG.print( lhs, rhs, _test_lhs, _test_rhs, filename, lineno );
            return true;
        }
        else
        {
            _test_count_assertions.incFailed();
            ::testing::internal::AssertHelper( ::testing::TestPartResult::kFatalFailure,
                filename,
                lineno,
                _test_vtz::_eqFail( lhs, rhs, _test_lhs, _test_rhs ).failure_message() )
                = testing::Message();
            return false;
        }
    }
} // namespace _test_vtz

constexpr inline _test_vtz::CountAssertionsNOOP _test_count_assertions{};

#define COUNT_ASSERTIONS()                                                                         \
    _test_vtz::CountAssertions _test_count_assertions { __func__ }


#define FIELD( type, member )                                                                      \
    _test_vtz::Field<&type::member> { #member }

#define STRUCT_INFO( type, ... )                                                                   \
    template<>                                                                                     \
    struct _test_vtz::StructInfo<type> {                                                           \
        constexpr static bool             has  = true;                                             \
        constexpr static std::string_view name = #type;                                            \
        template<class F>                                                                          \
        constexpr static void apply( F&& func ) {                                                  \
            func( __VA_ARGS__ );                                                                   \
        }                                                                                          \
    };                                                                                             \
    template<>                                                                                     \
    struct fmt::formatter<type> : fmt::formatter<std::string_view> {                               \
        auto format( type const& value, format_context& ctx ) const {                              \
            std::string s;                                                                         \
            _test_vtz::debugPrint( s, value );                                                     \
            return fmt::formatter<std::string_view>::format( s, ctx );                             \
        }                                                                                          \
    }

#undef ASSERT_EQ
#define ASSERT_EQ( lhs, rhs )                                                                      \
    if( !_test_vtz::_doAssertEq( lhs,                                                              \
            rhs,                                                                                   \
            #lhs,                                                                                  \
            #rhs,                                                                                  \
            _test_vtz::TEST_LOG.print_on_success,                                                  \
            __FILE__,                                                                              \
            __LINE__,                                                                              \
            _test_count_assertions ) )                                                             \
    return

#define ASSERT_EQ_QUIET( lhs, rhs )                                                                \
    if( !_test_vtz::_doAssertEq(                                                                   \
            lhs, rhs, #lhs, #rhs, false, __FILE__, __LINE__, _test_count_assertions ) )            \
    return
