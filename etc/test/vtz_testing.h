#pragma once


#include <algorithm>
#include <gtest/gtest.h>
#include <ostream>
#include <vector>
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

constexpr auto BOLD        = fmt::emphasis::bold;
constexpr auto BOLD_GREEN  = fmt::emphasis::bold | fmt::fg( fmt::color::green );
constexpr auto BOLD_RED    = fmt::emphasis::bold | fmt::fg( fmt::color::red );
constexpr auto BOLD_CYAN   = fmt::emphasis::bold | fmt::fg( fmt::color::cyan );
constexpr auto BOLD_BLUE   = fmt::emphasis::bold | fmt::fg( fmt::color::blue );
constexpr auto BOLD_YELLOW = fmt::emphasis::bold | fmt::fg( fmt::color::yellow );
constexpr auto GREEN       = fmt::fg( fmt::color::green );
constexpr auto RED         = fmt::fg( fmt::color::red );
constexpr auto CYAN        = fmt::fg( fmt::color::cyan );
constexpr auto BLUE        = fmt::fg( fmt::color::blue );
constexpr auto YELLOW      = fmt::fg( fmt::color::yellow );
constexpr auto FAINT_RED   = fmt::emphasis::faint | fmt::fg( fmt::color::red );
constexpr auto FAINT_GRAY  = fmt::emphasis::faint | fmt::fg( fmt::color::light_gray );

namespace _test_vtz {
    using std::vector;

    struct TestContextI {
        TestContextI const* prev = nullptr;
        char const*         header;

        constexpr TestContextI( TestContextI const* prev, char const* header ) noexcept
        : prev( prev )
        , header( header ) {}

        virtual std::string print_frame( size_t indent ) const = 0;

        std::string context() const {
            auto        ctx = this;
            std::string extra_ctx;
            extra_ctx += "\n\nAdditional context for failure:\n\n";
            while( ctx )
            {
                extra_ctx += fmt::format(
                    "{}\n└── {}\n", fmt::styled( ctx->header, FAINT_GRAY ), ctx->print_frame( 4 ) );

                ctx = ctx->prev;
            }
            return extra_ctx;
        }
    };

    template<class F>
    struct TestContext : TestContextI {
        F func;

        TestContext( TestContextI const* prev, char const* header, F&& func )
        : TestContextI( prev, header )
        , func( std::move( func ) ) {}

        std::string print_frame( size_t indent ) const override { return func( indent ); }
    };
    template<class F>
    TestContext( TestContextI const* prev, char const*, F func ) -> TestContext<F>;


    struct NoopContext {
        constexpr TestContextI const* operator&() const noexcept { return nullptr; }
    };

    constexpr NoopContext _current_test_context;

    inline std::ostream& operator<<( std::ostream& msg, TestContextI const& ctx ) {
        return msg << ctx.context();
    }

    inline std::ostream& operator<<( std::ostream& msg, NoopContext ) { return msg; }

    template<class Enum>
    std::string enum_to_string( std::string_view type_name, Enum value ) {
        static_assert( std::is_enum_v<Enum> );
        return fmt::format( "{}({})", type_name, std::int64_t( value ) );
    }

    struct CountAssertionsNOOP {
        constexpr void inc() const noexcept {}
        constexpr void inc_failed() const noexcept {};
    };

    struct CountAssertions {
        std::string_view context;
        size_t           count  = 0;
        size_t           failed = 0;

        void inc() noexcept { ++count; }
        void inc_failed() noexcept { ++failed; }

