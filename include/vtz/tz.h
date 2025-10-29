#pragma once

#include "vtz/span.h"
#include <climits>
#include <cstdint>
#include <fmt/format.h>
#include <memory>
#include <string_view>
#include <utility>
#include <vtz/strings.h>

#include <vtz/bit.h>

namespace vtz {
    constexpr u64 _join32( u32 lo, u32 hi ) {
        return u64( lo ) | ( u64( hi ) << 32 );
    }

    /// Implements fast lookup of 32-bit values for quasi-evenly-spaced inputs
    struct S32TableView {
        int  g;
        i64* tt;
        u64* bb;
        i32  start_;
        u32  size_;

        i64 const* data_tt() const noexcept { return tt + start_; }
        u64 const* data_bb() const noexcept { return bb + start_; }

        VTZ_INLINE constexpr void swap( S32TableView& rhs ) noexcept {
            auto tmp = *this;
            *this    = rhs;
            rhs      = tmp;
        }

        /// Return the first value in the table
        constexpr i64 initial() const noexcept {
            u64 block = *data_bb();
            return i64( block << 32 ) >> 32;
        }

        /// if t >= tt[i], return the low 32 bits of bb[i], else obtain the hi
        /// 32
        /// bits of
        /// bb[i], where i = t >> g
        ///
        /// Treats the result as a signed integer, and sign-extends it back to
        /// 64 bits.
        VTZ_INLINE constexpr i64 lookup( i64 t ) const noexcept {
            i64  i        = t >> g;
            bool selectLo = t < tt[i];
            u64  block    = bb[i];
            return i64( block << ( int( selectLo ) << 5 ) ) >> 32;
        }

        VTZ_INLINE constexpr u32 lookupU32( i64 t ) const noexcept {
            i64  i        = t >> g;
            bool selectHi = t >= tt[i];
            u64  block    = bb[i];
            return u32( block >> ( int( selectHi ) << 5 ) );
        }
    };


    // Represents an owning S32Table
    class S32Table : private S32TableView {
        using Base = S32TableView;

      public:

        constexpr S32Table() noexcept
        : S32TableView() {}

        S32Table( int                g,
            std::unique_ptr<i64[]>&& tt,
            std::unique_ptr<u64[]>&& bb,
            i64                      start,
            size_t                   size ) noexcept
        : S32TableView{
            g,
            tt.release() - start,
            bb.release() - start,
            i32( start ),
            u32( size ),
        } {}

        S32Table( S32Table&& rhs ) noexcept
        : S32Table() {
            swap( rhs );
        }

        using Base::lookup;
        using Base::lookupU32;
        using Base::initial;

        VTZ_INLINE S32TableView view() const noexcept { return *this; }

        VTZ_INLINE void swap( S32Table& rhs ) noexcept { Base::swap( rhs ); }

        S32Table& operator=( S32Table rhs ) noexcept {
            return swap( rhs ), *this;
        }

        ~S32Table() {
            if( tt ) delete[]( tt + start_ );
            if( bb ) delete[]( bb + start_ );
        }
    };

    /// Seconds from epoch
    using sec_t = i64;

    enum class choose { earliest, latest };

    struct ZoneStates;

    using std::string_view;
    class TimeZone {
      public:

        using Abbr = FixStr<15>;

        TimeZone( string name, ZoneStates const& states );

        string_view name() const noexcept { return name_; }

        /// For a given system time T, represented as "offsets from UTC", return
        /// the timezone's current offset from UTC, in seconds.
        VTZ_INLINE sec_t offsetFromUTC( sec_t t ) const noexcept {
            u64 i = u64( t ) + tz0_;
            if( i <= tzMax_ )
            {
                // We can use the lookup table, which is very fast
                return utcOff.lookup( t );
            }

            // t is _early_: use initial zone state
            if( t < 0 ) return utcOff.initial();

            // use zone symmetry to compute state for equivalent time
            return utcOff.lookup( getCyclic( t ) );
        }

