
#include <date/tz.h>

#include <vtz/date_types.h>
#include <vtz/files.h>
#include <vtz/strings.h>
#include <vtz/tz_reader.h>

#include <gtest/gtest.h>

#include <chrono>
#include <fmt/chrono.h>
#include <fmt/color.h>
#include <fmt/format.h>

#include <vtz/civil.h>
#include <vtz/tz.h>

#include <vtz/libfmt_compat.h>

#include "vtz_debug.h"
#include "vtz_testing.h"

using namespace vtz;
using _test_vtz::TEST_LOG;

static_assert( format_as( Mon::Jan ) == "Jan" );
static_assert( format_as( Mon::Feb ) == "Feb" );
static_assert( format_as( Mon::Mar ) == "Mar" );
static_assert( format_as( Mon::Apr ) == "Apr" );
static_assert( format_as( Mon::May ) == "May" );
static_assert( format_as( Mon::Jun ) == "Jun" );
static_assert( format_as( Mon::Jul ) == "Jul" );
static_assert( format_as( Mon::Aug ) == "Aug" );
static_assert( format_as( Mon::Sep ) == "Sep" );
static_assert( format_as( Mon::Oct ) == "Oct" );
static_assert( format_as( Mon::Nov ) == "Nov" );
static_assert( format_as( Mon::Dec ) == "Dec" );

static_assert( format_as( DOW::Sun ) == "Sun" );
static_assert( format_as( DOW::Mon ) == "Mon" );
static_assert( format_as( DOW::Tue ) == "Tue" );
static_assert( format_as( DOW::Wed ) == "Wed" );
static_assert( format_as( DOW::Thu ) == "Thu" );
static_assert( format_as( DOW::Fri ) == "Fri" );
static_assert( format_as( DOW::Sat ) == "Sat" );

static_assert( _impl::_parse_mon( "blarg", 3 ) == none );
static_assert( _impl::_parse_mon( "Jan", 3 ).value() == Mon::Jan );
static_assert( _impl::_parse_mon( "Feb", 3 ).value() == Mon::Feb );
static_assert( _impl::_parse_mon( "Mar", 3 ).value() == Mon::Mar );
static_assert( _impl::_parse_mon( "Apr", 3 ).value() == Mon::Apr );
static_assert( _impl::_parse_mon( "May", 3 ).value() == Mon::May );
static_assert( _impl::_parse_mon( "Jun", 3 ).value() == Mon::Jun );
static_assert( _impl::_parse_mon( "Jul", 3 ).value() == Mon::Jul );
static_assert( _impl::_parse_mon( "Aug", 3 ).value() == Mon::Aug );
static_assert( _impl::_parse_mon( "Sep", 3 ).value() == Mon::Sep );
static_assert( _impl::_parse_mon( "Oct", 3 ).value() == Mon::Oct );
static_assert( _impl::_parse_mon( "Nov", 3 ).value() == Mon::Nov );
static_assert( _impl::_parse_mon( "Dec", 3 ).value() == Mon::Dec );

static_assert( _impl::_parse_dow( "blarg", 3 ) == none );
static_assert( _impl::_parse_dow( "Sun", 3 ).value() == DOW::Sun );
static_assert( _impl::_parse_dow( "Mon", 3 ).value() == DOW::Mon );
static_assert( _impl::_parse_dow( "Tue", 3 ).value() == DOW::Tue );
static_assert( _impl::_parse_dow( "Wed", 3 ).value() == DOW::Wed );
static_assert( _impl::_parse_dow( "Thu", 3 ).value() == DOW::Thu );
static_assert( _impl::_parse_dow( "Fri", 3 ).value() == DOW::Fri );
static_assert( _impl::_parse_dow( "Sat", 3 ).value() == DOW::Sat );
static_assert( _impl::_parse_dow( "Su", 2 ).value() == DOW::Sun );
static_assert( _impl::_parse_dow( "Mo", 2 ).value() == DOW::Mon );
static_assert( _impl::_parse_dow( "Tu", 2 ).value() == DOW::Tue );
static_assert( _impl::_parse_dow( "We", 2 ).value() == DOW::Wed );
static_assert( _impl::_parse_dow( "Th", 2 ).value() == DOW::Thu );
static_assert( _impl::_parse_dow( "Fr", 2 ).value() == DOW::Fri );
static_assert( _impl::_parse_dow( "Sa", 2 ).value() == DOW::Sat );
static_assert( _impl::_parse_dow( "M", 1 ).value() == DOW::Mon );
static_assert( _impl::_parse_dow( "W", 1 ).value() == DOW::Wed );
static_assert( _impl::_parse_dow( "F", 1 ).value() == DOW::Fri );


static_assert( !OptMon{}.has_value() );
static_assert( !OptMon( none ).has_value() );
static_assert( OptMon{ Mon::Jan }.has_value() );
static_assert( OptMon{ Mon::Feb }.has_value() );
static_assert( OptMon{ Mon::Mar }.has_value() );
static_assert( OptMon{ Mon::Apr }.has_value() );
static_assert( OptMon{ Mon::May }.has_value() );
static_assert( OptMon{ Mon::Jun }.has_value() );
static_assert( OptMon{ Mon::Jul }.has_value() );
static_assert( OptMon{ Mon::Aug }.has_value() );
static_assert( OptMon{ Mon::Sep }.has_value() );
static_assert( OptMon{ Mon::Oct }.has_value() );
static_assert( OptMon{ Mon::Nov }.has_value() );
static_assert( OptMon{ Mon::Dec }.has_value() );

static_assert( !OptDOW{}.has_value() );
static_assert( !OptDOW( none ).has_value() );
static_assert( OptDOW{ DOW::Sun }.has_value() );
static_assert( OptDOW{ DOW::Mon }.has_value() );
static_assert( OptDOW{ DOW::Tue }.has_value() );
static_assert( OptDOW{ DOW::Wed }.has_value() );
static_assert( OptDOW{ DOW::Thu }.has_value() );
static_assert( OptDOW{ DOW::Fri }.has_value() );
static_assert( OptDOW{ DOW::Sat }.has_value() );

