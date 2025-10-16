
#include <vtz/date_types.h>
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

static_assert( _impl::_parseMon( "blarg", 3 ) == none );
static_assert( _impl::_parseMon( "Jan", 3 ).value() == Mon::Jan );
static_assert( _impl::_parseMon( "Feb", 3 ).value() == Mon::Feb );
static_assert( _impl::_parseMon( "Mar", 3 ).value() == Mon::Mar );
static_assert( _impl::_parseMon( "Apr", 3 ).value() == Mon::Apr );
static_assert( _impl::_parseMon( "May", 3 ).value() == Mon::May );
static_assert( _impl::_parseMon( "Jun", 3 ).value() == Mon::Jun );
static_assert( _impl::_parseMon( "Jul", 3 ).value() == Mon::Jul );
static_assert( _impl::_parseMon( "Aug", 3 ).value() == Mon::Aug );
static_assert( _impl::_parseMon( "Sep", 3 ).value() == Mon::Sep );
static_assert( _impl::_parseMon( "Oct", 3 ).value() == Mon::Oct );
static_assert( _impl::_parseMon( "Nov", 3 ).value() == Mon::Nov );
static_assert( _impl::_parseMon( "Dec", 3 ).value() == Mon::Dec );

static_assert( _impl::_parseDOW( "blarg", 3 ) == none );
static_assert( _impl::_parseDOW( "Sun", 3 ).value() == DOW::Sun );
static_assert( _impl::_parseDOW( "Mon", 3 ).value() == DOW::Mon );
static_assert( _impl::_parseDOW( "Tue", 3 ).value() == DOW::Tue );
static_assert( _impl::_parseDOW( "Wed", 3 ).value() == DOW::Wed );
static_assert( _impl::_parseDOW( "Thu", 3 ).value() == DOW::Thu );
static_assert( _impl::_parseDOW( "Fri", 3 ).value() == DOW::Fri );
static_assert( _impl::_parseDOW( "Sat", 3 ).value() == DOW::Sat );
static_assert( _impl::_parseDOW( "Su", 2 ).value() == DOW::Sun );
static_assert( _impl::_parseDOW( "Mo", 2 ).value() == DOW::Mon );
static_assert( _impl::_parseDOW( "Tu", 2 ).value() == DOW::Tue );
static_assert( _impl::_parseDOW( "We", 2 ).value() == DOW::Wed );
static_assert( _impl::_parseDOW( "Th", 2 ).value() == DOW::Thu );
static_assert( _impl::_parseDOW( "Fr", 2 ).value() == DOW::Fri );
static_assert( _impl::_parseDOW( "Sa", 2 ).value() == DOW::Sat );
static_assert( _impl::_parseDOW( "M", 1 ).value() == DOW::Mon );
static_assert( _impl::_parseDOW( "W", 1 ).value() == DOW::Wed );
static_assert( _impl::_parseDOW( "F", 1 ).value() == DOW::Fri );


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

static_assert( RuleLetter().sv() == "" );
static_assert( RuleLetter().size_ == 0 );
static_assert( RuleLetter( "x" ).sv() == "x" );
static_assert( RuleLetter( "x" ) == RuleLetter( "x" ) );

DECLARE_STRINGLIKE( RuleLetter );

namespace {
    template<size_t... N>
    std::vector<string_view> vecSV( char const ( &... arr )[N] ) {
        return std::vector<string_view>{ string_view( arr, N - 1 )... };
    }