        /// Return the abbreviation (eg, 'EST' or 'EDT') for a given
        /// timestamp
        string_view abbrFromUTC( sec_t t ) const noexcept {
            u64 i = u64( t ) + tz0_;
            if( i <= tzMax_ )
            {
                // We can use the lookup table, which is very fast
                return abbrFromBlock( abbr.lookup( t ) );
            }

            // t is _early_: use initial zone state
            if( t < 0 ) return abbrFromBlock( abbr.initial() );

            // use zone symmetry to compute state for equivalent time
            return abbrFromBlock( abbr.lookup( getCyclic( t ) ) );
        }


        /// For a given system time T, represented as "offsets from UTC", return
        /// the timezone's current offset from UTC, in seconds.
        VTZ_INLINE sec_t toLocal( sec_t t ) const noexcept {
            u64 i = u64( t ) + tz0_;
            if( i <= tzMax_ )
            {
                // We can use the lookup table, which is very fast
                return t + utcOff.lookup( t );
            }

            // t is _early_: use initial zone state
            if( t < 0 ) return t + utcOff.initial();

            // use zone symmetry to compute state for equivalent time
            return t + utcOff.lookup( getCyclic( t ) );
        }


        /// Returns the UTC time represented by a given input local time.
        ///
        /// If the local time is ambiguous (referring to potentially two system
        /// times), returns the later of the two.
        ///
        /// For nonexistant times, returns:
        ///
        ///     (time since last existant time) + (systime of last existant
        ///     time)
        ///
        /// Which is the correct interpretation if, eg, someone had forgotten to
        /// set their clock forward.
        VTZ_INLINE sec_t toUTCLatest( sec_t t ) const noexcept {
            u64 i = u64( t ) + tz0_;
            if( i <= tzMax_ )
            {
                // We can use the lookup table, which is very fast
                return t + localOffLatest.lookup( t );
            }

            // t is _early_: use initial zone state
            if( t < 0 ) return t + localOffLatest.initial();

            // use zone symmetry to compute state for equivalent time
            return t + localOffLatest.lookup( getCyclic( t ) );
        }


      private:

        /// Takes a `t` that is out of range, and converts it to a `t` that
        /// is in-range of the lookup table, such that the result of functions
        /// like `offsetFromUTC` will be the same.
        ///
        /// A Proleptic Civil Calendar follows a 400 year cycle.
        ///
        /// - Each 400-year cycle contains the same number of days
        ///   (146097 days),
        /// - Whatever the given day of the week is (eg, Monday), this
        ///   date 400 years from now will also be a Monday.
        /// - If the last sunday of the month is the 26th, the last
        ///   sunday of that same month 400 years from now will also
        ///   be the 26th, etc
        ///
        /// This means that when daylight savings time starts, ends, etc
        /// will _also_ be the same in 400 years, at least based on all
        /// the rules currently supported by the timezone database source
        /// files.
        sec_t getCyclic( sec_t t ) const noexcept {
            // 12622780800 is the number of seconds in 400 years.
            //
            // There are _always_ 97 leap days in _any_ given 400 year period
            // within the civil calendar, so the number of seconds in 400 years
            // is given by (365 * 400 + 97) * 24 * 3600, which is 12622780800
            return ( ( t - cycleTime ) % 12622780800 ) + cycleTime;
        }

        string_view abbrFromBlock( u32 block ) const noexcept {
            size_t      size = block & 0xf;
            char const* data = abbrTable[block >> 4].buff_;
            return string_view( data, size );
        }


        u64 tz0_;
        u64 tzMax_;

        S32Table utcOff;
        S32Table abbr;

        S32Table localOffLatest;
        S32Table localOffEarliest;


        std::unique_ptr<Abbr[]> abbrTable;

        i64 cycleTime;

        string name_;
    };
} // namespace vtz
