#pragma once


#include <vtz/tz.h>

namespace vtz {
    struct TimeZone::Impl {
        static auto local_block_s( OffTables const& table, sec_t t ) {
            // If the time is in-bounds, we can use the lookup table
            if( u64( t ) + table.tz0_ <= table.tzMax_ )
                return table.TTutc.get( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return table.TTutc.firstEntry();

            // use zone symmetry to compute state for equivalent time
            return table.TTutc.get( getCyclic( t, table.cycleTime ) );
        }
        static auto local_block_s( TimeZone const& table, sec_t t ) {
            return local_block_s( static_cast<OffTables const&>( table ), t );
        }

        static AbbrTable const& getAbbrTable( TimeZone const& tz ) noexcept {
            return tz;
        }
        static decltype( auto ) getTT( TimeZone const& tz ) noexcept {
            return ( tz.TTutc );
        }
    };
} // namespace vtz
