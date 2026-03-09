
#include "vtz_testing.h"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <vtz/util/microfmt.h>

using namespace vtz::util;

TEST( microfmt, max_space ) {
    ASSERT_EQ( max_space( 'x' ), size_t( 1 ) );
    ASSERT_EQ( max_space( "hello" ), size_t( 5 ) );

    std::string_view sv = "test";
    ASSERT_EQ( max_space( sv ), size_t( 4 ) );
    ASSERT_EQ( max_space( std::string_view() ), size_t( 0 ) );

    ASSERT_EQ( max_space( 0 ), size_t( 11 ) );
    ASSERT_EQ( max_space( 0u ), size_t( 10 ) );
    ASSERT_EQ( max_space( INT64_MAX ), size_t( 20 ) );
    ASSERT_EQ( max_space( UINT64_MAX ), size_t( 20 ) );
    ASSERT_EQ( max_space( INT64_MIN ), size_t( 20 ) );
}

TEST( microfmt, dump ) {
    char   buf[32] = {};
    size_t n;

    // char
    n = dump( buf, 'A' );
    ASSERT_EQ( n, size_t( 1 ) );
    ASSERT_EQ( buf[0], 'A' );

    // string literal
    n = dump( buf, "hello" );
    ASSERT_EQ( n, size_t( 5 ) );
    ASSERT_EQ( std::string_view( buf, n ), "hello" );

    // string_view
    std::string_view sv = "test";
    n                   = dump( buf, sv );
    ASSERT_EQ( n, size_t( 4 ) );
    ASSERT_EQ( std::string_view( buf, n ), "test" );

    // empty string_view
    n = dump( buf, std::string_view() );
    ASSERT_EQ( n, size_t( 0 ) );

    // std::string
    std::string s = "abc";
    n             = dump( buf, s );
    ASSERT_EQ( n, size_t( 3 ) );
    ASSERT_EQ( std::string_view( buf, n ), "abc" );

    // char const*
    char const* p = "xyz";
    n             = dump( buf, p );
    ASSERT_EQ( n, size_t( 3 ) );
    ASSERT_EQ( std::string_view( buf, n ), "xyz" );

    // integers
    n = dump( buf, 42 );
    ASSERT_EQ( std::string_view( buf, n ), "42" );

    n = dump( buf, -7 );
    ASSERT_EQ( std::string_view( buf, n ), "-7" );

    n = dump( buf, 0 );
    ASSERT_EQ( std::string_view( buf, n ), "0" );

    n = dump( buf, 12345u );
    ASSERT_EQ( std::string_view( buf, n ), "12345" );

    n = dump( buf, (long long)INT64_MAX );
    ASSERT_EQ( std::string_view( buf, n ), "9223372036854775807" );

    n = dump( buf, (unsigned long long)UINT64_MAX );
    ASSERT_EQ( std::string_view( buf, n ), "18446744073709551615" );
}

TEST( microfmt, join ) {
    ASSERT_EQ( join(), std::string() );
    ASSERT_EQ( join( "hello" ), std::string( "hello" ) );
    ASSERT_EQ( join( 42 ), std::string( "42" ) );
    ASSERT_EQ( join( "value=", 99, ", ok=", 'Y' ), std::string( "value=99, ok=Y" ) );
    ASSERT_EQ( join( "x=", -123 ), std::string( "x=-123" ) );

    std::string_view sv = "world";
    ASSERT_EQ( join( "hello ", sv ), std::string( "hello world" ) );

    std::string part = "bar";
    ASSERT_EQ( join( "foo", part, "baz" ), std::string( "foobarbaz" ) );

    ASSERT_EQ( join( 'a', 'b', 'c' ), std::string( "abc" ) );

    // Force the heap-allocation path (stack buffer is 16384 bytes)
    std::string big( 20000, 'X' );
    auto        s = join( std::string_view( big ) );
    ASSERT_EQ( s.size(), size_t( 20000 ) );
    ASSERT_EQ_QUIET( s, big );

    std::string big2( 16000, 'A' );
    auto        s2 = join( std::string_view( big2 ), big2, "---trailing" );
    ASSERT_EQ_QUIET( s2, big2 + big2 + "---trailing" );

    // joined range
    std::vector<std::string> empty_vec;
    ASSERT_EQ( join( joined{ empty_vec, ", " } ), std::string() );

    std::vector<std::string> one = { "only" };
    ASSERT_EQ( join( joined{ one, ", " } ), std::string( "only" ) );

    std::vector<std::string> files = { "a.txt", "b.txt", "c.txt" };
    ASSERT_EQ( join( "files: [", joined{ files, ", " }, "]" ),
        std::string( "files: [a.txt, b.txt, c.txt]" ) );
}