        ~CountAssertions() {
            if( failed )
            {
                fmt::println( "{}\n└── {} Assertions Checked - {}",
                    fmt::styled( context, FAINT_GRAY ),
                    count,
                    fmt::styled( fmt::format( "{} assertions failed", failed ), BOLD_RED ) );
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

    inline void print_indent( std::string& s, size_t indent ) {
        s.resize( s.size() + indent, ' ' );
    }

    template<class T>
    void debug_print( std::string& s, T const& value, size_t indent = 0 );

    template<class T>
    void debug_print( std::string& s, std::vector<T> const& values, size_t indent = 0 );

    template<class K, class V>
    void debug_print(
        std::string& s, ankerl::unordered_dense::map<K, V> const& values, size_t indent );


    inline void debug_print( std::string& dest, std::string const& s, size_t indent = 0 ) {
        return debug_print( dest, string_view( s ), indent );
    }

    inline void debug_print( std::string& dest, char const* s, size_t indent = 0 ) {
        return debug_print( dest, string_view( s ), indent );
    }

    template<class T>
    void debug_print_field( std::string& s, string_view name, T const& value, size_t indent ) {
        print_indent( s, indent );
        s += ".";
        s += name;
        s += " = ";
        debug_print( s, value, indent );
        s += ",\n";
    }

    template<class T>
    void debug_print( std::string& s, T const* p, size_t ) {
        s += fmt::format( "{}", (void const*)p );
    }

    template<class T>
    void debug_print( std::string& s, std::vector<T> const& values, size_t indent ) {
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


    template<class K, class V>
    void debug_print(
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
            print_indent( s, indent + 4 );
            debug_print( s, k, indent + 4 );
            s += ": ";
            debug_print( s, v, indent + 4 );
            s += ",\n";
        }
        print_indent( s, indent );
        s += "}";
    }


    template<class T>
    void debug_print( std::string& s, T const& value, size_t indent ) {
        if constexpr( StructInfo<T>::has )
        {
            StructInfo<T>::apply( [&]( auto... fields ) {
                s += StructInfo<T>::name;
                s += " {\n";
                ( debug_print_field( s, fields.name, fields( value ), indent + 4 ), ... );
                print_indent( s, indent );
                s += "}";
            } );
        }
        else if constexpr( DebugTraits<T>::stringlike )
            s += vtz::escape_string( std::string_view( value ) );
        else
            s += fmt::format( "{}", value );
    }

    template<class T>
    std::string debug_to_string( T const& value ) {
        std::string result;
        debug_print( result, value, 0 );
        return result;
    }

    struct TestPrinter {
        bool print_on_success = true;
        bool print_expr       = true;
        bool print_location   = true;

        template<class T>
        void log_good( string_view what, T const& value ) {
            fmt::println( "{} {}: {}",
                fmt::styled( "[info]", FAINT_GRAY ),
                fmt::styled( what, BOLD ),
                fmt::styled( debug_to_string( value ), BOLD_GREEN ) );
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
                    debug_to_string( lhs ), fmt::emphasis::bold | fmt::fg( fmt::color::green ) ),
                fmt::styled(
                    debug_to_string( rhs ), fmt::emphasis::bold | fmt::fg( fmt::color::green ) ) );
        }
    };


    inline TestPrinter TEST_LOG{};


    /// Find the enclosing parenthesis (returns one past the paren, or the 'end' if no paren is
    /// found)
    constexpr char const* find_enclosing_paren( char const* p, char const* end ) noexcept {
        int depth = 1;
        for( ; p != end; ++p )
        {
            char ch = *p;
            if( ch == '(' )
            {
                // Go deeper - we still need a closing paren
                ++depth;
            }
            else if( ch == ')' )
            {
                // Decrement depth - things are good
                --depth;
                // We found the last paren
                if( depth == 0 ) { return p + 1; }
            }
        }
        return end;
    }

    inline vector<string_view> split_macro_args( string_view args ) {
        char const* p   = args.data();
        char const* p0  = p;
        char const* end = p + args.size();
        if( p == end ) return vector<string_view>();

        vector<string_view> result;
        while( p != end )
        {
            if( *p == ',' )
            {
                result.emplace_back( p0, size_t( p - p0 ) );
                ++p;
                p0 = p;
                continue;
            }
            else if( *p == '(' )
            {
                // p will be set to either end, or one past the enclosing parenthesis
                p = find_enclosing_paren( p + 1, end );
            }
            else
            {
                // p is not a special character - just increment it
                ++p;
            }
        }

        result.emplace_back( p0, size_t( p - p0 ) );
        for( auto& s : result )
        {
            char const* p   = s.data();
            char const* end = p + s.size();
            while( p != end && *p == ' ' ) ++p;
            s = string_view( p, size_t( end - p ) );
        }
        return result;
    }

    inline std::string print_values( size_t ident, string_view macro_args = "" ) {
        return "(no additional info)";
    }

    template<class First, class... T>
    std::string print_values(
        size_t indent, string_view macro_args, First&& first, T&&... values ) {
        auto args = split_macro_args( macro_args );
        if( args.size() != sizeof...( values ) + 1 )
        {
            throw std::runtime_error(
                fmt::format( "print_values(): '{}' contained too many or too few arguments "
                             "(expected {} args), split was [{}]",
                    macro_args,
                    sizeof...( values ),
                    fmt::join( args, ", " ) ) );
        }

        size_t max_len = 0;
        for( auto const& ar : args )
        {
            if( ar.size() > max_len ) max_len = ar.size();
        }
        std::string s;
        auto        pad         = std::string( max_len, ' ' );
        auto        ind         = std::string( indent, ' ' );
        auto        print_value = [&, i = 0]( auto const& value ) mutable {
            s += args[i];
            s += string_view( pad.data(), pad.size() - args[i].size() );
            s += " = ";
            debug_print( s, value );
            ++i;
        };

        print_value( first );


        ( ( ( s += '\n', s += ind ), print_value( values ) ), ... );
        return s;
    }

    struct _do_print_values {
        std::string result;
        template<class... Args>
        _do_print_values( Args&&... args )
        : result( print_values( static_cast<Args&&>( args )... ) ) {}
    };

