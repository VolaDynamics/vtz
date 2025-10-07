
#include <vtz/files.h>
#include <vtz/strings.h>
#include <vtz/tz_reader.h>

#include <gtest/gtest.h>

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>

#include "vtz_testing.h"

using namespace vtz;
using _test_vtz::TEST_LOG;

namespace {
    template<size_t... N>
    std::vector<string_view> vec_sv( char const ( &... arr )[N] ) {
        return std::vector<string_view>{ string_view( arr, N - 1 )... };
    }

    void check_lines( string_view input, std::vector<string_view> lines ) {
        auto reader = line_iter( input );
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

    void check_tokens( string_view input, std::vector<string_view> tokens ) {
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

TEST( vtz_parser, tokenizer ) {
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


TEST( vtz_parser, lines ) {
    ASSERT_EQ( count_lines( "" ), 0 );
    ASSERT_EQ( count_lines( "\n" ), 1 );
    ASSERT_EQ( count_lines( "hello" ), 1 );
    ASSERT_EQ( count_lines( "hello\n" ), 1 );
    ASSERT_EQ( count_lines( "hello\nworld" ), 2 );
    ASSERT_EQ( count_lines( "hello\nworld\n" ), 2 );
    ASSERT_EQ( count_lines( "hello\r\nworld\n" ), 2 );
    ASSERT_EQ( count_lines( "hello\n"
                            "\n"
                            "The quick brown fox jumps over the lazy dogs.\n"
                            "End.\n" ),
        4 );
    ASSERT_EQ( count_lines( "hello\n"
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


TEST( vtz_parser, strip_comments ) {
    ASSERT_EQ( strip_comment( string_view() ), "" );
    ASSERT_EQ( strip_comment( "# This is a comment" ), "" );
    ASSERT_EQ( strip_comment( "#STDOFF" ), "" );
    ASSERT_EQ( strip_comment( "        #STDOFF" ), "        " );
    ASSERT_EQ( strip_comment( "hello # This is a comment" ), "hello " );
    ASSERT_EQ( strip_comment( "        hello #STDOFF" ), "        hello " );
}

STRUCT_INFO(
    vtz::location, FIELD( vtz::location, line ), FIELD( vtz::location, col ) );

STRUCT_INFO( vtz::raw_rule,
    FIELD( vtz::raw_rule, name ),
    FIELD( vtz::raw_rule, from ),
    FIELD( vtz::raw_rule, to ),
    FIELD( vtz::raw_rule, in ),
    FIELD( vtz::raw_rule, on ),
    FIELD( vtz::raw_rule, at ),
    FIELD( vtz::raw_rule, save ),
    FIELD( vtz::raw_rule, letter ) );

STRUCT_INFO( vtz::raw_zone_entry,
    FIELD( vtz::raw_zone_entry, stdoff ),
    FIELD( vtz::raw_zone_entry, rules ),
    FIELD( vtz::raw_zone_entry, format ),
    FIELD( vtz::raw_zone_entry, until ) );

STRUCT_INFO(
    vtz::raw_zone, FIELD( vtz::raw_zone, name ), FIELD( vtz::raw_zone, ents ) );

STRUCT_INFO( vtz::raw_link,
    FIELD( vtz::raw_link, canonical ),
    FIELD( vtz::raw_link, alias ) );

STRUCT_INFO( vtz::raw_tzdata_file,
    FIELD( vtz::raw_tzdata_file, rules ),
    FIELD( vtz::raw_tzdata_file, zones ),
    FIELD( vtz::raw_tzdata_file, links ) );


using namespace vtz;
TEST( vtz_parser, parse_tzdata ) {
    using ze = raw_zone_entry;
    using r  = raw_rule;


    ASSERT_EQ( parse_tzdata(
                   R"(#
# Note that 1933-05-21 was a Sunday.
# We're left to guess the time of day when Act 163 was approved; guess noon.

# Zone	NAME		STDOFF	RULES	FORMAT	[UNTIL]
Zone Pacific/Honolulu	-10:31:26 -	LMT	1896 Jan 13 12:00
			-10:30	-	HST	1933 Apr 30  2:00
			-10:30	1:00	HDT	1933 May 21 12:00
			-10:30	US	H%sT	1947 Jun  8  2:00
			-10:00	-	HST

# Now we turn to US areas that have diverged from the consensus since 1970.

# Arizona mostly uses MST.
)" ),
        ( raw_tzdata_file{
            {},
            {
                raw_zone{
                    "Pacific/Honolulu",
                    {
                        ze{ "-10:31:26", "-", "LMT", "1896 Jan 13 12:00" },
                        ze{ "-10:30", "-", "HST", "1933 Apr 30  2:00" },
                        ze{ "-10:30", "1:00", "HDT", "1933 May 21 12:00" },
                        ze{ "-10:30", "US", "H%sT", "1947 Jun  8  2:00" },
                        ze{ "-10:00", "-", "HST", {} },
                    },
                },
            },
        } ) );


    /// Test rules immediately before or after zone declaration

    ASSERT_EQ( parse_tzdata(
                   R"(
# Rule	NAME	FROM	TO	-	IN	ON	AT	SAVE	LETTER/S
Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D
Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S
Rule	US	1942	only	-	Feb	9	2:00	1:00	W # War
Rule	US	1945	only	-	Aug	14	23:00u	1:00	P # Peace
Zone Pacific/Honolulu	-10:31:26 -	LMT	1896 Jan 13 12:00
			-10:30	-	HST	1933 Apr 30  2:00
			-10:30	1:00	HDT	1933 May 21 12:00
			-10:30	US	H%sT	1947 Jun  8  2:00
			-10:00	-	HST
Rule	US	1945	only	-	Sep	30	2:00	0	S
Rule	US	1967	2006	-	Oct	lastSun	2:00	0	S
Rule	US	1967	1973	-	Apr	lastSun	2:00	1:00	D
Rule	US	1974	only	-	Jan	6	2:00	1:00	D

# Now we turn to US areas that have diverged from the consensus since 1970.

# Arizona mostly uses MST.
)" ),
        ( raw_tzdata_file{
            std::vector<raw_rule>{
                { "US", "1918", "1919", "Mar", "lastSun", "2:00", "1:00", "D" },
                { "US", "1918", "1919", "Oct", "lastSun", "2:00", "0", "S" },
                { "US", "1942", "only", "Feb", "9", "2:00", "1:00", "W" },
                { "US", "1945", "only", "Aug", "14", "23:00u", "1:00", "P" },
                { "US", "1945", "only", "Sep", "30", "2:00", "0", "S" },
                { "US", "1967", "2006", "Oct", "lastSun", "2:00", "0", "S" },
                { "US", "1967", "1973", "Apr", "lastSun", "2:00", "1:00", "D" },
                { "US", "1974", "only", "Jan", "6", "2:00", "1:00", "D" },
            },
            {
                raw_zone{
                    "Pacific/Honolulu",
                    {
                        ze{ "-10:31:26", "-", "LMT", "1896 Jan 13 12:00" },
                        ze{ "-10:30", "-", "HST", "1933 Apr 30  2:00" },
                        ze{ "-10:30", "1:00", "HDT", "1933 May 21 12:00" },
                        ze{ "-10:30", "US", "H%sT", "1947 Jun  8  2:00" },
                        ze{ "-10:00", "-", "HST", {} },
                    },
                },
            },
        } ) );


    ASSERT_EQ( parse_tzdata(
                   R"(
Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D
Link	Africa/Abidjan	Africa/Accra
Link	Africa/Abidjan	Africa/Bamako
Zone Pacific/Honolulu	-10:31:26 -	LMT	1896 Jan 13 12:00
			-10:30	-	HST	1933 Apr 30  2:00
			-10:30	1:00	HDT	1933 May 21 12:00
			-10:30	US	H%sT	1947 Jun  8  2:00
			-10:00	-	HST
Link	Africa/Abidjan	Africa/Banjul

Link	Africa/Abidjan	Africa/Conakry
Link	Africa/Abidjan	Africa/Dakar
)" ),
        ( raw_tzdata_file{
            std::vector<raw_rule>{
                { "US", "1918", "1919", "Mar", "lastSun", "2:00", "1:00", "D" },
            },
            {
                raw_zone{
                    "Pacific/Honolulu",
                    {
                        ze{ "-10:31:26", "-", "LMT", "1896 Jan 13 12:00" },
                        ze{ "-10:30", "-", "HST", "1933 Apr 30  2:00" },
                        ze{ "-10:30", "1:00", "HDT", "1933 May 21 12:00" },
                        ze{ "-10:30", "US", "H%sT", "1947 Jun  8  2:00" },
                        ze{ "-10:00", "-", "HST", {} },
                    },
                },
            },
            {
                raw_link{ "Africa/Abidjan", "Africa/Accra" },
                raw_link{ "Africa/Abidjan", "Africa/Bamako" },
                raw_link{ "Africa/Abidjan", "Africa/Banjul" },
                raw_link{ "Africa/Abidjan", "Africa/Conakry" },
                raw_link{ "Africa/Abidjan", "Africa/Dakar" },
            },
        } ) );


    /// Test comments within zone declaration
    ASSERT_EQ( parse_tzdata(
                   R"(#
# Note that 1933-05-21 was a Sunday.
# We're left to guess the time of day when Act 163 was approved; guess noon.

# Zone	NAME		STDOFF	RULES	FORMAT	[UNTIL]
Zone Pacific/Honolulu	-10:31:26 -	LMT	1896 Jan 13 12:00
			-10:30	-	HST	1933 Apr 30  2:00   # This is a comment
			-10:30	1:00	HDT	1933 May 21 12:00
            #STDOFF
			-10:30	US	H%sT	1947 Jun  8  2:00
			-10:00	-	HST
# Now we turn to US areas that have diverged from the consensus since 1970.

# Arizona mostly uses MST.
)" ),
        ( raw_tzdata_file{
            {},
            {
                raw_zone{
                    "Pacific/Honolulu",
                    {
                        ze{ "-10:31:26", "-", "LMT", "1896 Jan 13 12:00" },
                        ze{ "-10:30", "-", "HST", "1933 Apr 30  2:00" },
                        ze{ "-10:30", "1:00", "HDT", "1933 May 21 12:00" },
                        ze{ "-10:30", "US", "H%sT", "1947 Jun  8  2:00" },
                        ze{ "-10:00", "-", "HST", {} },
                    },
                },
            },
        } ) );

    /// Test consecutive Zone declarations, with no spacing
    ASSERT_EQ( parse_tzdata(
                   R"(
# From Paul Eggert (2023-01-23):
# America/Adak is for the Aleutian Islands that are part of Alaska
# and are west of 169.5° W.

# Zone	NAME		STDOFF	RULES	FORMAT	[UNTIL]
Zone America/Juneau	 15:02:19 -	LMT	1867 Oct 19 15:33:32
			 -8:57:41 -	LMT	1900 Aug 20 12:00
			 -9:00	US	AK%sT
Zone America/Sitka	 14:58:47 -	LMT	1867 Oct 19 15:30
			 -9:01:13 -	LMT	1900 Aug 20 12:00
			 -8:00	-	PST	1942
			 -8:00	US	P%sT	1946)" ),
        ( raw_tzdata_file{
            {},
            {
                raw_zone{ "America/Juneau",
                    {
                        ze{ "15:02:19", "-", "LMT", "1867 Oct 19 15:33:32" },
                        ze{ "-8:57:41", "-", "LMT", "1900 Aug 20 12:00" },
                        ze{ "-9:00", "US", "AK%sT" },
                    } },
                raw_zone{
                    "America/Sitka",
                    {
                        ze{ "14:58:47", "-", "LMT", "1867 Oct 19 15:30" },
                        ze{ "-9:01:13", "-", "LMT", "1900 Aug 20 12:00" },
                        ze{ "-8:00", "-", "PST", "1942" },
                        ze{ "-8:00", "US", "P%sT", "1946" },
                    },
                },
            },
        } ) )
}


TEST( vtz_parser, location ) {
    ASSERT_EQ( location( 1, 1 ), location::where( "" ) );
    ASSERT_EQ( location( 2, 1 ), location::where( "\n" ) );
    ASSERT_EQ( location( 3, 1 ), location::where( "\n\n" ) );

    ASSERT_EQ( location( 1, 6 ), location::where( "hello" ) );
    ASSERT_EQ( location( 2, 6 ), location::where( "\nhello" ) );
    ASSERT_EQ( location( 3, 6 ), location::where( "\n\nhello" ) );
    ASSERT_EQ( location( 4, 1 ), location::where( "\n\nhello\n" ) );

    ASSERT_EQ( "1:1", location::where( "" ).str() );
    ASSERT_EQ( "2:1", location::where( "\n" ).str() );
    ASSERT_EQ( "3:1", location::where( "\n\n" ).str() );
    ASSERT_EQ( "1:6", location::where( "hello" ).str() );
    ASSERT_EQ( "2:6", location::where( "\nhello" ).str() );
    ASSERT_EQ( "3:6", location::where( "\n\nhello" ).str() );
    ASSERT_EQ( "4:1", location::where( "\n\nhello\n" ).str() );

    string_view body = "line1\n"
                       "line2\n"
                       "line3\n"
                       "line4\n";

    ASSERT_EQ( location( 1, 1 ), location::where( body, 0 ) );
    ASSERT_EQ( location( 1, 6 ), location::where( body, body.find( '\n' ) ) );
    ASSERT_EQ( location( 4, 5 ), location::where( body, body.find( '4' ) ) );
    ASSERT_EQ( location( 5, 1 ), location::where( body, body.size() ) );

    string_view body2 = R"(Link	Africa/Abidjan	Africa/Accra
Link	Africa/Abidjan	Africa/Bamako
Zone Pacific/Honolulu	-10:31:26 -	LMT	1896 Jan 13 12:00
			-10:30	-	HST	1933 Apr 30  2:00
			-10:30	1:00	HDT	1933 May 21 12:00
			-10:30	US	H%sT	1947 Jun  8  2:00
			-10:00	-	HST)";
    ASSERT_EQ( "3:6",
        location::where( body2, body2.find( "Pacific/Honolulu" ) ).str() );

    ASSERT_EQ( "18446744073709551615:18446744073709551615",
        location( 18446744073709551615ul, 18446744073709551615ul ).str() );
}


TEST( vtz_io, read_file ) {
    using namespace std::literals;
    // Check that we can read a text file
    ASSERT_EQ( read_file( "etc/testdata/hello.txt" ), "hello" );

    // Check that we can read a binary file
    ASSERT_EQ( read_file( "etc/testdata/test_file.bin" ),
        "z!\xFB;rh\x9F\xEB"
        "f\x84\xB8\xE5\xA6\xBC"
        "d\x9F\0\xD3\b\xE5"s );

    EXPECT_THROW(
        try {
            // Attempt to open and read the file
            auto s = read_file( nullptr );
        } catch( std::exception const& ex ) {
            TEST_LOG.info_good( "error message", ex.what() );
            throw;
        },
        std::exception );


    EXPECT_THROW(
        try {
            // Attempt to open and read the file
            auto s = read_file( "" );
        } catch( std::exception const& ex ) {
            TEST_LOG.info_good( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // Attempt to open and read the file
            auto s = read_file( "this-file-does-not-exist.txt" );
        } catch( std::exception const& ex ) {
            TEST_LOG.info_good( "error message", ex.what() );
            throw;
        },
        std::exception );
}


TEST( vtz_io, load_northamerica ) {
    using HRC = std::chrono::high_resolution_clock;
    using std::chrono::duration_cast;

    auto t0      = HRC::now();
    auto content = read_file( "build/data/tzdata/northamerica" );
    auto t1      = HRC::now();
    auto result  = parse_tzdata( content );
    auto t2      = HRC::now();

    using msR = std::chrono::duration<double, std::micro>;

    // TEST_LOG.info_good( "northamerica", result );

    TEST_LOG.info_good( "    time read_file()", duration_cast<msR>( t1 - t0 ) );
    TEST_LOG.info_good( " time parse_tzdata()", duration_cast<msR>( t2 - t1 ) );
    TEST_LOG.info_good( "               total", duration_cast<msR>( t2 - t0 ) );
}
