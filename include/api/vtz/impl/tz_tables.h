#pragma once

#include <string>
#include <vtz/impl/chrono_types.h>
#include <vtz/impl/macros.h>
#include <vtz/impl/math.h>
#include <vtz/impl/zone_abbr.h>
#include <vtz/types.h>

namespace vtz {
    using std::string;

    constexpr u64 _join32( u32 lo, u32 hi ) {
        return u64( lo ) | ( u64( hi ) << 32 );
    }

    /// Implements fast lookup of 32-bit values for quasi-evenly-spaced inputs
    struct s32_table_view {
        struct entry {
            i64 t;
            u64 block;

            /// Return the low 32 bits as a signed integer. Sign-extend to 64
            /// bits
            i64 lo() const noexcept { return i64( block << 32 ) >> 32; }
            /// Return the high 32 bits as a signed integer. Sign-extend to 64
            /// bits
            i64 hi() const noexcept { return i64( block ) >> 32; }
        };

        int  g;
        i64* tt;
        u64* bb;
        i32  start_;
        u32  size_;

        constexpr i64 const* data_tt() const noexcept { return tt + start_; }
        constexpr u64 const* data_bb() const noexcept { return bb + start_; }

        VTZ_INLINE constexpr void swap( s32_table_view& rhs ) noexcept {
            auto tmp = *this;
            *this    = rhs;
            rhs      = tmp;
        }

        constexpr u64 block_size() const noexcept { return u64( 1 ) << g; }
        constexpr i64 block_start_t( i64 t ) const noexcept {
            return i64( ( u64( t ) >> g ) << g );
        }
        constexpr i64 block_end_t( i64 t ) const noexcept {
            return i64( u64( block_start_t( t ) ) + block_size() );
        }

        /// Return the first value in the table
        constexpr i64 initial() const noexcept {
            u64 block = *data_bb();
            return i64( block << 32 ) >> 32;
        }

        constexpr i32 initial_i32() const noexcept {
            u64 block = *data_bb();
            return i32( i64( block << 32 ) >> 32 );
        }

        constexpr u32 initial_u32() const noexcept {
            u64 block = *data_bb();
            return u32( i64( block << 32 ) >> 32 );
        }

        VTZ_INLINE constexpr entry first_entry() const noexcept {
            return entry{ *data_tt(), *data_bb() };
        }

        VTZ_INLINE constexpr entry get( i64 t ) const noexcept {
            i64 i = t >> g;
            return entry{ tt[i], bb[i] };
        }

        /// if t >= tt[i], return the low 32 bits of bb[i], else obtain the hi
        /// 32
        /// bits of
        /// bb[i], where i = t >> g
        ///
        /// Treats the result as a signed integer, and sign-extends it back to
        /// 64 bits.
        VTZ_INLINE constexpr i64 lookup( i64 t ) const noexcept {
            i64  i         = t >> g;
            bool select_lo = t < tt[i];
            u64  block     = bb[i];
            return i64( block << ( int( select_lo ) << 5 ) ) >> 32;
        }

        /// Same as lookup_i64, but returns i32
        VTZ_INLINE constexpr i32 lookup_i32( i64 t ) const noexcept {
            i64  i         = t >> g;
            bool select_lo = t < tt[i];
            u64  block     = bb[i];
            return i32( i64( block << ( int( select_lo ) << 5 ) ) >> 32 );
        }

        /// Same as lookup_i64, but returns u32
        VTZ_INLINE constexpr u32 lookup_u32( i64 t ) const noexcept {
            i64  i         = t >> g;
            bool select_hi = t >= tt[i];
            u64  block     = bb[i];
            return u32( block >> ( int( select_hi ) << 5 ) );
        }
    };


    // Represents an owning S32Table
    class s32_table : public s32_table_view {
        using Base = s32_table_view;

      public:

        constexpr s32_table() noexcept
        : s32_table_view() {}

        s32_table( int               g,
            std::unique_ptr<i64[]>&& tt,
            std::unique_ptr<u64[]>&& bb,
            i64                      start,
            size_t                   size ) noexcept
        : s32_table_view{
            g,
            tt.release() - start,
            bb.release() - start,
            i32( start ),
            u32( size ),
        } {}

        s32_table( s32_table&& rhs ) noexcept
        : s32_table() {
            swap( rhs );
        }

        using Base::get;
        using Base::initial;
        using Base::lookup;
        using Base::lookup_u32;

        VTZ_INLINE s32_table_view view() const noexcept { return *this; }

        VTZ_INLINE void swap( s32_table& rhs ) noexcept { Base::swap( rhs ); }

