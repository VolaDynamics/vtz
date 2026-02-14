
#include "vtz_testing.h"
#include <climits>
#include <random>
#include <vtz/impl/bit.h>
#include <vtz/impl/math.h>

using namespace vtz;

namespace ref {
    /// Compute the largest p such that 2^p >= x
    constexpr int blog2( u64 x ) {
        int i = 0;
        while( x > 1 )
        {
            ++i;
            x >>= 1;
        }
        return i;
    }

    static_assert( blog2( 1 ) == 0 );
    static_assert( blog2( 2 ) == 1 );
    static_assert( blog2( 3 ) == 1 );
    static_assert( blog2( 4 ) == 2 );
    static_assert( blog2( 7 ) == 2 );
    static_assert( blog2( 8 ) == 3 );
    static_assert( blog2( 15 ) == 3 );
    static_assert( blog2( 16 ) == 4 );

} // namespace ref

static_assert( _blog2_fallback( 1 << 0 ) == 0 );
static_assert( _blog2_fallback( 1 << 1 ) == 1 );
static_assert( _blog2_fallback( 1 << 2 ) == 2 );
static_assert( _blog2_fallback( 1 << 3 ) == 3 );
static_assert( _blog2_fallback( 1 << 4 ) == 4 );
static_assert( _blog2_fallback( 1 << 5 ) == 5 );
static_assert( _blog2_fallback( 1 << 6 ) == 6 );
static_assert( _blog2_fallback( 1 << 7 ) == 7 );
static_assert( _blog2_fallback( 1 << 8 ) == 8 );
static_assert( _blog2_fallback( 1 << 9 ) == 9 );
static_assert( _blog2_fallback( 1 << 10 ) == 10 );
static_assert( _blog2_fallback( 1 << 11 ) == 11 );
static_assert( _blog2_fallback( 1 << 12 ) == 12 );
static_assert( _blog2_fallback( 1 << 13 ) == 13 );
static_assert( _blog2_fallback( 1 << 14 ) == 14 );
static_assert( _blog2_fallback( 1 << 15 ) == 15 );
static_assert( _blog2_fallback( 1 << 16 ) == 16 );
static_assert( _blog2_fallback( 1 << 17 ) == 17 );
static_assert( _blog2_fallback( 1 << 18 ) == 18 );
static_assert( _blog2_fallback( 1 << 19 ) == 19 );
static_assert( _blog2_fallback( 1 << 20 ) == 20 );
static_assert( _blog2_fallback( 1 << 21 ) == 21 );
static_assert( _blog2_fallback( 1 << 22 ) == 22 );
static_assert( _blog2_fallback( 1 << 23 ) == 23 );
static_assert( _blog2_fallback( 1 << 24 ) == 24 );
static_assert( _blog2_fallback( 1 << 25 ) == 25 );
static_assert( _blog2_fallback( 1 << 26 ) == 26 );
static_assert( _blog2_fallback( 1 << 27 ) == 27 );
static_assert( _blog2_fallback( 1 << 28 ) == 28 );
static_assert( _blog2_fallback( 1 << 29 ) == 29 );
static_assert( _blog2_fallback( UINT64_MAX ) == 63 );

static_assert( _blog2( 1 << 0 ) == 0 );
static_assert( _blog2( 1 << 1 ) == 1 );
static_assert( _blog2( 1 << 2 ) == 2 );
static_assert( _blog2( 1 << 3 ) == 3 );
static_assert( _blog2( 1 << 4 ) == 4 );
static_assert( _blog2( 1 << 5 ) == 5 );
static_assert( _blog2( 1 << 6 ) == 6 );
static_assert( _blog2( 1 << 7 ) == 7 );
static_assert( _blog2( 1 << 8 ) == 8 );
static_assert( _blog2( 1 << 9 ) == 9 );
static_assert( _blog2( 1 << 10 ) == 10 );
static_assert( _blog2( 1 << 11 ) == 11 );
static_assert( _blog2( 1 << 12 ) == 12 );
static_assert( _blog2( 1 << 13 ) == 13 );
static_assert( _blog2( 1 << 14 ) == 14 );
static_assert( _blog2( 1 << 15 ) == 15 );
static_assert( _blog2( 1 << 16 ) == 16 );
static_assert( _blog2( 1 << 17 ) == 17 );
static_assert( _blog2( 1 << 18 ) == 18 );
static_assert( _blog2( 1 << 19 ) == 19 );
static_assert( _blog2( 1 << 20 ) == 20 );
static_assert( _blog2( 1 << 21 ) == 21 );
static_assert( _blog2( 1 << 22 ) == 22 );
static_assert( _blog2( 1 << 23 ) == 23 );
static_assert( _blog2( 1 << 24 ) == 24 );
static_assert( _blog2( 1 << 25 ) == 25 );
static_assert( _blog2( 1 << 26 ) == 26 );
static_assert( _blog2( 1 << 27 ) == 27 );
static_assert( _blog2( 1 << 28 ) == 28 );
static_assert( _blog2( 1 << 29 ) == 29 );
static_assert( _blog2( UINT64_MAX ) == 63 );

static_assert( vtz::math::rem<5>( -5 ) == 0 );
static_assert( vtz::math::rem<5>( -4 ) == 1 );
static_assert( vtz::math::rem<5>( -3 ) == 2 );
static_assert( vtz::math::rem<5>( -2 ) == 3 );
static_assert( vtz::math::rem<5>( -1 ) == 4 );
static_assert( vtz::math::rem<5>( 0 ) == 0 );
static_assert( vtz::math::rem<5>( 1 ) == 1 );
static_assert( vtz::math::rem<5>( 2 ) == 2 );
static_assert( vtz::math::rem<5>( 3 ) == 3 );
static_assert( vtz::math::rem<5>( 4 ) == 4 );
static_assert( vtz::math::rem<5>( 5 ) == 0 );


TEST( vtz, blog2 ) {
    for( int i = 0; i < 64; ++i )
    {
        u64 input = u64( 1 ) << i;
        ASSERT_EQ_QUIET( ref::blog2( input ), i );

        ASSERT_EQ_QUIET( _blog2_fallback( input ), i );
        ASSERT_EQ_QUIET( _blog2( input ), i );
    }

    for( int i = 0; i < 64; ++i )
    {
        u64 input = UINT64_MAX >> i;
        ASSERT_EQ_QUIET( _blog2_fallback( input ), 63 - i );
        ASSERT_EQ_QUIET( _blog2( input ), 63 - i );
    }

    auto rng       = std::mt19937_64();
    auto dist      = std::uniform_int_distribution<u64>( 0 );
    auto shift_dist = std::uniform_int_distribution<int>( 0, 63 );

    // Check random inputs
    for( int i = 0; i < 1000; ++i )
    {
        u64 input = dist( rng ) >> shift_dist( rng );
        if( input == 0 ) continue;
        ADD_CONTEXT( "Test Input: ", input );
        ASSERT_EQ_QUIET( _blog2_fallback( input ), ref::blog2( input ) );
        ASSERT_EQ_QUIET( _blog2( input ), ref::blog2( input ) );
    }
}
