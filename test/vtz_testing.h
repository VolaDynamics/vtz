#include <fmt/color.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <gtest/gtest.h>

namespace _test_vtz {

    struct test_printer
    {
        bool print_on_success = true;
        bool print_expr       = true;
        bool print_location   = true;

        template<class A, class B>
        void print( char const* lhs_expr,
            char const*         rhs_expr,
            A const&            lhs,
            B const&            rhs,
            char const*         filename,
            int                 line )
        {
            char lhs_buff[64];
            char rhs_buff[64];
            auto lhs_result = fmt::format_to_n( lhs_buff, 60, "{}", lhs );
            auto rhs_result = fmt::format_to_n( rhs_buff, 60, "{}", rhs );
            if( lhs_result.size > 60 )
            {
                lhs_result.size = 63;
                lhs_buff[60]    = '.';
                lhs_buff[61]    = '.';
                lhs_buff[62]    = '.';
            }
            if( rhs_result.size > 60 )
            {
                rhs_result.size = 63;
                rhs_buff[60]    = '.';
                rhs_buff[61]    = '.';
                rhs_buff[62]    = '.';
            }
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
                fmt::styled(
                    lhs, fmt::emphasis::bold | fmt::fg( fmt::color::green ) ),
                fmt::styled(
                    rhs, fmt::emphasis::bold | fmt::fg( fmt::color::green ) ) );
        }
    };


    inline test_printer PRINT_SETTINGS{};


    template<class A, class B>
    static auto _eqFail(
        char const* lhs_expr, char const* rhs_expr, A const& lhs, B const& rhs )
    {
        return testing::internal::EqFailure( lhs_expr,
            rhs_expr,
            testing::internal::FormatForComparisonFailureMessage( lhs, rhs ),
            testing::internal::FormatForComparisonFailureMessage( rhs, lhs ),
            false );
    }
} // namespace _test_vtz

#undef ASSERT_EQ
#define ASSERT_EQ( lhs, rhs )                                                  \
    {                                                                          \
        auto const& _test_lhs = lhs;                                           \
        auto const& _test_rhs = rhs;                                           \
        if( _test_lhs == _test_rhs )                                           \
        {                                                                      \
            if( _test_vtz::PRINT_SETTINGS.print_on_success )                   \
                _test_vtz::PRINT_SETTINGS.print( #lhs,                         \
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