        s32_table& operator=( s32_table rhs ) noexcept {
            return swap( rhs ), *this;
        }

        ~s32_table() {
            if( tt ) delete[]( tt + start_ );
            if( bb ) delete[]( bb + start_ );
        }
    };

    enum class choose : bool { earliest = false, latest = true };

    struct zone_states;

    using std::string_view;

    /// Takes a `t` that is out of range, and converts it to a `t` that
    /// is in-range of the lookup table, such that the result of functions
    /// like `offset_from_utc` will be the same.
    ///
    /// A Proleptic Civil Calendar follows a 400 year cycle.
    ///
    /// - Each 400-year cycle contains the same number of days
    /// (146097 days),
    /// - Whatever the given day of the week is (eg, Monday), this
    /// date 400 years from now will also be a Monday.
    /// - If the last sunday of the month is the 26th, the last
    /// sunday of that same month 400 years from now will also
    /// be the 26th, etc
    ///
    /// This means that when daylight savings time starts, ends, etc
    /// will _also_ be the same in 400 years, at least based on all
    /// the rules currently supported by the timezone database source
    /// files.
    constexpr sec_t get_cyclic( sec_t t, sec_t cycle_time ) noexcept {
        // 12622780800 is the number of seconds in 400 years.
        //
        // There are _always_ 97 leap days in _any_ given 400 year period
        // within the civil calendar, so the number of seconds in 400 years
        // is given by (365 * 400 + 97) * 24 * 3600, which is 12622780800
        return ( ( t - cycle_time ) % 12622780800 ) + cycle_time;
    }


    struct off_tables {
        u64       tz0_;
        u64       tz_max_;
        s32_table TTutc;
        sec_t     cycle_time;