static_assert( rule_letter().sv() == "" );
static_assert( rule_letter().size_ == 0 );
static_assert( rule_letter( "x" ).sv() == "x" );

DECLARE_STRINGLIKE( rule_letter );

FMT_ENUM_PLAIN( vtz::ZoneRule::Kind );

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
        vec_sv( "hello", "", "The quick brown fox jumps over the lazy dogs.", "End." ) );

    check_lines( "hello\n"
                 "\n"
                 "The quick brown fox jumps over the lazy dogs.\n"
                 "End.",
        vec_sv( "hello", "", "The quick brown fox jumps over the lazy dogs.", "End." ) );

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


using namespace vtz;
TEST( vtz_parser, parse_tzdata ) {
    using ze = ZoneEntry;
    using M  = Mon;

    constexpr auto lastSun = RuleOn::last( DOW::Sun );
    constexpr auto on      = []( u8 day ) { return RuleOn::on( day ); };

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
        ( TZDataFile{
            {},
            {
                {
                    "Pacific/Honolulu",
                    {
                        ze{ "-10:31:26", "-", "LMT", "1896 Jan 13 12:00" },
                        ze{ "-10:30", "-", "HST", "1933 Apr 30  2:00" },
                        ze{ "-10:30", "1:00", "HDT", "1933 May 21 12:00" },
                        ze{ "-10:30", "US", "H%sT", "1947 Jun  8  2:00" },
                        ze{ "-10:00", "-", "HST" },
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
        ( TZDataFile{
            { {
                "US",
                vector<RuleEntry>{
                    { 1918, 1919, M::Mar, lastSun, "2:00", "1:00", "D" },
                    { 1918, 1919, M::Oct, lastSun, "2:00", "0", "S" },
                    { 1942, 1942, M::Feb, on( 9 ), "2:00", "1:00", "W" },
                    { 1945, 1945, M::Aug, on( 14 ), "23:00u", "1:00", "P" },
                    { 1945, 1945, M::Sep, on( 30 ), "2:00", "0", "S" },
                    { 1967, 2006, M::Oct, lastSun, "2:00", "0", "S" },
                    { 1967, 1973, M::Apr, lastSun, "2:00", "1:00", "D" },
                    { 1974, 1974, M::Jan, on( 6 ), "2:00", "1:00", "D" },
                },
            } },
            {
                {
                    "Pacific/Honolulu",
                    {
                        ze{ "-10:31:26", "-", "LMT", "1896 Jan 13 12:00" },
                        ze{ "-10:30", "-", "HST", "1933 Apr 30  2:00" },
                        ze{ "-10:30", "1:00", "HDT", "1933 May 21 12:00" },
                        ze{ "-10:30", "US", "H%sT", "1947 Jun  8  2:00" },
                        ze{ "-10:00", "-", "HST" },
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
        ( TZDataFile{
            { {
                {
                    "US",
                    { { 1918, 1919, M::Mar, lastSun, "2:00", "1:00", "D" } },
                },
            } },
            {
                {
                    "Pacific/Honolulu",
                    {
                        ze{ "-10:31:26", "-", "LMT", "1896 Jan 13 12:00" },
                        ze{ "-10:30", "-", "HST", "1933 Apr 30  2:00" },
                        ze{ "-10:30", "1:00", "HDT", "1933 May 21 12:00" },
                        ze{ "-10:30", "US", "H%sT", "1947 Jun  8  2:00" },
                        ze{ "-10:00", "-", "HST" },
                    },
                },
            },
            {
                { "Africa/Accra", "Africa/Abidjan" },
                { "Africa/Bamako", "Africa/Abidjan" },
                { "Africa/Banjul", "Africa/Abidjan" },
                { "Africa/Conakry", "Africa/Abidjan" },
                { "Africa/Dakar", "Africa/Abidjan" },
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
        ( TZDataFile{
            {},
            {
                {
                    "Pacific/Honolulu",
                    {
                        ze{ "-10:31:26", "-", "LMT", "1896 Jan 13 12:00" },
                        ze{ "-10:30", "-", "HST", "1933 Apr 30  2:00" },
                        ze{ "-10:30", "1:00", "HDT", "1933 May 21 12:00" },
                        ze{ "-10:30", "US", "H%sT", "1947 Jun  8  2:00" },
                        ze{ "-10:00", "-", "HST" },
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
			 -8:00	US	P%sT	1946
             -8:00	-	PST	1969
			 -8:00	US	P%sT	1983 Oct 30  2:00
			 -9:00	US	Y%sT	1983 Nov 30
			 -9:00	US	AK%sT)" ),
        ( TZDataFile{
            {},
            {
                { "America/Juneau",
                    {
                        ze{ "15:02:19", "-", "LMT", "1867 Oct 19 15:33:32" },
                        ze{ "-8:57:41", "-", "LMT", "1900 Aug 20 12:00" },
                        ze{ "-9:00", "US", "AK%sT" },
                    } },
                {
                    "America/Sitka",
                    {
                        ze{ "14:58:47", "-", "LMT", "1867 Oct 19 15:30" },
                        ze{ "-9:01:13", "-", "LMT", "1900 Aug 20 12:00" },
                        ze{ "-8:00", "-", "PST", "1942" },
                        ze{ "-8:00", "US", "P%sT", "1946" },
                        ze{ "-8:00", "-", "PST", "1969" },
                        ze{ "-8:00", "US", "P%sT", "1983 Oct 30  2:00" },
                        ze{ "-9:00", "US", "Y%sT", "1983 Nov 30" },
                        ze{ "-9:00", "US", "AK%sT" },
                    },
                },
            },
        } ) );
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
    ASSERT_EQ( "3:6", location::where( body2, body2.find( "Pacific/Honolulu" ) ).str() );

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
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );


    EXPECT_THROW(
        try {
            // Attempt to open and read the file
            auto s = read_file( "" );
        } catch( std::exception const& ex ) {
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // Attempt to open and read the file
            auto s = read_file( "this-file-does-not-exist.txt" );
        } catch( std::exception const& ex ) {
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );
}


TEST( vtz_parser, parse_errors ) {
    EXPECT_THROW(
        try {
            // clang-format off
            parse_tzdata(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb2	9	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parse_tzdata(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	# Feb	9	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parse_tzdata(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942x	only	-	Feb	9	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parse_tzdata(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	92	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: Day of Month is out of range
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parse_tzdata(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	9x	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: Day of Month is not completely parsed
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parse_tzdata(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	0	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: Day of Month is out of range (0 is out of range)
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parse_tzdata(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	x9	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: Day of Month is not an int
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parse_tzdata(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	last_sud	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: last[DOW] does not have a valid DOW
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );

    // This is fine
    // clang-format off
    EXPECT_NO_THROW(
        parse_tzdata(
            "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
            "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
            "Rule	US	1942	only	-	Feb	Tue>=13	2:00	1:00	W # War\n" ) );

    // Also fine - we can have [DOW]<=[DOM]
    EXPECT_NO_THROW(
        parse_tzdata(
            "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
            "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
            "Rule	US	1942	only	-	Feb	Fri<=1	2:00	1:00	W # War\n" ) );
    // clang-format on

    EXPECT_THROW(
        try {
            // clang-format off
            parse_tzdata(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	Sud>=13	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: last[DOW] does not have a valid DOW
            TEST_LOG.log_good( "error message", ex.what() );
            throw;
        },
        std::exception );
}


namespace {
    std::pair<string, TZDataFile> test_load_file( char const* fp ) {
        using HRC = std::chrono::high_resolution_clock;
        using std::chrono::duration_cast;

        auto t0      = HRC::now();
        auto content = read_file( fp );
        auto t1      = HRC::now();
        auto result  = parse_tzdata( content, fp );
        auto t2      = HRC::now();

        using ms_r = std::chrono::duration<double, std::micro>;

        // Find longest rule name
        string_view longest_rule_name;
        for( const auto& [rule_name, rule_entries] : result.rules )
        {
            if( rule_name.size() > longest_rule_name.size() ) { longest_rule_name = rule_name; }
        }

        TEST_LOG.log_good( " successfully loaded", fp );
        TEST_LOG.log_good( "    time read_file()", duration_cast<ms_r>( t1 - t0 ) );
        TEST_LOG.log_good( " time parse_tzdata()", duration_cast<ms_r>( t2 - t1 ) );
        TEST_LOG.log_good( "               total", duration_cast<ms_r>( t2 - t0 ) );

        if( !longest_rule_name.empty() )
        {
            TEST_LOG.log_good( "  longest rule name",
                fmt::format( "{} (length={})", longest_rule_name, longest_rule_name.size() ) );
        }

        return { std::move( content ), std::move( result ) };
    }
} // namespace


TEST( vtz_io, load_all ) {
    test_load_file( "build/data/tzdata/africa" );
    test_load_file( "build/data/tzdata/antarctica" );
    test_load_file( "build/data/tzdata/asia" );
    test_load_file( "build/data/tzdata/australasia" );
    test_load_file( "build/data/tzdata/backward" );
    test_load_file( "build/data/tzdata/backzone" );
    test_load_file( "build/data/tzdata/etcetera" );
    test_load_file( "build/data/tzdata/europe" );
    test_load_file( "build/data/tzdata/factory" );
    test_load_file( "build/data/tzdata/northamerica" );
    test_load_file( "build/data/tzdata/southamerica" );
    auto [file, all_zones] = test_load_file( "build/data/tzdata/tzdata.zi" );

    ASSERT_EQ( all_zones.links["GMT"], "Etc/GMT" );
    ASSERT_EQ( all_zones.links["EST5EDT"], "America/New_York" );
}


TEST( vtz_tz, US_rules ) {
    auto [content, zones] = test_load_file( "build/data/tzdata/northamerica" );

    auto US = zones.evaluate_rules( "US" );

    using RE = RuleEntry;
    using RT = RuleTrans;

    // https://www.timeanddate.com/calendar/?year=1919&country=1
    ASSERT_EQ( US.year_start, 1918 );
    ASSERT_EQ( US.year_end, 2007 );

    // There were 7 transitions before 1967, and then 2 transitions a year between 1967 and 2007
    // Transitions within or after 2007 are "active". We're currently using the 2007 rules,
    // so they are not listed as historical.
    ASSERT_EQ( US.historical.size(), 7 + ( 2007 - 1967 ) * 2 );

    ASSERT_EQ( US.historical[0], RT::from_civil( 1918, 3, 31, "2:00", "1:00", "D" ) );
    ASSERT_EQ( US.historical[1], RT::from_civil( 1918, 10, 27, "2:00", "0", "S" ) );
    ASSERT_EQ( US.historical[2], RT::from_civil( 1919, 3, 30, "2:00", "1:00", "D" ) );
    ASSERT_EQ( US.historical[3], RT::from_civil( 1919, 10, 26, "2:00", "0", "S" ) );

    // Enter War Time on February 9th, 1942
    ASSERT_EQ( US.historical[4], RT::from_civil( 1942, 2, 9, "2:00", "1:00", "W" ) );
    // Transition to Peace Time on August 14th, 1945
    ASSERT_EQ( US.historical[5], RT::from_civil( 1945, 8, 14, "23:00u", "1:00", "P" ) );

    // On September 30th, 1945, the US moved back to Standard Time
    ASSERT_EQ( US.historical[6], RT::from_civil( 1945, 9, 30, "2:00", "0", "S" ) );

    // We started to do Daylight Savings Time again for some fucking reasons
    ASSERT_EQ( US.historical[7], RT::from_civil( 1967, 4, 30, "2:00", "1:00", "D" ) );
    ASSERT_EQ( US.historical[8], RT::from_civil( 1967, 10, 29, "2:00", "0", "S" ) );

    // Historical rules apply until 2006. From then onwards we started using the current set of
    // rules!
    size_t n = US.historical.size();
    ASSERT_EQ( US.historical[n - 2], RT::from_civil( 2006, 4, 2, "2:00", "1:00", "D" ) );
    ASSERT_EQ( US.historical[n - 1], RT::from_civil( 2006, 10, 29, "2:00", "0", "S" ) );

    ASSERT_EQ( US.active.at( 0 ),
        ( RE{ 2007, Y_MAX, Mon::Mar, RuleOn::ge( DOW::Sun, 8 ), "2:00", "1:00", "D" } ) );
    ASSERT_EQ( US.active.at( 1 ),
        ( RE{ 2007, Y_MAX, Mon::Nov, RuleOn::ge( DOW::Sun, 1 ), "2:00", "0", "S" } ) );
}


TEST( vtz_tz, America_NewYork ) {
    using HRC             = std::chrono::high_resolution_clock;
    auto [content, zones] = test_load_file( "build/data/tzdata/northamerica" );
    auto t0               = HRC::now();

    auto times = zones.get_zone_states( "America/New_York", 2050 );

    auto t1 = HRC::now();

    using ms_r = std::chrono::duration<double, std::micro>;

    TEST_LOG.log_good( "time get_zone_states()", std::chrono::duration_cast<ms_r>( t1 - t0 ) );

    fmt::println( "Initial state: stdoff={} save={} abbr={}",
        times.initial().stdoff,
        times.initial().save(),
        times.initial().abbr.sv() );

    for( auto const& trans : times.get_transitions() )
    {
        fmt::println( "NEW STATE @ utc={} local={} stdoff={} save={} abbr={}",
            fmt::styled( utc_to_string( trans.when ), FAINT_GRAY ),
            fmt::styled(
                local_to_string( trans.when, trans.state.walloff, trans.state.abbr ), BOLD_GREEN ),
            trans.state.stdoff,
            trans.state.save(),
            trans.state.abbr.sv() );
    }
}


TEST( vtz_tz, Asia_Singapore ) {
    using HRC             = std::chrono::high_resolution_clock;
    auto [content, zones] = test_load_file( "build/data/tzdata/asia" );
    auto t0               = HRC::now();
    auto times            = zones.get_zone_states( "Asia/Singapore", 2050 );

    auto t1 = HRC::now();

    using ms_r = std::chrono::duration<double, std::micro>;

    TEST_LOG.log_good( "time get_zone_states()", std::chrono::duration_cast<ms_r>( t1 - t0 ) );

    fmt::println( "Initial state: stdoff={} save={} abbr={}",
        times.initial().stdoff,
        times.initial().save(),
        times.initial().abbr.sv() );

    for( auto const& trans : times.get_transitions() )
    {
        fmt::println( "NEW STATE @ utc={} local={} stdoff={} save={} abbr={}",
            fmt::styled( utc_to_string( trans.when ), FAINT_GRAY ),
            fmt::styled(
                local_to_string( trans.when, trans.state.walloff, trans.state.abbr ), BOLD_GREEN ),
            trans.state.stdoff,
            trans.state.save(),
            trans.state.abbr.sv() );
    }
}


TEST( vtz_parser, basics ) {
    ASSERT_EQ( parse_hhmmss_offset( "0:12:12" ), 60 * 12 + 12 );
    ASSERT_EQ( parse_hhmmss_offset( "2:00" ), 7200 );
    ASSERT_EQ( parse_signed_hhmmss_offset( "2:00" ), 7200 );
    ASSERT_EQ( parse_signed_hhmmss_offset( "-2:00" ), -7200 );

    ASSERT_EQ( format_as( parse_rule_on( "lastSun" ) ), "lastSun" );
    ASSERT_EQ( format_as( parse_rule_on( "lastMon" ) ), "lastMon" );
    ASSERT_EQ( format_as( parse_rule_on( "lastTue" ) ), "lastTue" );
    ASSERT_EQ( format_as( parse_rule_on( "lastWed" ) ), "lastWed" );
    ASSERT_EQ( format_as( parse_rule_on( "lastThu" ) ), "lastThu" );
    ASSERT_EQ( format_as( parse_rule_on( "lastFri" ) ), "lastFri" );
    ASSERT_EQ( format_as( parse_rule_on( "lastSat" ) ), "lastSat" );

    ASSERT_EQ( format_as( parse_rule_on( "Sun>=1" ) ), "Sun>=1" );
    ASSERT_EQ( format_as( parse_rule_on( "Mon>=2" ) ), "Mon>=2" );
    ASSERT_EQ( format_as( parse_rule_on( "Tue>=3" ) ), "Tue>=3" );
    ASSERT_EQ( format_as( parse_rule_on( "Wed>=10" ) ), "Wed>=10" );
    ASSERT_EQ( format_as( parse_rule_on( "Thu>=11" ) ), "Thu>=11" );
    ASSERT_EQ( format_as( parse_rule_on( "Fri>=12" ) ), "Fri>=12" );
    ASSERT_EQ( format_as( parse_rule_on( "Sat>=31" ) ), "Sat>=31" );

    ASSERT_EQ( format_as( parse_rule_on( "Sun<=1" ) ), "Sun<=1" );
    ASSERT_EQ( format_as( parse_rule_on( "Mon<=2" ) ), "Mon<=2" );
    ASSERT_EQ( format_as( parse_rule_on( "Tue<=3" ) ), "Tue<=3" );
    ASSERT_EQ( format_as( parse_rule_on( "Wed<=10" ) ), "Wed<=10" );
    ASSERT_EQ( format_as( parse_rule_on( "Thu<=11" ) ), "Thu<=11" );
    ASSERT_EQ( format_as( parse_rule_on( "Fri<=12" ) ), "Fri<=12" );
    ASSERT_EQ( format_as( parse_rule_on( "Sat<=31" ) ), "Sat<=31" );

    ASSERT_ANY_THROW( parse_rule_on( "Sun<=0" ); );

    auto E_EurAsia    = parse_zone_rule( "E-EurAsia" );
    auto NT_YK        = parse_zone_rule( "NT_YK" );
    auto Indianapolis = parse_zone_rule( "Indianapolis" );
    auto hyphen       = parse_zone_rule( "-" );
    auto off1         = parse_zone_rule( "1:23" );
    auto off2         = parse_zone_rule( "+1:23" );
    auto off3         = parse_zone_rule( "-1:23" );

    ASSERT_EQ( NT_YK.kind(), ZoneRule::NAMED );
    ASSERT_EQ( NT_YK.name(), "NT_YK" );

    ASSERT_EQ( Indianapolis.name(), "Indianapolis" );

    ASSERT_EQ( E_EurAsia.is_named(), true );
    ASSERT_EQ( E_EurAsia.is_hyphen(), false );
    ASSERT_EQ( E_EurAsia.is_offset(), false );
    ASSERT_EQ( E_EurAsia.kind(), ZoneRule::NAMED );
    ASSERT_EQ( E_EurAsia.name(), "E-EurAsia" );

    ASSERT_EQ( hyphen.is_named(), false );
    ASSERT_EQ( hyphen.is_hyphen(), true );
    ASSERT_EQ( hyphen.is_offset(), false );
    ASSERT_EQ( hyphen.kind(), ZoneRule::HYPHEN );
    ASSERT_EQ( hyphen.name(), "-" );

    ASSERT_EQ( off1.is_named(), false );
    ASSERT_EQ( off1.is_hyphen(), false );
    ASSERT_EQ( off1.is_offset(), true );
    ASSERT_EQ( off1.kind(), ZoneRule::OFFSET );
    ASSERT_EQ( off1.offset(), 4980 );
    ASSERT_EQ( format_as( off1 ), "01:23" );

    ASSERT_EQ( off2.kind(), ZoneRule::OFFSET );
    ASSERT_EQ( off2.offset(), 4980 );
    ASSERT_EQ( format_as( off2 ), "01:23" );

    ASSERT_EQ( off3.kind(), ZoneRule::OFFSET );
    ASSERT_EQ( off3.offset(), -4980 );
    ASSERT_EQ( format_as( off3 ), "-01:23" );
}


TEST( vtz_parser, zone_format ) {
    COUNT_ASSERTIONS();

    // Test ZoneFormat
    using ZF = ZoneFormat;

    // === LITERAL format tests ===
    static_assert( ZF{ "GMT" }.with( ZF::LITERAL, 3, 0 ).format( 0, false, "S" ).sv() == "GMT" );
    ASSERT_EQ( parse_zone_format( "EST" ), ZF{ "EST" }.with( ZF::LITERAL, 3 ) );
    ASSERT_EQ( parse_zone_format( "-00" ), ZF{ "-00" }.with( ZF::LITERAL, 3 ) );

    // Longer literal formats
    ASSERT_EQ( parse_zone_format( "WEST" ), ZF{ "WEST" }.with( ZF::LITERAL, 4 ) );
    ASSERT_EQ( parse_zone_format( "ACST" ), ZF{ "ACST" }.with( ZF::LITERAL, 4 ) );

    // === FMT_S (%s substitution) tests ===
    static_assert( ZF{ "ET" }.with( ZF::FMT_S, 1, 1 ).tag() == ZF::FMT_S );
    static_assert( ZF{ "ET" }.with( ZF::FMT_S, 1, 1 ).format( 0, false, "S" ).sv() == "EST" );
    static_assert( ZF{ "ET" }.with( ZF::FMT_S, 1, 1 ).format( 0, false, "D" ).sv() == "EDT" );
    static_assert( ZF{ "ET" }.with( ZF::FMT_S, 1, 1 ).format( 0, false, "-xx-" ).sv() == "E-xx-T" );
    static_assert( ZF{ "ET" }.with( ZF::FMT_S, 0, 2 ).format( 0, false, "-xx-" ).sv() == "-xx-ET" );
    static_assert( ZF{ "ET" }.with( ZF::FMT_S, 2, 0 ).format( 0, false, "-xx-" ).sv() == "ET-xx-" );

    ASSERT_EQ( parse_zone_format( "E%sT" ), ZF{ "ET" }.with( ZF::FMT_S, 1, 1 ) );
    ASSERT_EQ( parse_zone_format( "C%sT" ), ZF{ "CT" }.with( ZF::FMT_S, 1, 1 ) );

    // %s at the beginning
    ASSERT_EQ( parse_zone_format( "%sT" ), ZF{ "T" }.with( ZF::FMT_S, 0, 1 ) );
    ASSERT_EQ( parse_zone_format( "%sST" ), ZF{ "ST" }.with( ZF::FMT_S, 0, 2 ) );

    // %s at the end
    ASSERT_EQ( parse_zone_format( "E%s" ), ZF{ "E" }.with( ZF::FMT_S, 1, 0 ) );
    ASSERT_EQ( parse_zone_format( "WE%s" ), ZF{ "WE" }.with( ZF::FMT_S, 2, 0 ) );

    // %s in the middle with more context
    ASSERT_EQ( parse_zone_format( "AC%sT" ), ZF{ "ACT" }.with( ZF::FMT_S, 2, 1 ) );
    ASSERT_EQ( parse_zone_format( "AE%sST" ), ZF{ "AEST" }.with( ZF::FMT_S, 2, 2 ) );

    // === FMT_Z (%z numeric offset) tests ===
    static_assert( ZF{}.with( ZF::FMT_Z, 0, 0 ).format( 0, false, "" ).sv() == "+00" );
    static_assert( ZF{}.with( ZF::FMT_Z, 0, 0 ).format( 3600, false, "" ).sv() == "+01" );
    static_assert( ZF{}.with( ZF::FMT_Z, 0, 0 ).format( -3600, false, "" ).sv() == "-01" );
    static_assert( ZF{}.with( ZF::FMT_Z, 0, 0 ).format( 3660, false, "" ).sv() == "+0101" );
    static_assert( ZF{}.with( ZF::FMT_Z, 0, 0 ).format( -3660, false, "" ).sv() == "-0101" );
    static_assert( ZF{}.with( ZF::FMT_Z, 0, 0 ).format( 3697, false, "" ).sv() == "+010137" );
    static_assert( ZF{}.with( ZF::FMT_Z, 0, 0 ).format( -3697, false, "" ).sv() == "-010137" );
    static_assert( ZF{}.with( ZF::FMT_Z, 0, 0 ).format( 5025, false, "" ).sv() == "+012345" );
    static_assert( ZF{}.with( ZF::FMT_Z, 0, 0 ).format( -5025, false, "" ).sv() == "-012345" );

    static_assert( ZF{ "xyz" }.with( ZF::FMT_Z, 3, 0 ).format( 0, false, "" ).sv() == "xyz+00" );
    static_assert( ZF{ "xyz" }.with( ZF::FMT_Z, 2, 1 ).format( 0, false, "" ).sv() == "xy+00z" );
    static_assert( ZF{ "xyz" }.with( ZF::FMT_Z, 1, 2 ).format( 0, false, "" ).sv() == "x+00yz" );
    static_assert( ZF{ "xyz" }.with( ZF::FMT_Z, 0, 3 ).format( 0, false, "" ).sv() == "+00xyz" );

    static_assert(
        ZF{ "he" }.with( ZF::FMT_Z, 2, 0 ).format( 5025, false, "" ).sv() == "he+012345" );
    static_assert(
        ZF{ "hello" }.with( ZF::FMT_Z, 5, 0 ).format( 5025, false, "" ).sv() == "hello+012345" );
    static_assert(
        ZF{ "hello" }.with( ZF::FMT_Z, 1, 4 ).format( 5025, false, "" ).sv() == "h+012345ello" );

    ASSERT_EQ( parse_zone_format( "E%zT" ), ZF{ "ET" }.with( ZF::FMT_Z, 1, 1 ) );
    ASSERT_EQ( parse_zone_format( "%z" ), ZF{}.with( ZF::FMT_Z ) );

    // %z with prefixes
    ASSERT_EQ( parse_zone_format( "GMT%z" ), ZF{ "GMT" }.with( ZF::FMT_Z, 3, 0 ) );
    ASSERT_EQ( parse_zone_format( "UTC%z" ), ZF{ "UTC" }.with( ZF::FMT_Z, 3, 0 ) );

    // %z with suffixes
    ASSERT_EQ( parse_zone_format( "%zST" ), ZF{ "ST" }.with( ZF::FMT_Z, 0, 2 ) );

    // %z with both prefix and suffix
    ASSERT_EQ( parse_zone_format( "A%zB" ), ZF{ "AB" }.with( ZF::FMT_Z, 1, 1 ) );

    // === SLASH format tests ===
    ASSERT_EQ( parse_zone_format( "EST/EDT" ), ZF{ "ESTEDT" }.with( ZF::SLASH, 3, 3 ) );
    ASSERT_EQ( parse_zone_format( "GMT/BDST" ), ZF{ "GMTBDST" }.with( ZF::SLASH, 3, 4 ) );
    ASSERT_EQ( parse_zone_format( "GMT/BST" ), ZF{ "GMTBST" }.with( ZF::SLASH, 3, 3 ) );
    ASSERT_EQ( parse_zone_format( "CET/CEST" ), ZF{ "CETCEST" }.with( ZF::SLASH, 3, 4 ) );
    ASSERT_EQ( parse_zone_format( "EET/EEST" ), ZF{ "EETEEST" }.with( ZF::SLASH, 3, 4 ) );

    // Slash with asymmetric parts
    ASSERT_EQ( parse_zone_format( "A/BCDE" ), ZF{ "ABCDE" }.with( ZF::SLASH, 1, 4 ) );
    ASSERT_EQ( parse_zone_format( "A/BCDE" ).format( 0, false, "" ).sv(), "A" );
    ASSERT_EQ( parse_zone_format( "A/BCDE" ).format( 0, true, "" ).sv(), "BCDE" );

    // === Round-trip tests (parse then format) ===

    // Literal round-trips
    auto lit1 = parse_zone_format( "ACST" );
    ASSERT_EQ( lit1, ZF{ "ACST" }.with( ZF::LITERAL, 4 ) );
    static_assert( ZF{ "ACST" }.with( ZF::LITERAL, 4 ).format( 0, false, "" ).sv() == "ACST" );

    ASSERT_EQ( lit1.format( 0, false, "" ).sv(), "ACST" );

    // %s round-trips
    auto fmts1 = parse_zone_format( "WE%sT" );
    ASSERT_EQ( fmts1.format( 0, false, "S" ).sv(), "WEST" );
    ASSERT_EQ( fmts1.format( 0, false, "D" ).sv(), "WEDT" );

    auto fmts2 = parse_zone_format( "%sT" );
    ASSERT_EQ( fmts2.format( 0, false, "ES" ).sv(), "EST" );
    ASSERT_EQ( fmts2.format( 0, false, "ED" ).sv(), "EDT" );

    // %z round-trips
    auto fmtz1 = parse_zone_format( "%z" );
    ASSERT_EQ( fmtz1.format( -10800, false, "" ).sv(), "-03" );
    ASSERT_EQ( fmtz1.format( 19800, false, "" ).sv(), "+0530" );

    auto fmtz2 = parse_zone_format( "GMT%z" );
    ASSERT_EQ( fmtz2.format( 0, false, "" ).sv(), "GMT+00" );
    ASSERT_EQ( fmtz2.format( -3600, false, "" ).sv(), "GMT-01" );

    // Slash round-trips
    auto slash1 = parse_zone_format( "CST/CDT" );
    ASSERT_EQ( slash1.format( 0, false, "" ).sv(), "CST" );
    ASSERT_EQ( slash1.format( 0, true, "" ).sv(), "CDT" );

    // Test slash format selection based on is_dst flag
    auto slash_fmt = parse_zone_format( "PST/PDT" );
    ASSERT_EQ( slash_fmt.format( 0, false, "" ).sv(), "PST" ); // is_dst=false
    ASSERT_EQ( slash_fmt.format( 0, true, "" ).sv(), "PDT" );  // is_dst=true

    // === Edge cases ===

    // Empty format
    ASSERT_ANY_THROW( parse_zone_format( "" ) );

    // Single character
    ASSERT_EQ( parse_zone_format( "A" ), ZF{ "A" }.with( ZF::LITERAL, 1 ) );

    // Maximum reasonable length literal
    ASSERT_EQ( parse_zone_format( "VERYLONGZONE" ), ZF{ "VERYLONGZONE" }.with( ZF::LITERAL, 12 ) );

    // === Format with offset tests (for %z) ===

    // Test various offset values with %z format
    auto z_fmt = parse_zone_format( "%z" );
    ASSERT_EQ( z_fmt.format( 0, false, "" ).sv(), "+00" );
    ASSERT_EQ( z_fmt.format( 1800, false, "" ).sv(), "+0030" );   // Half hour offset
    ASSERT_EQ( z_fmt.format( 5400, false, "" ).sv(), "+0130" );   // 1.5 hours
    ASSERT_EQ( z_fmt.format( 12600, false, "" ).sv(), "+0330" );  // 3.5 hours (Iran)
    ASSERT_EQ( z_fmt.format( 20700, false, "" ).sv(), "+0545" );  // Nepal
    ASSERT_EQ( z_fmt.format( -12600, false, "" ).sv(), "-0330" ); // Newfoundland

    // Test %z with seconds
    ASSERT_EQ( z_fmt.format( 3601, false, "" ).sv(), "+010001" );
    ASSERT_EQ( z_fmt.format( -3661, false, "" ).sv(), "-010101" );
}

namespace vtz {
    std::string _get_current_zone_name();
} // namespace vtz

TEST( vtz, current_zone ) {
    ASSERT_EQ( _get_current_zone_name(), date::current_zone()->name() );
    ASSERT_EQ( vtz::current_zone()->name(), date::current_zone()->name() );
}


TEST( vtz, reload_current_zone ) {
    ASSERT_EQ( _get_current_zone_name(), date::current_zone()->name() );
    auto* old_tz = vtz::current_zone();
    ASSERT_EQ( old_tz->name(), date::current_zone()->name() );

    std::string new_zone_name
        = old_tz->name() == "America/New_York"
              ? std::string( "America/Denver" )    // Switch to America/Denver
              : std::string( "America/New_York" ); // Switch to America/New_York

    // Ensure that the new zone is different
    ASSERT_NE( new_zone_name, vtz::current_zone()->name() );

    auto* new_tz = vtz::set_current_zone_for_application( new_zone_name );

    // check that the zone was updated
    ASSERT_EQ( new_tz, vtz::current_zone() );

    // Assert that the old zone is different than the new zone
    ASSERT_NE( old_tz, new_tz );

    ASSERT_EQ( vtz::current_zone()->name(), new_zone_name );

    ASSERT_NE( vtz::current_zone()->name(), date::current_zone()->name() );

    // Reload the current zone
    vtz::reload_current_zone();
    ASSERT_NE( new_zone_name, vtz::current_zone()->name() );

    ASSERT_EQ( vtz::current_zone()->name(), date::current_zone()->name() );

    // Assert that the 'restored' current_zone is the same as before
    ASSERT_EQ( old_tz, vtz::current_zone() );
}


namespace vtz::win32 {
    string_view windows_zone_to_native( string_view windows_zone );
} // namespace vtz::win32


TEST( vtz, windows_zones ) {
    std::pair<string_view, string_view> test_zones[]{
        { "Dateline Standard Time", "Etc/GMT+12" },
        { "UTC-11", "Etc/GMT+11" },
        { "Aleutian Standard Time", "America/Adak" },
        { "Hawaiian Standard Time", "Pacific/Honolulu" },
        { "Marquesas Standard Time", "Pacific/Marquesas" },
        { "Alaskan Standard Time", "America/Anchorage" },
        { "UTC-09", "Etc/GMT+9" },
        { "Pacific Standard Time (Mexico)", "America/Tijuana" },
        { "UTC-08", "Etc/GMT+8" },
        { "Pacific Standard Time", "America/Los_Angeles" },
        { "US Mountain Standard Time", "America/Phoenix" },
        { "Mountain Standard Time (Mexico)", "America/Mazatlan" },
        { "Mountain Standard Time", "America/Denver" },
        { "Yukon Standard Time", "America/Whitehorse" },
        { "Central America Standard Time", "America/Guatemala" },
        { "Central Standard Time", "America/Chicago" },
        { "Easter Island Standard Time", "Pacific/Easter" },
        { "Central Standard Time (Mexico)", "America/Mexico_City" },
        { "Canada Central Standard Time", "America/Regina" },
        { "SA Pacific Standard Time", "America/Bogota" },
        { "Eastern Standard Time (Mexico)", "America/Cancun" },
        { "Eastern Standard Time", "America/New_York" },
        { "Haiti Standard Time", "America/Port-au-Prince" },
        { "Cuba Standard Time", "America/Havana" },
        { "US Eastern Standard Time", "America/Indianapolis" },
        { "Turks And Caicos Standard Time", "America/Grand_Turk" },
        { "Paraguay Standard Time", "America/Asuncion" },
        { "Atlantic Standard Time", "America/Halifax" },
        { "Venezuela Standard Time", "America/Caracas" },
        { "Central Brazilian Standard Time", "America/Cuiaba" },
        { "SA Western Standard Time", "America/La_Paz" },
        { "Pacific SA Standard Time", "America/Santiago" },
        { "Newfoundland Standard Time", "America/St_Johns" },
        { "Tocantins Standard Time", "America/Araguaina" },
        { "E. South America Standard Time", "America/Sao_Paulo" },
        { "SA Eastern Standard Time", "America/Cayenne" },
        { "Argentina Standard Time", "America/Buenos_Aires" },
        { "Greenland Standard Time", "America/Godthab" },
        { "Montevideo Standard Time", "America/Montevideo" },
        { "Magallanes Standard Time", "America/Punta_Arenas" },
        { "Saint Pierre Standard Time", "America/Miquelon" },
        { "Bahia Standard Time", "America/Bahia" },
        { "UTC-02", "Etc/GMT+2" },
        { "Azores Standard Time", "Atlantic/Azores" },
        { "Cape Verde Standard Time", "Atlantic/Cape_Verde" },
        { "UTC", "Etc/UTC" },
        { "GMT Standard Time", "Europe/London" },
        { "Greenwich Standard Time", "Atlantic/Reykjavik" },
        { "Sao Tome Standard Time", "Africa/Sao_Tome" },
        { "Morocco Standard Time", "Africa/Casablanca" },
        { "W. Europe Standard Time", "Europe/Berlin" },
        { "Central Europe Standard Time", "Europe/Budapest" },
        { "Romance Standard Time", "Europe/Paris" },
        { "Central European Standard Time", "Europe/Warsaw" },
        { "W. Central Africa Standard Time", "Africa/Lagos" },
        { "Jordan Standard Time", "Asia/Amman" },
        { "GTB Standard Time", "Europe/Bucharest" },
        { "Middle East Standard Time", "Asia/Beirut" },
        { "Egypt Standard Time", "Africa/Cairo" },
        { "E. Europe Standard Time", "Europe/Chisinau" },
        { "Syria Standard Time", "Asia/Damascus" },
        { "West Bank Standard Time", "Asia/Hebron" },
        { "South Africa Standard Time", "Africa/Johannesburg" },
        { "FLE Standard Time", "Europe/Kiev" },
        { "Israel Standard Time", "Asia/Jerusalem" },
        { "South Sudan Standard Time", "Africa/Juba" },
        { "Kaliningrad Standard Time", "Europe/Kaliningrad" },
        { "Sudan Standard Time", "Africa/Khartoum" },
        { "Libya Standard Time", "Africa/Tripoli" },
        { "Namibia Standard Time", "Africa/Windhoek" },
        { "Arabic Standard Time", "Asia/Baghdad" },
        { "Turkey Standard Time", "Europe/Istanbul" },
        { "Arab Standard Time", "Asia/Riyadh" },
        { "Belarus Standard Time", "Europe/Minsk" },
        { "Russian Standard Time", "Europe/Moscow" },
        { "E. Africa Standard Time", "Africa/Nairobi" },
        { "Iran Standard Time", "Asia/Tehran" },
        { "Arabian Standard Time", "Asia/Dubai" },
        { "Astrakhan Standard Time", "Europe/Astrakhan" },
        { "Azerbaijan Standard Time", "Asia/Baku" },
        { "Russia Time Zone 3", "Europe/Samara" },
        { "Mauritius Standard Time", "Indian/Mauritius" },
        { "Saratov Standard Time", "Europe/Saratov" },
        { "Georgian Standard Time", "Asia/Tbilisi" },
        { "Volgograd Standard Time", "Europe/Volgograd" },
        { "Caucasus Standard Time", "Asia/Yerevan" },
        { "Afghanistan Standard Time", "Asia/Kabul" },
        { "West Asia Standard Time", "Asia/Tashkent" },
        { "Ekaterinburg Standard Time", "Asia/Yekaterinburg" },
        { "Pakistan Standard Time", "Asia/Karachi" },
        { "Qyzylorda Standard Time", "Asia/Qyzylorda" },
        { "India Standard Time", "Asia/Calcutta" },
        { "Sri Lanka Standard Time", "Asia/Colombo" },
        { "Nepal Standard Time", "Asia/Katmandu" },
        { "Central Asia Standard Time", "Asia/Bishkek" },
        { "Bangladesh Standard Time", "Asia/Dhaka" },
        { "Omsk Standard Time", "Asia/Omsk" },
        { "Myanmar Standard Time", "Asia/Rangoon" },
        { "SE Asia Standard Time", "Asia/Bangkok" },
        { "Altai Standard Time", "Asia/Barnaul" },
        { "W. Mongolia Standard Time", "Asia/Hovd" },
        { "North Asia Standard Time", "Asia/Krasnoyarsk" },
        { "N. Central Asia Standard Time", "Asia/Novosibirsk" },
        { "Tomsk Standard Time", "Asia/Tomsk" },
        { "China Standard Time", "Asia/Shanghai" },
        { "North Asia East Standard Time", "Asia/Irkutsk" },
        { "Singapore Standard Time", "Asia/Singapore" },
        { "W. Australia Standard Time", "Australia/Perth" },
        { "Taipei Standard Time", "Asia/Taipei" },
        { "Ulaanbaatar Standard Time", "Asia/Ulaanbaatar" },
        { "Aus Central W. Standard Time", "Australia/Eucla" },
        { "Transbaikal Standard Time", "Asia/Chita" },
        { "Tokyo Standard Time", "Asia/Tokyo" },
        { "North Korea Standard Time", "Asia/Pyongyang" },
        { "Korea Standard Time", "Asia/Seoul" },
        { "Yakutsk Standard Time", "Asia/Yakutsk" },
        { "Cen. Australia Standard Time", "Australia/Adelaide" },
        { "AUS Central Standard Time", "Australia/Darwin" },
        { "E. Australia Standard Time", "Australia/Brisbane" },
        { "AUS Eastern Standard Time", "Australia/Sydney" },
        { "West Pacific Standard Time", "Pacific/Port_Moresby" },
        { "Tasmania Standard Time", "Australia/Hobart" },
        { "Vladivostok Standard Time", "Asia/Vladivostok" },
        { "Lord Howe Standard Time", "Australia/Lord_Howe" },
        { "Bougainville Standard Time", "Pacific/Bougainville" },
        { "Russia Time Zone 10", "Asia/Srednekolymsk" },
        { "Magadan Standard Time", "Asia/Magadan" },
        { "Norfolk Standard Time", "Pacific/Norfolk" },
        { "Sakhalin Standard Time", "Asia/Sakhalin" },
        { "Central Pacific Standard Time", "Pacific/Guadalcanal" },
        { "Russia Time Zone 11", "Asia/Kamchatka" },
        { "New Zealand Standard Time", "Pacific/Auckland" },
        { "UTC+12", "Etc/GMT-12" },
        { "Fiji Standard Time", "Pacific/Fiji" },
        { "Chatham Islands Standard Time", "Pacific/Chatham" },
        { "UTC+13", "Etc/GMT-13" },
        { "Tonga Standard Time", "Pacific/Tongatapu" },
        { "Samoa Standard Time", "Pacific/Apia" },
        { "Line Islands Standard Time", "Pacific/Kiritimati" },
    };


    for( auto const& [windows_name, IANA_name] : test_zones )
    {
        // Check that the windows zone name matches the IANA name
        ASSERT_EQ( win32::windows_zone_to_native( windows_name ), IANA_name );
    }
}
