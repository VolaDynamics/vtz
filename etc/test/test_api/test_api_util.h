#pragma once

#include <date/tz.h>
#include <test_vtz/context.h>
#include <test_vtz/utils.h>
#include <test_vtz/zones.h>
#include <test_vtz/zones_test_suite.h>
#include <vtz/tz.h>


struct test_state : vtz::sys_info {
    vtz::local_seconds to_local( vtz::sys_seconds T ) const {
        return _to_local( T, offset );
    }
};

inline std::vector<test_state> extract_states(
    date::time_zone const* tz, vtz::sys_seconds T, vtz::sys_seconds Tmax ) {
    std::vector<test_state> result;

    if( !( T < Tmax ) )
    {
        throw std::runtime_error( "extract_states(): bad input time (T should "
                                  "be smaller than Tmax)" );
    }

    while( T < Tmax )
    {
        auto info  = tz->get_info( T );
        auto state = test_state{
            info.begin,
            info.end,
            info.offset,
            info.save,
            info.abbrev,
        };
        // Update T to point to start of next block (end of this one)
        T = info.end;

        if( result.empty() )
        {
            // just push back the state
            result.push_back( state );
        }
        else
        {
            auto& last = result.back();
            if( last.offset == info.offset // offsets match
                && last.save == info.save  // save match
                && last.abbrev == info.abbrev )
            {
                // this new one is identical; just update the end (this is a bug
                // in date where sometimes it will emit multiple consecutive
                // sys_info blocks with the same offset/save/abbrev)
                last.end = info.end;
            }
            else
            {
                // the new one is distinct, add it to the end
                result.push_back( state );
            }
        }
    }

    auto& first = result.front();
    auto& last  = result.back();
    if( first.begin < _get_time( 1500, 1, 1 ) )
    {
        // set the first value to the earliest possible time
        first.begin = sys_seconds( seconds( INT64_MIN ) );
    }
    if( last.end > _get_time( 4000, 1, 1 ) )
    {
        // set the last value to the latest possible time
        last.end = sys_seconds( seconds( INT64_MAX ) );
    }

    return result;
}
