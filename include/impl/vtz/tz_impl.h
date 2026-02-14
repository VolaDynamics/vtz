#pragma once


#include <vtz/tz.h>

namespace vtz {
    struct time_zone::Impl {
        static auto local_block_s( OffTables const& table, sec_t t ) {
            // If the time is in-bounds, we can use the lookup table
            if( u64( t ) + table.tz0_ <= table.tz_max_ )
                return table.TTutc.get( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return table.TTutc.first_entry();

            // use zone symmetry to compute state for equivalent time
            return table.TTutc.get( get_cyclic( t, table.cycle_time ) );
        }
        static auto local_block_s( time_zone const& table, sec_t t ) {
            return local_block_s( static_cast<OffTables const&>( table ), t );
        }

        static AbbrTable const& get_abbr_table( time_zone const& tz ) noexcept {
            return tz;
        }
        static decltype( auto ) get_tt( time_zone const& tz ) noexcept {
            return ( tz.TTutc );
        }
    };
} // namespace vtz
