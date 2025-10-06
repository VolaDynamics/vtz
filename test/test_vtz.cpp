#include <vtz/strings.h>

#include <gtest/gtest.h>

#include <fmt/color.h>
#include <fmt/format.h>

#include "vtz_testing.h"


namespace vtz {
    template<size_t... N>
    std::vector<string_view> vec_sv( char const ( &... arr )[N] )
    {
        return std::vector<string_view>{ string_view( arr )... };
    }


    namespace {
        void check_lines( string_view input, std::vector<string_view> lines )
        {
            auto reader = line_reader( input );
            for( auto expected : lines )
            {
                auto line = reader.next();
                ASSERT_TRUE( line.has_value() );
                ASSERT_EQ( line.value(), expected );
            }
            // Trying to pull additional values results in empty optionals
            ASSERT_FALSE( reader.next().has_value() );
            ASSERT_FALSE( reader.next().has_value() );
            ASSERT_FALSE( reader.next().has_value() );

            ASSERT_EQ( vtz::lines( input ), lines );
        }

        void check_tokens( string_view input, std::vector<string_view> tokens )
        {
            auto reader = token_iter( input );
            for( auto expected : tokens )
            {
                auto tok = reader.next();
                ASSERT_TRUE( tok.has_value() );
                ASSERT_TRUE( !tok.empty() );
                ASSERT_EQ( tok.value(), expected );
            }
            // Trying to pull additional values results in empty optionals
            ASSERT_FALSE( reader.next().has_value() );
            ASSERT_FALSE( reader.next().has_value() );
            ASSERT_FALSE( reader.next().has_value() );

            ASSERT_EQ( tokenize( input ), tokens );
        }
    } // namespace

    TEST( vtz_parser, tokenizer )
    {
        // These are all empty
        check_tokens( "", vec_sv() );
        check_tokens( "    ", vec_sv() );
        check_tokens( "  \t \t", vec_sv() );

        // Placement of whitespace does not matter
        check_tokens( "word", vec_sv( "word" ) );
        check_tokens( "  word", vec_sv( "word" ) );
        check_tokens( "word  ", vec_sv( "word" ) );

        check_tokens( "word", vec_sv( "word" ) );
        check_tokens( " \t word", vec_sv( "word" ) );
        check_tokens( "word \t ", vec_sv( "word" ) );

        check_tokens( "hello world", vec_sv( "hello", "world" ) );
        check_tokens( "hello\tworld", vec_sv( "hello", "world" ) );
        check_tokens( "   hello\tworld \t \t", vec_sv( "hello", "world" ) );

        // clang-format off
        check_tokens( "Zone America/Los_Angeles    -7:52:58\t -	LMT	1883 Nov 18 20:00u",
            vec_sv( "Zone",
                "America/Los_Angeles",
                "-7:52:58",
                "-",
                "LMT",
                "1883",
                "Nov",
                "18",
                "20:00u" ) );
        // clang-format on
    }


    TEST( vtz_parser, lines )
    {
        ASSERT_EQ( count_lines( "" ), 0 );
        ASSERT_EQ( count_lines( "\n" ), 1 );
        ASSERT_EQ( count_lines( "hello" ), 1 );
        ASSERT_EQ( count_lines( "hello\n" ), 1 );
        ASSERT_EQ( count_lines( "hello\nworld" ), 2 );
        ASSERT_EQ( count_lines( "hello\nworld\n" ), 2 );
        ASSERT_EQ( count_lines( "hello\r\nworld\n" ), 2 );
        ASSERT_EQ(
            count_lines( "hello\n"
                         "\n"
                         "The quick brown fox jumps over the lazy dogs.\n"
                         "End.\n" ),
            4 );
        ASSERT_EQ(
            count_lines( "hello\n"
                         "\n"
                         "The quick brown fox jumps over the lazy dogs.\n"
                         "End." ),
            4 );

        ASSERT_EQ( count_lines( "hello\n"
                                "\n"
                                "\n"
                                "\n"
                                "\n" ),
            5 );

        check_lines( "", vec_sv() );
        check_lines( "\n", vec_sv( "" ) );
        check_lines( "hello", vec_sv( "hello" ) );
        check_lines( "hello\n", vec_sv( "hello" ) );
        check_lines( "hello\nworld", vec_sv( "hello", "world" ) );
        check_lines( "hello\nworld\n", vec_sv( "hello", "world" ) );
        check_lines( "hello\r\nworld", vec_sv( "hello", "world" ) );
        check_lines( "hello\r\nworld\r\n", vec_sv( "hello", "world" ) );

        // Only '\r\n' is recognized and stripped; '\r' on it's own is not
        // stripped
        check_lines( "hello\r\nworld\r", vec_sv( "hello", "world\r" ) );

        check_lines( "hello\n"
                     "\n"
                     "The quick brown fox jumps over the lazy dogs.\n"
                     "End.\n",
            vec_sv( "hello",
                "",
                "The quick brown fox jumps over the lazy dogs.",
                "End." ) );

        check_lines( "hello\n"
                     "\n"
                     "The quick brown fox jumps over the lazy dogs.\n"
                     "End.",
            vec_sv( "hello",
                "",
                "The quick brown fox jumps over the lazy dogs.",
                "End." ) );

        check_lines( "hello\n"
                     "\n"
                     "\n"
                     "\n"
                     "\n",
            vec_sv( "hello", "", "", "", "" ) );
    }


    TEST( vtz_parser, strip_comments )
    {
        ASSERT_EQ( strip_comment( "# This is a comment" ), "" );
        ASSERT_EQ( strip_comment( "#STDOFF" ), "" );
        ASSERT_EQ( strip_comment( "        #STDOFF" ), "        " );
        ASSERT_EQ( strip_comment( "hello # This is a comment" ), "hello " );
        ASSERT_EQ( strip_comment( "        hello #STDOFF" ), "        hello " );
    }
} // namespace vtz