        /// For a given system time T, represented as "offsets from UTC", return
        /// the timezone's current offset from UTC, in seconds.
        VTZ_INLINE sec_t offset_s( sec_t t ) const noexcept {
            // If the time is in-bounds, use the lookup table
            if( u64( t ) + tz0_ <= tz_max_ ) VTZ_LIKELY
                return TTutc.lookup( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return TTutc.initial();

            // use zone symmetry to compute state for equivalent time
            return TTutc.lookup( get_cyclic( t, cycle_time ) );
        }

        /// For a given system time T, represented as "offsets from UTC", return
        /// the timezone's current offset from UTC, in seconds.
        VTZ_INLINE sec_t to_local_s( sec_t t ) const noexcept {
            // If the time is in-bounds we can use the lookup table
            if( u64( t ) + tz0_ <= tz_max_ ) VTZ_LIKELY
                return t + TTutc.lookup( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return t + TTutc.initial();

            // use zone symmetry to compute state for equivalent time
            return t + TTutc.lookup( get_cyclic( t, cycle_time ) );
        }

        VTZ_INLINE nanos_t to_local_ns( nanos_t ns ) const noexcept {
            auto parts = math::div_floor2<1000000000>( ns );
            return 1000000000 * to_local_s( parts.quot ) + parts.rem;
        }

        template<bool choose_latest>
        VTZ_INLINE sec_t _lookup_utc( sec_t t_key, sec_t t ) const noexcept {
            auto ent = TTutc.get( t_key );
            /// offset from UTC before transition time
            i64 off_pre = ent.lo();
            /// offset from UTC on or after transition time
            i64  off_post = ent.hi();
            auto when1    = ent.t + off_pre;  // eg, 2AM when DST starts
            auto when2    = ent.t + off_post; // 3am (we saved 1 hour)
            auto when     = choose_latest ? when2 : when1;
            if( off_post <= off_pre )
            {
                // if latest, select when2, otherwise, select when1
                auto off = t_key < when ? off_pre : off_post;
                return t - off;
            }
            else
            {
                // This case occurs if we're entering Daylight Savings Time (or
                // otherwise moving the clocks forward). In this case, there
                // is some chance that we have a nonexistant local time.

                if( t_key >= when2 ) return t - off_post;
                if( t_key <= when1 ) return t - off_pre;
                // Times between 'when1' and 'when2' are nonexistant.
                // All of them refer to the same timestamp
                return ent.t + ( t - t_key );
            }
        }

        int _lookup_local(
            sec_t t_key, sec_t t, sysseconds_t ( &result )[2] ) const noexcept {
            auto ent = TTutc.get( t_key );
            /// offset from UTC before transition time
            i64 off_pre = ent.lo();
            /// offset from UTC on or after transition time
            i64  off_post = ent.hi();
            auto when1    = ent.t + off_pre;  // eg, 2AM when DST starts
            auto when2    = ent.t + off_post; // 3am (we saved 1 hour)
            if( off_post <= off_pre )
            {
                // Let's take the end of daylight savings time in
                // America/New_York as an example.
                //
                // At 1:59:59am, rather than changing to 2am, the clock falls
                // back an hour to 1:00 am, and the offset goes from UTC-04 to
                // UTC-05

                // `off_post <= off_pre`
                // -> `ent.t + off_post <= ent.t + off_pre`
                // -> `when2 <= when1`
                //
                // This case occurs if we're dealing with ambiguous times.

                if( when1 <= t_key ) // eg, the key is 2AM or after
                {
                    // We're past the time we could be ambiguous
                    result[0] = result[1] = t - off_post;
                    return local_info::unique;
                }
                if( t_key < when2 )
                {
                    // eg, the key is before 1am
                    result[0] = result[1] = t - off_pre;
                    return local_info::unique;
                }
                // The result is ambiguous
                result[0] = t - off_pre;  // earliest possible time
                result[1] = t - off_post; // latest possible time
                return local_info::ambiguous;
            }
            else
            {
                // This case occurs if we're entering Daylight Savings Time (or
                // otherwise moving the clocks forward). In this case, there
                // is some chance that we have a nonexistant local time.

                if( t_key >= when2 )
                { // eg, local time is 3am or after
                    result[0] = result[1] = t - off_post;
                    return local_info::unique;
                }
                if( t_key < when1 )
                { // eg, local time is before 2am
                    result[0] = result[1] = t - off_pre;
                    return local_info::unique;
                }

                // Time was nonexistant.
                sysseconds_t jump_time
                    = ent.t + ( t - t_key ); // Time at which jump occurred (eg,
                                             // 3am (since 2am is nonexistent))

                result[0] = jump_time - 1;   // Time right before jump
                result[1] = jump_time;       // Time at jump
                return local_info::nonexistent;
            }
        }

        int lookup_local(
            sec_t t, sysseconds_t ( &result )[2] ) const noexcept {
            // If the time is in-bounds, we can use the lookup table
            if( u64( t ) + tz0_ <= tz_max_ ) VTZ_LIKELY
                return _lookup_local( t, t, result );

            // t is _early_: this means it occurs before any timezone
            // transitions.
            if( t < 0 )
            {
                sysseconds_t utc_time = t - TTutc.initial();
                result[0] = result[1] = utc_time;
                return local_info::unique;
            }

            // use zone symmetry to compute state for equivalent time
            return _lookup_local( get_cyclic( t, cycle_time ), t, result );
        }


        template<bool choose_latest>
        sec_t _to_utc( sec_t t ) const noexcept {
            // If the time is in-bounds, we can use the lookup table
            if( u64( t ) + tz0_ <= tz_max_ ) VTZ_LIKELY
                return _lookup_utc<choose_latest>( t, t );

            // t is _early_: use initial zone state
            if( t < 0 ) return t - TTutc.initial();

            // use zone symmetry to compute state for equivalent time
            return _lookup_utc<choose_latest>( get_cyclic( t, cycle_time ), t );
        }

        /// Returns the UTC time represented by a given input local time.
        ///
        /// If the local time is ambiguous (referring to potentially two system
        /// times), selects between them based on whether `which` is `latest` or
        /// `earliest`.
        ///
        /// SPECIAL CASE - NON-EXISTANT LOCAL TIMES
        ///
        /// When we move the clock forward, there is some chance that we
        /// encounter a non-existant local time.
        ///
        /// Let's take America/New_York as an example.
        ///
        /// The time 2025 Mar 09 02:30 AM (local time) is nonexistant -
        /// on that date, the clocks move forward from 1:59:59 am to 3am,
        /// so local times in the range 2am..3am are nonexistant, because the
        /// clock skips forward to 3am.
        ///
        /// What do we return for a nonexistant local time?
        ///
        /// - 01:59:59am EST is 06:59:59 UTC
        /// - 02:00:00am EST _would be_ 7am UTC...
        ///
        /// BUT the clocks skip forward, so the next existant
        /// time is 3am EDT, which is 7am UTC
        ///
        /// There's no gap between those two times.
        ///
        /// So, all nonexistant times refer to the same UTC timestamp.
        ///
        /// This matches the definition of `to_sys` given in P0355R7,
        /// which was incorporated into the C++ standard as the standard
        /// timezone library:
        ///
        /// > If the tp represents a non-existent time between two UTC
        /// > time_points, then the two UTC time_points will be the same,
        /// > and that UTC time_point will be returned.
        VTZ_INLINE sec_t to_sys_s( sec_t t, choose which ) const noexcept {
            if( which == choose::earliest )
            {
                // Return earliest UTC time this could refer to
                return _to_utc<false>( t );
            }
            else
            {
                // Return latest UTC time this could refer to
                return _to_utc<true>( t );
            }
        }

        VTZ_INLINE nanos_t to_sys_ns( nanos_t t, choose which ) const noexcept {
            auto parts = math::div_floor2<1000000000>( t );
            if( which == choose::earliest )
            {
                // Return earliest UTC time this could refer to
                return 1000000000 * _to_utc<false>( parts.quot ) + parts.rem;
            }
            else
            {
                // Return latest UTC time this could refer to
                return 1000000000 * _to_utc<true>( parts.quot ) + parts.rem;
            }
        }

        struct _impl;
    };

    struct trans_table {
        u64       tz0_;
        u64       tz_max_;
        s32_table tt_index;
        sec_t     cycle_time;

        std::unique_ptr<sysseconds_t[]> when;

        struct time_range {
            sysseconds_t begin;
            sysseconds_t end;
        };

        VTZ_INLINE time_range sys_range_s( sysseconds_t t ) const noexcept {
            if( u64( t ) + tz0_ <= tz_max_ ) VTZ_LIKELY
                return sys_range_impl( t );

            if( t < 0 ) { return time_range{ when[0], when[1] }; }

            auto t2    = get_cyclic( t, cycle_time );
            auto r     = sys_range_impl( t2 );
            auto delta = t - t2;
            return {
                r.begin + delta,
                r.end + delta,
            };
        }

      private:

        VTZ_INLINE time_range sys_range_impl( sysseconds_t t ) const noexcept {
            auto i = tt_index.lookup( t );
            return { when[i], when[i + 1] };
        }
    };

    struct stdoff_table {
        sysseconds_t t_min;
        sysseconds_t t_max;
        s32_table    stdoff;

        VTZ_INLINE i32 stdoff_s( sysseconds_t t ) const noexcept {
            if( t < t_min ) t = t_min;
            if( t > t_max ) t = t_max;
            return stdoff.lookup_i32( t );
        }
    };

    struct abbr_table {
        u64 tz0_;
        u64 tz_max_;

        s32_table abbr;
        sec_t     cycle_time;

        std::unique_ptr<zone_abbr[]> abbrev_list;

        u32 abbr_block_s( sec_t t ) const noexcept {
            if( u64( t ) + tz0_ <= tz_max_ ) VTZ_LIKELY
                return abbr.lookup_i32( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return abbr.initial_i32();

            // use zone symmetry to compute state for equivalent time
            return abbr.lookup_u32( get_cyclic( t, cycle_time ) );
        }

        /// Writes up to 7 characters, representing the abbreviation
        size_t abbrev_to_s( sec_t t, char* p ) const noexcept {
            auto        block = abbr_block_s( t );
            size_t      size  = block & 0xf;
            char const* data  = abbrev_list[block >> 4].buff_;
            _vtz_memcpy( p, data, size );
            return size;
        }

        /// Return the abbreviation (eg, 'EST' or 'EDT') for a given
        /// timestamp
        string_view abbrev_s( sec_t t ) const noexcept {
            if( u64( t ) + tz0_ <= tz_max_ ) VTZ_LIKELY
                return abbr_from_block( abbr.lookup_i32( t ) );

            // t is _early_: use initial zone state
            if( t < 0 ) return abbr_from_block( abbr.initial_u32() );

            // use zone symmetry to compute state for equivalent time
            return abbr_from_block(
                abbr.lookup_u32( get_cyclic( t, cycle_time ) ) );
        }

        /// Return the abbreviation (eg, 'EST' or 'EDT') for a given
        /// timestamp
        string abbrev_string_s( sec_t t ) const noexcept {
            if( u64( t ) + tz0_ <= tz_max_ ) VTZ_LIKELY
                return abbr_string_from_block( abbr.lookup_u32( t ) );

            // t is _early_: use initial zone state
            if( t < 0 ) return abbr_string_from_block( abbr.initial_u32() );

            // use zone symmetry to compute state for equivalent time
            return abbr_string_from_block(
                abbr.lookup_u32( get_cyclic( t, cycle_time ) ) );
        }

        string_view abbr_from_block( u32 block ) const noexcept {
            size_t      size = block & 0xf;
            char const* data = abbrev_list[block >> 4].buff_;
            return string_view( data, size );
        }

        std::string abbr_string_from_block( u32 block ) const noexcept {
            size_t      size = block & 0xf;
            char const* data = abbrev_list[block >> 4].buff_;
            return std::string( data, size );
        }
    };
} // namespace vtz