    void checkLines( string_view input, std::vector<string_view> lines ) {
        auto reader = LineIter( input );
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

    void checkTokens( string_view input, std::vector<string_view> tokens ) {
        auto reader = TokenIter( input );
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
    checkTokens( "", vecSV() );
    checkTokens( "    ", vecSV() );
    checkTokens( "  \t \t", vecSV() );

    // Placement of whitespace does not matter
    checkTokens( "word", vecSV( "word" ) );
    checkTokens( "  word", vecSV( "word" ) );
    checkTokens( "word  ", vecSV( "word" ) );

    checkTokens( "word", vecSV( "word" ) );
    checkTokens( " \t word", vecSV( "word" ) );
    checkTokens( "word \t ", vecSV( "word" ) );

    checkTokens( "hello world", vecSV( "hello", "world" ) );
    checkTokens( "hello\tworld", vecSV( "hello", "world" ) );
    checkTokens( "   hello\tworld \t \t", vecSV( "hello", "world" ) );

    // clang-format off
        checkTokens( "Zone America/Los_Angeles    -7:52:58\t -	LMT	1883 Nov 18 20:00u",
            vecSV( "Zone",
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
    ASSERT_EQ( countLines( "" ), 0 );
    ASSERT_EQ( countLines( "\n" ), 1 );
    ASSERT_EQ( countLines( "hello" ), 1 );
    ASSERT_EQ( countLines( "hello\n" ), 1 );
    ASSERT_EQ( countLines( "hello\nworld" ), 2 );
    ASSERT_EQ( countLines( "hello\nworld\n" ), 2 );
    ASSERT_EQ( countLines( "hello\r\nworld\n" ), 2 );
    ASSERT_EQ( countLines( "hello\n"
                           "\n"
                           "The quick brown fox jumps over the lazy dogs.\n"
                           "End.\n" ),
        4 );
    ASSERT_EQ( countLines( "hello\n"
                           "\n"
                           "The quick brown fox jumps over the lazy dogs.\n"
                           "End." ),
        4 );

    ASSERT_EQ( countLines( "hello\n"
                           "\n"
                           "\n"
                           "\n"
                           "\n" ),
        5 );

    checkLines( "", vecSV() );
    checkLines( "\n", vecSV( "" ) );
    checkLines( "hello", vecSV( "hello" ) );
    checkLines( "hello\n", vecSV( "hello" ) );
    checkLines( "hello\nworld", vecSV( "hello", "world" ) );
    checkLines( "hello\nworld\n", vecSV( "hello", "world" ) );
    checkLines( "hello\r\nworld", vecSV( "hello", "world" ) );
    checkLines( "hello\r\nworld\r\n", vecSV( "hello", "world" ) );

    // Only '\r\n' is recognized and stripped; '\r' on it's own is not
    // stripped
    checkLines( "hello\r\nworld\r", vecSV( "hello", "world\r" ) );

    checkLines( "hello\n"
                "\n"
                "The quick brown fox jumps over the lazy dogs.\n"
                "End.\n",
        vecSV( "hello", "", "The quick brown fox jumps over the lazy dogs.", "End." ) );

    checkLines( "hello\n"
                "\n"
                "The quick brown fox jumps over the lazy dogs.\n"
                "End.",
        vecSV( "hello", "", "The quick brown fox jumps over the lazy dogs.", "End." ) );

    checkLines( "hello\n"
                "\n"
                "\n"
                "\n"
                "\n",
        vecSV( "hello", "", "", "", "" ) );
}


TEST( vtz_parser, strip_comments ) {
    ASSERT_EQ( stripComment( string_view() ), "" );
    ASSERT_EQ( stripComment( "# This is a comment" ), "" );
    ASSERT_EQ( stripComment( "#STDOFF" ), "" );
    ASSERT_EQ( stripComment( "        #STDOFF" ), "        " );
    ASSERT_EQ( stripComment( "hello # This is a comment" ), "hello " );
    ASSERT_EQ( stripComment( "        hello #STDOFF" ), "        hello " );
}

STRUCT_INFO( vtz::Location, FIELD( vtz::Location, line ), FIELD( vtz::Location, col ) );

STRUCT_INFO( vtz::RuleEntry,
    FIELD( vtz::RuleEntry, from ),
    FIELD( vtz::RuleEntry, to ),
    FIELD( vtz::RuleEntry, in ),
    FIELD( vtz::RuleEntry, on ),
    FIELD( vtz::RuleEntry, at ),
    FIELD( vtz::RuleEntry, save ),
    FIELD( vtz::RuleEntry, letter ) );

STRUCT_INFO( vtz::ZoneEntry,
    FIELD( vtz::ZoneEntry, stdoff ),
    FIELD( vtz::ZoneEntry, rules ),
    FIELD( vtz::ZoneEntry, format ),
    FIELD( vtz::ZoneEntry, until ) );

STRUCT_INFO( vtz::Zone, FIELD( vtz::Zone, name ), FIELD( vtz::Zone, ents ) );

STRUCT_INFO( vtz::Link, FIELD( vtz::Link, canonical ), FIELD( vtz::Link, alias ) );

STRUCT_INFO( vtz::TZDataFile,
    FIELD( vtz::TZDataFile, rules ),
    FIELD( vtz::TZDataFile, zones ),
    FIELD( vtz::TZDataFile, links ) );


using namespace vtz;
TEST( vtz_parser, parse_tzdata ) {
    using ze = ZoneEntry;
    using re = RuleEntry;
    using M  = Mon;

    constexpr auto lastSun = RuleOn::last( DOW::Sun );
    constexpr auto on      = []( u8 day ) { return RuleOn::on( day ); };

    ASSERT_EQ( parseTZData(
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

    ASSERT_EQ( parseTZData(
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


    ASSERT_EQ( parseTZData(
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
    ASSERT_EQ( parseTZData(
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
    ASSERT_EQ( parseTZData(
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
    ASSERT_EQ( Location( 1, 1 ), Location::where( "" ) );
    ASSERT_EQ( Location( 2, 1 ), Location::where( "\n" ) );
    ASSERT_EQ( Location( 3, 1 ), Location::where( "\n\n" ) );

    ASSERT_EQ( Location( 1, 6 ), Location::where( "hello" ) );
    ASSERT_EQ( Location( 2, 6 ), Location::where( "\nhello" ) );
    ASSERT_EQ( Location( 3, 6 ), Location::where( "\n\nhello" ) );
    ASSERT_EQ( Location( 4, 1 ), Location::where( "\n\nhello\n" ) );

    ASSERT_EQ( "1:1", Location::where( "" ).str() );
    ASSERT_EQ( "2:1", Location::where( "\n" ).str() );
    ASSERT_EQ( "3:1", Location::where( "\n\n" ).str() );
    ASSERT_EQ( "1:6", Location::where( "hello" ).str() );
    ASSERT_EQ( "2:6", Location::where( "\nhello" ).str() );
    ASSERT_EQ( "3:6", Location::where( "\n\nhello" ).str() );
    ASSERT_EQ( "4:1", Location::where( "\n\nhello\n" ).str() );

    string_view body = "line1\n"
                       "line2\n"
                       "line3\n"
                       "line4\n";

    ASSERT_EQ( Location( 1, 1 ), Location::where( body, 0 ) );
    ASSERT_EQ( Location( 1, 6 ), Location::where( body, body.find( '\n' ) ) );
    ASSERT_EQ( Location( 4, 5 ), Location::where( body, body.find( '4' ) ) );
    ASSERT_EQ( Location( 5, 1 ), Location::where( body, body.size() ) );

    string_view body2 = R"(Link	Africa/Abidjan	Africa/Accra
Link	Africa/Abidjan	Africa/Bamako
Zone Pacific/Honolulu	-10:31:26 -	LMT	1896 Jan 13 12:00
			-10:30	-	HST	1933 Apr 30  2:00
			-10:30	1:00	HDT	1933 May 21 12:00
			-10:30	US	H%sT	1947 Jun  8  2:00
			-10:00	-	HST)";
    ASSERT_EQ( "3:6", Location::where( body2, body2.find( "Pacific/Honolulu" ) ).str() );

    ASSERT_EQ( "18446744073709551615:18446744073709551615",
        Location( 18446744073709551615ul, 18446744073709551615ul ).str() );
}


TEST( vtz_io, read_file ) {
    using namespace std::literals;
    // Check that we can read a text file
    ASSERT_EQ( readFile( "etc/testdata/hello.txt" ), "hello" );

    // Check that we can read a binary file
    ASSERT_EQ( readFile( "etc/testdata/test_file.bin" ),
        "z!\xFB;rh\x9F\xEB"
        "f\x84\xB8\xE5\xA6\xBC"
        "d\x9F\0\xD3\b\xE5"s );

    EXPECT_THROW(
        try {
            // Attempt to open and read the file
            auto s = readFile( nullptr );
        } catch( std::exception const& ex ) {
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );


    EXPECT_THROW(
        try {
            // Attempt to open and read the file
            auto s = readFile( "" );
        } catch( std::exception const& ex ) {
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // Attempt to open and read the file
            auto s = readFile( "this-file-does-not-exist.txt" );
        } catch( std::exception const& ex ) {
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );
}


TEST( vtz_parser, parse_errors ) {
    EXPECT_THROW(
        try {
            // clang-format off
            parseTZData(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb2	9	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parseTZData(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	# Feb	9	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parseTZData(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942x	only	-	Feb	9	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parseTZData(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	92	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: Day of Month is out of range
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parseTZData(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	9x	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: Day of Month is not completely parsed
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parseTZData(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	0	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: Day of Month is out of range (0 is out of range)
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parseTZData(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	x9	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: Day of Month is not an int
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );

    EXPECT_THROW(
        try {
            // clang-format off
            parseTZData(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	lastSud	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: last[DOW] does not have a valid DOW
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );

    // This is fine
    // clang-format off
    EXPECT_NO_THROW(
        parseTZData(
            "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
            "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
            "Rule	US	1942	only	-	Feb	Tue>=13	2:00	1:00	W # War\n" ) );

    // Also fine - we can have [DOW]<=[DOM]
    EXPECT_NO_THROW(
        parseTZData(
            "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
            "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
            "Rule	US	1942	only	-	Feb	Fri<=1	2:00	1:00	W # War\n" ) );
    // clang-format on

    EXPECT_THROW(
        try {
            // clang-format off
            parseTZData(
                "Rule	US	1918	1919	-	Mar	lastSun	2:00	1:00	D\n"
                "Rule	US	1918	1919	-	Oct	lastSun	2:00	0	S\n"
                "Rule	US	1942	only	-	Feb	Sud>=13	2:00	1:00	W # War\n" );
            // clang-format on
        } catch( std::exception const& ex ) {
            // BAD: last[DOW] does not have a valid DOW
            TEST_LOG.logGood( "error message", ex.what() );
            throw;
        },
        std::exception );
}


namespace {
    std::pair<string, TZDataFile> testLoadFile( char const* fp ) {
        using HRC = std::chrono::high_resolution_clock;
        using std::chrono::duration_cast;

        auto t0      = HRC::now();
        auto content = readFile( fp );
        auto t1      = HRC::now();
        auto result  = parseTZData( content, fp );
        auto t2      = HRC::now();

        using msR = std::chrono::duration<double, std::micro>;


        TEST_LOG.logGood( " successfully loaded", fp );
        TEST_LOG.logGood( "    time read_file()", duration_cast<msR>( t1 - t0 ) );
        TEST_LOG.logGood( " time parse_tzdata()", duration_cast<msR>( t2 - t1 ) );
        TEST_LOG.logGood( "               total", duration_cast<msR>( t2 - t0 ) );

        return { std::move( content ), std::move( result ) };
    }
} // namespace


TEST( vtz_io, load_all ) {
    testLoadFile( "build/data/tzdata/africa" );
    testLoadFile( "build/data/tzdata/antarctica" );
    testLoadFile( "build/data/tzdata/asia" );
    testLoadFile( "build/data/tzdata/australasia" );
    testLoadFile( "build/data/tzdata/backward" );
    testLoadFile( "build/data/tzdata/backzone" );
    testLoadFile( "build/data/tzdata/etcetera" );
    testLoadFile( "build/data/tzdata/europe" );
    testLoadFile( "build/data/tzdata/factory" );
    testLoadFile( "build/data/tzdata/northamerica" );
    testLoadFile( "build/data/tzdata/southamerica" );
    auto [file, allZones] = testLoadFile( "build/data/tzdata/tzdata.zi" );

    ASSERT_EQ( allZones.links["GMT"], "Etc/GMT" );
    ASSERT_EQ( allZones.links["EST5EDT"], "America/New_York" );
}


TEST( vtz_tz, US_rules ) {
    auto [content, zones] = testLoadFile( "build/data/tzdata/northamerica" );

    auto US = zones.evaluateRules( "US" );

    using RE = RuleEntry;
    using RT = RuleTrans;

    // https://www.timeanddate.com/calendar/?year=1919&country=1
    ASSERT_EQ( US.yearStart, 1918 );
    ASSERT_EQ( US.yearEnd, 2007 );

    // There were 7 transitions before 1967, and then 2 transitions a year between 1967 and 2007
    // Transitions within or after 2007 are "active". We're currently using the 2007 rules,
    // so they are not listed as historical.
    ASSERT_EQ( US.historical.size(), 7 + ( 2007 - 1967 ) * 2 );

    ASSERT_EQ( US.historical[0], RT::fromCivil( 1918, 3, 31, "2:00", "1:00", "D" ) );
    ASSERT_EQ( US.historical[1], RT::fromCivil( 1918, 10, 27, "2:00", "0", "S" ) );
    ASSERT_EQ( US.historical[2], RT::fromCivil( 1919, 3, 30, "2:00", "1:00", "D" ) );
    ASSERT_EQ( US.historical[3], RT::fromCivil( 1919, 10, 26, "2:00", "0", "S" ) );

    // Enter War Time on February 9th, 1942
    ASSERT_EQ( US.historical[4], RT::fromCivil( 1942, 2, 9, "2:00", "1:00", "W" ) );
    // Transition to Peace Time on August 14th, 1945
    ASSERT_EQ( US.historical[5], RT::fromCivil( 1945, 8, 14, "23:00u", "1:00", "P" ) );

    // On September 30th, 1945, the US moved back to Standard Time
    ASSERT_EQ( US.historical[6], RT::fromCivil( 1945, 9, 30, "2:00", "0", "S" ) );

    // We started to do Daylight Savings Time again for some fucking reasons
    ASSERT_EQ( US.historical[7], RT::fromCivil( 1967, 4, 30, "2:00", "1:00", "D" ) );
    ASSERT_EQ( US.historical[8], RT::fromCivil( 1967, 10, 29, "2:00", "0", "S" ) );

    // Historical rules apply until 2006. From then onwards we started using the current set of
    // rules!
    size_t n = US.historical.size();
    ASSERT_EQ( US.historical[n - 2], RT::fromCivil( 2006, 4, 2, "2:00", "1:00", "D" ) );
    ASSERT_EQ( US.historical[n - 1], RT::fromCivil( 2006, 10, 29, "2:00", "0", "S" ) );

    ASSERT_EQ( US.active.at( 0 ),
        ( RE{ 2007, Y_MAX, Mon::Mar, RuleOn::ge( DOW::Sun, 8 ), "2:00", "1:00", "D" } ) );
    ASSERT_EQ( US.active.at( 1 ),
        ( RE{ 2007, Y_MAX, Mon::Nov, RuleOn::ge( DOW::Sun, 1 ), "2:00", "0", "S" } ) );
}


TEST( vtz_parser, basics ) {
    ASSERT_EQ( parseHHMMSSOffset( "0:12:12" ), 60 * 12 + 12 );
    ASSERT_EQ( parseHHMMSSOffset( "2:00" ), 7200 );
    ASSERT_EQ( parseSignedHHMMSSOffset( "2:00" ), 7200 );
    ASSERT_EQ( parseSignedHHMMSSOffset( "-2:00" ), -7200 );

    using RO = RuleOn;
    ASSERT_EQ( parseRuleOn( "lastSun" ).string(), "lastSun" );
    ASSERT_EQ( parseRuleOn( "lastMon" ).string(), "lastMon" );
    ASSERT_EQ( parseRuleOn( "lastTue" ).string(), "lastTue" );
    ASSERT_EQ( parseRuleOn( "lastWed" ).string(), "lastWed" );
    ASSERT_EQ( parseRuleOn( "lastThu" ).string(), "lastThu" );
    ASSERT_EQ( parseRuleOn( "lastFri" ).string(), "lastFri" );
    ASSERT_EQ( parseRuleOn( "lastSat" ).string(), "lastSat" );

    ASSERT_EQ( parseRuleOn( "Sun>=1" ).string(), "Sun>=1" );
    ASSERT_EQ( parseRuleOn( "Mon>=2" ).string(), "Mon>=2" );
    ASSERT_EQ( parseRuleOn( "Tue>=3" ).string(), "Tue>=3" );
    ASSERT_EQ( parseRuleOn( "Wed>=10" ).string(), "Wed>=10" );
    ASSERT_EQ( parseRuleOn( "Thu>=11" ).string(), "Thu>=11" );
    ASSERT_EQ( parseRuleOn( "Fri>=12" ).string(), "Fri>=12" );
    ASSERT_EQ( parseRuleOn( "Sat>=31" ).string(), "Sat>=31" );

    ASSERT_EQ( parseRuleOn( "Sun<=1" ).string(), "Sun<=1" );
    ASSERT_EQ( parseRuleOn( "Mon<=2" ).string(), "Mon<=2" );
    ASSERT_EQ( parseRuleOn( "Tue<=3" ).string(), "Tue<=3" );
    ASSERT_EQ( parseRuleOn( "Wed<=10" ).string(), "Wed<=10" );
    ASSERT_EQ( parseRuleOn( "Thu<=11" ).string(), "Thu<=11" );
    ASSERT_EQ( parseRuleOn( "Fri<=12" ).string(), "Fri<=12" );
    ASSERT_EQ( parseRuleOn( "Sat<=31" ).string(), "Sat<=31" );

    ASSERT_ANY_THROW( parseRuleOn( "Sun<=0" ); );
}
