#include "test_helper.h"

#include <gtest/gtest.h>


#include <vtz/parse.h>
#include <vtz/tz.h>

TEST( vtz_api, locate_zone_sanity ) {
    // Sanity check for ensuring we can locate the given zone
    //
    // There are MUCH more thorough tests throughout etc/test/

    auto tz = vtz::locate_zone( "America/New_York" );

    using std::chrono::seconds;

    // Check date before DST
    auto T = vtz::parse<seconds>( "%F %T", "2025-02-25 18:12:00" );
    ASSERT_EQ( tz->format( T ), "2025-02-25 13:12:00 EST" );

    // Check date after DST
    auto T2 = vtz::parse<seconds>( "%F %T", "2025-06-25 18:12:00" );
    ASSERT_EQ( tz->format( T2 ), "2025-06-25 14:12:00 EDT" );
}
