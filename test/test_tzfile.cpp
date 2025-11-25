#include <vtz/endian.h>


#include "test_utils.h"
#include "test_zones.h"
#include "vtz_testing.h"


TEST( vtz, endian ) {
    using vtz::endian::load_be;
    using vtz::endian::span_be;
    // Test _load_be

    constexpr char buff1[1]{ '\xde' };
    constexpr char buff2[2]{ '\xde', '\xad' };
    constexpr char buff4[4]{ '\xde', '\xad', '\xbe', '\xef' };
    constexpr char buff8[8]{ '\xde', '\xad', '\xbe', '\xef', '\1', '\2', '\3', '\4' };

    ASSERT_EQ( load_be<u8>( buff1 ), 0xDEu );
    ASSERT_EQ( load_be<u16>( buff2 ), 0xDEADu );
    ASSERT_EQ( load_be<u32>( buff4 ), 0xDEADBEEFu );
    ASSERT_EQ( load_be<u64>( buff8 ), 0xDEADBEEF01020304u );
    ASSERT_EQ( load_be<i8>( buff1 ), i8( 0xDEu ) );
    ASSERT_EQ( load_be<i16>( buff2 ), i16( 0xDEADu ) );
    ASSERT_EQ( load_be<i32>( buff4 ), i32( 0xDEADBEEFu ) );
    ASSERT_EQ( load_be<i64>( buff8 ), i64( 0xDEADBEEF01020304u ) );

    auto s1 = span_be<u8>( buff1, 1 );
    auto s2 = span_be<u16>( buff2, 1 );
    auto s4 = span_be<u32>( buff4, 1 );
    auto s8 = span_be<u64>( buff8, 1 );

    ASSERT_EQ( s1.size(), 1 );
    ASSERT_EQ( s2.size(), 1 );
    ASSERT_EQ( s4.size(), 1 );
    ASSERT_EQ( s8.size(), 1 );

    ASSERT_EQ( s1.end() - s1.begin(), 1 );
    ASSERT_EQ( s2.end() - s2.begin(), 1 );
    ASSERT_EQ( s4.end() - s4.begin(), 1 );
    ASSERT_EQ( s8.end() - s8.begin(), 1 );

    ASSERT_EQ( s1.end().p - s1.begin().p, 1 );
    ASSERT_EQ( s2.end().p - s2.begin().p, 2 );
    ASSERT_EQ( s4.end().p - s4.begin().p, 4 );
    ASSERT_EQ( s8.end().p - s8.begin().p, 8 );

    ASSERT_EQ( s1[0], 0xDEu );
    ASSERT_EQ( s2[0], 0xDEADu );
    ASSERT_EQ( s4[0], 0xDEADBEEFu );
    ASSERT_EQ( s8[0], 0xDEADBEEF01020304u );

    ASSERT_EQ( s1.begin() - s1.end(), -1 );
    ASSERT_EQ( s2.begin() - s2.end(), -1 );
    ASSERT_EQ( s4.begin() - s4.end(), -1 );
    ASSERT_EQ( s8.begin() - s8.end(), -1 );

    ASSERT_EQ( s1.size_bytes(), 1 );
    ASSERT_EQ( s2.size_bytes(), 2 );
    ASSERT_EQ( s4.size_bytes(), 4 );
    ASSERT_EQ( s8.size_bytes(), 8 );

    auto ss = span_be<u16>( buff8, 4 );
    ASSERT_EQ( ss[0], 0xDEAD );
    ASSERT_EQ( ss[1], 0xBEEF );
    ASSERT_EQ( ss[2], 0x0102 );
    ASSERT_EQ( ss[3], 0x0304 );
    ASSERT_EQ( ss.begin()[0], 0xDEAD );
    ASSERT_EQ( ss.begin()[1], 0xBEEF );
    ASSERT_EQ( ss.begin()[2], 0x0102 );
    ASSERT_EQ( ss.begin()[3], 0x0304 );
    ASSERT_EQ( ss.end() - ss.begin(), 4 );
    ASSERT_EQ( ss.begin() - ss.end(), -4 );

    ASSERT_EQ( ( std::vector<u16>{ 0xdead, 0xbeef, 0x0102, 0x0304 } ),
        std::vector<u16>( ss.begin(), ss.end() ) );
}
