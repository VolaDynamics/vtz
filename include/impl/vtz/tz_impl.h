#pragma once


#include <vtz/tz.h>

namespace vtz {
    struct time_zone::_impl {
        static auto local_block_s( off_tables const& table, sec_t t ) {
            // If the time is in-bounds, we can use the lookup table
            if( u64( t ) + table.tz0_ <= table.tz_max_ )
                return table.tt_utc.get( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return table.tt_utc.first_entry();

            // use zone symmetry to compute state for equivalent time
            return table.tt_utc.get( get_cyclic( t, table.cycle_time ) );
        }
        static auto local_block_s( time_zone const& table, sec_t t ) {
            return local_block_s( static_cast<off_tables const&>( table ), t );
        }

        static abbr_table const& get_abbr_table(
            time_zone const& tz ) noexcept {
            return tz;
        }

        static decltype( auto ) get_tt( time_zone const& tz ) noexcept {
            return ( tz.tt_utc );
        }

        template<class TZ>
        static auto stdoff_s( TZ const& tz, sec_t t ) noexcept {
            return tz.stdoff_s( t );
        }

        template<class TZ>
        static auto save_s( TZ const& tz, sec_t t ) noexcept {
            return tz.save_s( t );
        }
    };
} // namespace vtz