    struct CmpBadInfo {
        std::string lhs_;
        std::string rhs_;
    };

    static auto _eq_fail( char const* lhs_expr, char const* rhs_expr, CmpBadInfo& info ) {
        return testing::internal::EqFailure( lhs_expr, rhs_expr, info.lhs_, info.rhs_, false );
    }


    struct CmpEq {
        CmpBadInfo* info = nullptr;

        CmpEq( CmpEq const& ) = delete;
        CmpEq( CmpEq&& )      = delete;


        template<class A, class B>
        CmpEq( A const& a, B const& b )
        : info{
            a == b
                // if the compare is good, there is no error info
                ? nullptr
                // If the comparison is bad, store the lhs and rhs converted to their debug repr
                : new CmpBadInfo{ debug_to_string( a ), debug_to_string( b ) },
        } {}

        template<class A, class B>
        CmpEq( A const& a,
            B const&    b,
            char const* lhs_expr,
            char const* rhs_expr,
            char const* filename,
            int         lineno )
        : CmpEq{ a, b } {
            if( _test_vtz::TEST_LOG.print_on_success )
                _test_vtz::TEST_LOG.print( lhs_expr, rhs_expr, a, b, filename, lineno );
        }

        explicit operator bool() const noexcept { return info == nullptr; }

        ~CmpEq() { delete info; }
    };
} // namespace _test_vtz

using _test_vtz::_current_test_context;

#define ADD_CONTEXT( header, ... )                                                                 \
    auto _test_context_ptr = &_current_test_context;                                               \
    auto _current_test_context                                                                     \
        = _test_vtz::TestContext( _test_context_ptr, header, [&]( size_t indent ) {                \
              return _test_vtz::_do_print_values{ indent, #__VA_ARGS__, __VA_ARGS__ }.result;      \
          } )


constexpr inline _test_vtz::CountAssertionsNOOP _test_count_assertions{};

#define FMT_ENUM_PLAIN( type )                                                                     \
    template<>                                                                                     \
    struct fmt::formatter<type> : fmt::formatter<std::string> {                                    \
        auto format( type const& value, format_context& ctx ) const {                              \
            return fmt::formatter<std::string>::format(                                            \
                _test_vtz::enum_to_string( #type, value ), ctx );                                  \
        }                                                                                          \
    }

#define COUNT_ASSERTIONS()                                                                         \
    _test_vtz::CountAssertions _test_count_assertions { __func__ }


#define FIELD( member )                                                                            \
    _test_vtz::Field<&type::member> { #member }

#define STRUCT_INFO( TYPE, ... )                                                                   \
    template<>                                                                                     \
    struct _test_vtz::StructInfo<TYPE> {                                                           \
        constexpr static bool             has  = true;                                             \
        constexpr static std::string_view name = #TYPE;                                            \
        using type                             = TYPE;                                             \
        template<class F>                                                                          \
        constexpr static void apply( F&& func ) {                                                  \
            func( __VA_ARGS__ );                                                                   \
        }                                                                                          \
    };                                                                                             \
    template<>                                                                                     \
    struct fmt::formatter<TYPE> : fmt::formatter<std::string_view> {                               \
        auto format( TYPE const& value, format_context& ctx ) const {                              \
            std::string s;                                                                         \
            _test_vtz::debug_print( s, value );                                                    \
            return fmt::formatter<std::string_view>::format( s, ctx );                             \
        }                                                                                          \
    }

#undef ASSERT_EQ
#define ASSERT_EQ( lhs, rhs )                                                                      \
    if( auto _compare_result = ( (void)_test_count_assertions.inc(),                               \
            _test_vtz::CmpEq{ lhs, rhs, #lhs, #rhs, __FILE__, __LINE__ } ) )                       \
        ;                                                                                          \
    else                                                                                           \
        return (void)_test_count_assertions.inc_failed(),                                          \
               ::testing::internal::AssertHelper( ::testing::TestPartResult::kFatalFailure,        \
                   __FILE__,                                                                       \
                   __LINE__,                                                                       \
                   _test_vtz::_eq_fail( #lhs, #rhs, *_compare_result.info ).failure_message() )    \
               = testing::Message() << _current_test_context

#define ASSERT_EQ_QUIET( lhs, rhs )                                                                \
    if( auto _compare_result                                                                       \
        = ( (void)_test_count_assertions.inc(), _test_vtz::CmpEq{ lhs, rhs } ) )                   \
        ;                                                                                          \
    else                                                                                           \
        return (void)_test_count_assertions.inc_failed(),                                          \
               ::testing::internal::AssertHelper( ::testing::TestPartResult::kFatalFailure,        \
                   __FILE__,                                                                       \
                   __LINE__,                                                                       \
                   _test_vtz::_eq_fail( #lhs, #rhs, *_compare_result.info ).failure_message() )    \
               = testing::Message() << _current_test_context
