#pragma once

#include "vtz/civil.h"
#include <chrono>
#include <climits>
#include <cstdint>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vtz/span.h>
#include <vtz/strings.h>
#include <vtz/tz_reader/ZoneFormat.h>

#include <vtz/bit.h>

namespace vtz::detail {
    /// Compute the amount of precision necessary to represent some duration
    constexpr int getNecessaryPrecision(
        bool isFloatingPoint, intmax_t num, intmax_t denom ) noexcept {
        // if it's floating-point, use max precision
        if( isFloatingPoint ) return 9;

        // if the numerator is divisible by the denominator, we can use 0 for
        // the precision
        if( num % denom == 0 ) return 0;

        if( 10 % denom == 0 ) return 1;
        if( 100 % denom == 0 ) return 2;
        if( 1000 % denom == 0 ) return 3;
        if( 10000 % denom == 0 ) return 4;
        if( 100000 % denom == 0 ) return 5;
        if( 1000000 % denom == 0 ) return 6;
        if( 10000000 % denom == 0 ) return 7;
        if( 100000000 % denom == 0 ) return 8;

        return 9;
    }

    template<class Duration>
    constexpr int getNecessaryPrecision() {
        using rep    = typename Duration::rep;
        using period = typename Duration::period;
        // period::num is the numerator, period::den is the denominator
        return getNecessaryPrecision(
            std::is_floating_point<rep>(), period::num, period::den );
    }
} // namespace vtz::detail


namespace vtz {
    using sec_t   = i64;
    using nanos_t = i64;

    using std::chrono::hours;
    using std::chrono::minutes;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;

    using days = std::chrono::duration<i32, std::ratio<86400>>;
    using std::chrono::system_clock;
    using std::chrono::time_point;

    template<class Duration>
    using sys_time    = time_point<system_clock, Duration>;
    using sys_seconds = sys_time<seconds>;
    using sys_days    = sys_time<days>;

    struct local_t {};
    template<class Duration>
    using local_time = std::chrono::time_point<local_t, Duration>;

    using local_seconds = local_time<seconds>;
    using local_days    = local_time<days>;


    struct sys_info {
        sys_seconds begin;
        sys_seconds end;
        seconds     offset;
        minutes     save;
        std::string abbrev;

        bool operator==( sys_info const& rhs ) const noexcept {
            return begin == rhs.begin      //
                   && end == rhs.end       //
                   && offset == rhs.offset //
                   && save == rhs.save     //
                   && abbrev == rhs.abbrev;
        }
    };

    /// This class describes the result of converting a `local_time` to a
    /// `sys_time`.
    ///
    /// - If the result of the conversion is unique, then `result ==
    ///   local_info::unique`, `first` is filled out with the correct
    ///   `sys_info`, and second is zero-initialized.
    /// - If the `local_time` is nonexistent, then `result ==
    ///   local_info::nonexistent`, `first` is filled out with the `sys_info`
    ///   that ends just prior to the `local_time`, and second is filled out
    ///   with the `sys_info` that begins just after the `local_time`.
    /// - If the `local_time` is ambiguous, then `result ==
    ///   local_info::ambiguous`, `first` is filled out with the `sys_info` that
    ///   ends just after the `local_time`, and second is filled with the
    ///   `sys_info` that starts just before the `local_time`.
    ///
    /// This is a low-level data structure; typical conversions from local_time
    /// to sys_time will use it implicitly rather than explicitly.

    struct local_info {
        constexpr static int unique      = 0;
        constexpr static int nonexistant = 1;
        constexpr static int ambiguous   = 2;

        int      result;
        sys_info first;
        sys_info second;

        bool operator==( local_info const& rhs ) const noexcept {
            return result == rhs.result  //
                   && first == rhs.first //
                   && second == rhs.second;
        }
    };

    // clang-format off

    /// Get the number of seconds since the epoch
    VTZ_INLINE constexpr sec_t ns_to_s( nanos_t ns ) noexcept { return vtz::math::divFloor<1000000000>( ns ); }
    VTZ_INLINE constexpr nanos_t s_to_ns( sec_t s ) noexcept { return s * 1000000000; }

    // clang-format on

    constexpr u64 _join32( u32 lo, u32 hi ) {
        return u64( lo ) | ( u64( hi ) << 32 );
    }

    /// Implements fast lookup of 32-bit values for quasi-evenly-spaced inputs
    struct S32TableView {
        struct Entry {
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

        VTZ_INLINE constexpr void swap( S32TableView& rhs ) noexcept {
            auto tmp = *this;
            *this    = rhs;
            rhs      = tmp;
        }

        constexpr u64 blockSize() const noexcept { return u64( 1 ) << g; }
        constexpr i64 blockStartT( i64 t ) const noexcept {
            return i64( ( u64( t ) >> g ) << g );
        }
        constexpr i64 blockEndT( i64 t ) const noexcept {
            return i64( u64( blockStartT( t ) ) + blockSize() );
        }

        /// Return the first value in the table
        constexpr i64 initial() const noexcept {
            u64 block = *data_bb();
            return i64( block << 32 ) >> 32;
        }

        VTZ_INLINE constexpr Entry firstEntry() const noexcept {
            return Entry{ *data_tt(), *data_bb() };
        }

        VTZ_INLINE constexpr Entry get( i64 t ) const noexcept {
            i64 i = t >> g;
            return Entry{ tt[i], bb[i] };
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
    class S32Table : public S32TableView {
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

        using Base::get;
        using Base::initial;
        using Base::lookup;
        using Base::lookupU32;

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

    enum class choose : bool { earliest = false, latest = true };

    struct ZoneStates;

    using std::string_view;

    /// Takes a `t` that is out of range, and converts it to a `t` that
    /// is in-range of the lookup table, such that the result of functions
    /// like `offsetFromUTC` will be the same.
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
    constexpr sec_t getCyclic( sec_t t, sec_t cycleTime ) noexcept {
        // 12622780800 is the number of seconds in 400 years.
        //
        // There are _always_ 97 leap days in _any_ given 400 year period
        // within the civil calendar, so the number of seconds in 400 years
        // is given by (365 * 400 + 97) * 24 * 3600, which is 12622780800
        return ( ( t - cycleTime ) % 12622780800 ) + cycleTime;
    }


    struct OffTables {
        u64      tz0_;
        u64      tzMax_;
        S32Table TTutc;
        sec_t    cycleTime;

        /// For a given system time T, represented as "offsets from UTC", return
        /// the timezone's current offset from UTC, in seconds.
        VTZ_INLINE sec_t offset_s( sec_t t ) const noexcept {
            // If the time is in-bounds, use the lookup table
            if( u64( t ) + tz0_ <= tzMax_ ) [[likely]]
                return TTutc.lookup( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return TTutc.initial();

            // use zone symmetry to compute state for equivalent time
            return TTutc.lookup( getCyclic( t, cycleTime ) );
        }

        /// For a given system time T, represented as "offsets from UTC", return
        /// the timezone's current offset from UTC, in seconds.
        VTZ_INLINE sec_t to_local_s( sec_t t ) const noexcept {
            // If the time is in-bounds we can use the lookup table
            if( u64( t ) + tz0_ <= tzMax_ ) [[likely]]
                return t + TTutc.lookup( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return t + TTutc.initial();

            // use zone symmetry to compute state for equivalent time
            return t + TTutc.lookup( getCyclic( t, cycleTime ) );
        }

        VTZ_INLINE nanos_t to_local_ns( nanos_t ns ) const noexcept {
            auto parts = math::divFloor2<1000000000>( ns );
            return 1000000000 * to_local_s( parts.quot ) + parts.rem;
        }

        template<bool chooseLatest>
        VTZ_INLINE sec_t _lookupUTC( sec_t tKey, sec_t t ) const noexcept {
            auto ent = TTutc.get( tKey );
            /// offset from UTC before transition time
            i64 offPre = ent.lo();
            /// offset from UTC on or after transition time
            i64  offPost = ent.hi();
            auto when1   = ent.t + offPre;  // eg, 2AM when DST starts
            auto when2   = ent.t + offPost; // 3am (we saved 1 hour)
            auto when    = chooseLatest ? when2 : when1;
            if( offPost <= offPre )
            {
                // if latest, select when2, otherwise, select when1
                auto off = tKey < when ? offPre : offPost;
                return t - off;
            }
            else
            {
                // This case occurs if we're entering Daylight Savings Time (or
                // otherwise moving the clocks forward). In this case, there
                // is some chance that we have a nonexistant local time.

                if( tKey >= when2 ) return t - offPost;
                if( tKey <= when1 ) return t - offPre;
                // Times between 'when1' and 'when2' are nonexistant.
                // All of them refer to the same timestamp
                return ent.t + ( t - tKey );
            }
        }

        int _lookupLocal(
            sec_t tKey, sec_t t, sysseconds_t ( &result )[2] ) const noexcept {
            auto ent = TTutc.get( tKey );
            /// offset from UTC before transition time
            i64 offPre = ent.lo();
            /// offset from UTC on or after transition time
            i64  offPost = ent.hi();
            auto when1   = ent.t + offPre;  // eg, 2AM when DST starts
            auto when2   = ent.t + offPost; // 3am (we saved 1 hour)
            if( offPost <= offPre )
            {
                // Let's take the end of daylight savings time in
                // America/New_York as an example.
                //
                // At 1:59:59am, rather than changing to 2am, the clock falls
                // back an hour to 1:00 am, and the offset goes from UTC-04 to
                // UTC-05

                // `offPost <= offPre`
                // -> `ent.t + offPost <= ent.t + offPre`
                // -> `when2 <= when1`
                //
                // This case occurs if we're dealing with ambiguous times.

                if( when1 <= tKey ) // eg, the key is 2AM or after
                {
                    // We're past the time we could be ambiguous
                    result[0] = result[1] = t - offPost;
                    return local_info::unique;
                }
                if( tKey < when2 )
                {
                    // eg, the key is before 1am
                    result[0] = result[1] = t - offPre;
                    return local_info::unique;
                }
                // The result is ambiguous
                result[0] = t - offPre;  // earliest possible time
                result[1] = t - offPost; // latest possible time
                return local_info::ambiguous;
            }
            else
            {
                // This case occurs if we're entering Daylight Savings Time (or
                // otherwise moving the clocks forward). In this case, there
                // is some chance that we have a nonexistant local time.

                if( tKey >= when2 )
                { // eg, local time is 3am or after
                    result[0] = result[1] = t - offPost;
                    return local_info::unique;
                }
                if( tKey < when1 )
                { // eg, local time is before 2am
                    result[0] = result[1] = t - offPre;
                    return local_info::unique;
                }

                // Time was nonexistant.
                sysseconds_t jumpTime
                    = ent.t + ( t - tKey ); // Time at which jump occurred (eg,
                                            // 3am (since 2am is nonexistent))

                result[0] = jumpTime - 1;   // Time right before jump
                result[1] = jumpTime;       // Time at jump
                return local_info::nonexistant;
            }
        }

        int lookupLocal( sec_t t, sysseconds_t ( &result )[2] ) const noexcept {
            // If the time is in-bounds, we can use the lookup table
            if( u64( t ) + tz0_ <= tzMax_ ) [[likely]]
                return _lookupLocal( t, t, result );

            // t is _early_: this means it occurs before any timezone
            // transitions.
            if( t < 0 )
            {
                sysseconds_t utcTime = t - TTutc.initial();
                result[0] = result[1] = utcTime;
                return local_info::unique;
            }

            // use zone symmetry to compute state for equivalent time
            return _lookupLocal( getCyclic( t, cycleTime ), t, result );
        }


        template<bool chooseLatest>
        sec_t _toUTC( sec_t t ) const noexcept {
            // If the time is in-bounds, we can use the lookup table
            if( u64( t ) + tz0_ <= tzMax_ ) [[likely]]
                return _lookupUTC<chooseLatest>( t, t );

            // t is _early_: use initial zone state
            if( t < 0 ) return t - TTutc.initial();

            // use zone symmetry to compute state for equivalent time
            return _lookupUTC<chooseLatest>( getCyclic( t, cycleTime ), t );
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
                return _toUTC<false>( t );
            }
            else
            {
                // Return latest UTC time this could refer to
                return _toUTC<true>( t );
            }
        }

        VTZ_INLINE nanos_t to_sys_ns( nanos_t t, choose which ) const noexcept {
            auto parts = math::divFloor2<1000000000>( t );
            if( which == choose::earliest )
            {
                // Return earliest UTC time this could refer to
                return 1000000000 * _toUTC<false>( parts.quot ) + parts.rem;
            }
            else
            {
                // Return latest UTC time this could refer to
                return 1000000000 * _toUTC<true>( parts.quot ) + parts.rem;
            }
        }

        struct Impl;
    };

    struct TransTable {
        u64      tz0_;
        u64      tzMax_;
        S32Table ttIndex;
        sec_t    cycleTime;

        std::unique_ptr<sysseconds_t[]> when;

        struct Range {
            sysseconds_t begin;
            sysseconds_t end;
        };

        VTZ_INLINE Range sys_range_s( sysseconds_t t ) const noexcept {
            if( u64( t ) + tz0_ <= tzMax_ ) [[likely]]
                return sys_range_impl( t );

            if( t < 0 ) { return Range{ when[0], when[1] }; }

            auto t2    = getCyclic( t, cycleTime );
            auto r     = sys_range_impl( t2 );
            auto delta = t - t2;
            return {
                r.begin + delta,
                r.end + delta,
            };
        }

      private:

        VTZ_INLINE Range sys_range_impl( sysseconds_t t ) const noexcept {
            auto i = ttIndex.lookup( t );
            return { when[i], when[i + 1] };
        }
    };

    struct StdoffTable {
        sysseconds_t tMin;
        sysseconds_t tMax;
        S32Table     stdoff;

        VTZ_INLINE i32 stdoff_s( sysseconds_t t ) const noexcept {
            if( t < tMin ) t = tMin;
            if( t > tMax ) t = tMax;
            return stdoff.lookup( t );
        }
    };

    struct AbbrTable {
        u64 tz0_;
        u64 tzMax_;

        S32Table abbr;
        sec_t    cycleTime;

        std::unique_ptr<ZoneAbbr[]> abbrTable;

        u32 abbr_block_s( sec_t t ) const noexcept {
            if( u64( t ) + tz0_ <= tzMax_ ) [[likely]]
                return abbr.lookup( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return abbr.initial();

            // use zone symmetry to compute state for equivalent time
            return abbr.lookup( getCyclic( t, cycleTime ) );
        }

        /// Writes up to 7 characters, representing the abbreviation
        size_t abbrev_to_s( sec_t t, char* p ) const noexcept {
            auto        block = abbr_block_s( t );
            size_t      size  = block & 0xf;
            char const* data  = abbrTable[block >> 4].buff_;
            _vtz_memcpy( p, data, size );
            return size;
        }

        /// Return the abbreviation (eg, 'EST' or 'EDT') for a given
        /// timestamp
        string_view abbrev_s( sec_t t ) const noexcept {
            if( u64( t ) + tz0_ <= tzMax_ ) [[likely]]
                return abbrFromBlock( abbr.lookup( t ) );

            // t is _early_: use initial zone state
            if( t < 0 ) return abbrFromBlock( abbr.initial() );

            // use zone symmetry to compute state for equivalent time
            return abbrFromBlock( abbr.lookup( getCyclic( t, cycleTime ) ) );
        }

        /// Return the abbreviation (eg, 'EST' or 'EDT') for a given
        /// timestamp
        string abbrev_string_s( sec_t t ) const noexcept {
            if( u64( t ) + tz0_ <= tzMax_ ) [[likely]]
                return abbrStringFromBlock( abbr.lookup( t ) );

            // t is _early_: use initial zone state
            if( t < 0 ) return abbrStringFromBlock( abbr.initial() );

            // use zone symmetry to compute state for equivalent time
            return abbrStringFromBlock(
                abbr.lookup( getCyclic( t, cycleTime ) ) );
        }

        string_view abbrFromBlock( u32 block ) const noexcept {
            size_t      size = block & 0xf;
            char const* data = abbrTable[block >> 4].buff_;
            return string_view( data, size );
        }

        std::string abbrStringFromBlock( u32 block ) const noexcept {
            size_t      size = block & 0xf;
            char const* data = abbrTable[block >> 4].buff_;
            return std::string( data, size );
        }
    };


    class TimeZone
    : private OffTables
    , private AbbrTable
    , private StdoffTable
    , private TransTable {
      public:

        /// Constructs a TimeZone representing UTC (stdoff of 0, walloff of 0,
        /// abbreviation is UTC, etc)
        static TimeZone utc();

        TimeZone( string_view name, ZoneStates const& states );

        using AbbrTable::abbrev_s;
        using AbbrTable::abbrev_string_s;
        using AbbrTable::abbrev_to_s;
        using OffTables::offset_s;
        using OffTables::to_local_ns;
        using OffTables::to_local_s;
        using OffTables::to_sys_ns;
        using OffTables::to_sys_s;
        using StdoffTable::stdoff_s;

        /// Return the name of the zone
        string_view name() const noexcept { return name_; }

        /// Return the current save, in seconds
        i32 save_s( sysseconds_t t ) const noexcept {
            return offset_s( t ) - stdoff_s( t );
        }

        /// Convert the given time (sys_seconds) to local seconds
        local_seconds to_local( sys_seconds s ) const {
            return _local( to_local_s( s.time_since_epoch().count() ) );
        }

        /// Convert the given time (local seconds) to UTC
        sys_seconds to_sys( local_seconds s, choose z ) const {
            return _sys( to_sys_s( s.time_since_epoch().count(), z ) );
        }

        /// Convert the given time (sys_seconds) to local seconds
        template<class Dur>
        auto to_local( sys_time<Dur> t ) const
            -> local_time<std::common_type_t<Dur, seconds>> {
            // This is the time we're actually looking up, in seconds
            sys_seconds t2 = std::chrono::floor<seconds>( t );

            // This is the local time (with the unit being seconds)
            local_seconds localT
                = _local( to_local_s( t2.time_since_epoch().count() ) );

            // if t2 is less precise than t, we chopped off some unit of time
            // (eg, some number of nanoseconds)
            //
            // We need to add it back.
            auto delta = t - t2;

            return localT + delta;
        }

        /// Convert the given time (local seconds) to UTC
        template<class Dur>
        auto to_sys( local_time<Dur> t, choose z ) const
            -> sys_time<std::common_type_t<Dur, seconds>> {
            // Time we're looking up (must be seconds)
            local_seconds t2 = std::chrono::floor<seconds>( t );

            // This is the corresponding system time (with the unit being in
            // seconds)
            sys_seconds sysT
                = _sys( to_sys_s( t.time_since_epoch().count(), z ) );

            // if t2 is less precise than t, we chopped off some unit of time
            // (eg, some number of nanoseconds)
            //
            // We need to add it back.
            auto delta = t - t2;

            return sysT + delta;
        }

        sys_info get_info_sys_s( sysseconds_t t ) const {
            auto range = sys_range_s( t );

            auto stdoff = stdoff_s( t );
            auto off    = offset_s( t );
            auto save   = off - stdoff;

            return sys_info{
                sys_seconds( seconds( range.begin ) ),
                sys_seconds( seconds( range.end ) ),
                seconds( off ),
                std::chrono::floor<minutes>( seconds( save ) ),
                abbrev_string_s( t ),
            };
        }

        local_info get_info_local_s( sec_t t ) const {
            sysseconds_t tt[2];
            int          result = lookupLocal( t, tt );
            if( result == local_info::unique )
            {
                return local_info{
                    result,
                    get_info_sys_s( tt[0] ),
                    sys_info{},
                };
            }
            return local_info{
                result,
                get_info_sys_s( tt[0] ),
                get_info_sys_s( tt[1] ),
            };
        }

        template<class Dur>
        local_info get_info( local_time<Dur> input ) const {
            return get_info_local_s( _rawTime( input ) );
        }

        template<class Dur>
        sys_info get_info( sys_time<Dur> input ) const {
            return get_info_sys_s( _rawTime( input ) );
        }

        /// Returns a number [0,86400) representing the current time of day
        /// (local time)
        u32 local_time_of_day_s( sysseconds_t s ) const noexcept {
            return u32( math::rem<86400>( to_local_s( s ) ) );
        }

        /// Returns the date of the given systime, as number of days since the
        /// epoch
        i64 local_date_s( sysseconds_t t ) const noexcept {
            return math::divFloor<86400>( to_local_s( t ) );
        }

        /// Given nanoseconds since the epoch, return the current date (as days
        /// since the epoch)
        i64 local_date_ns( nanos_t nanos ) const noexcept {
            return math::divFloor<86400000000000>( to_local_ns( nanos ) );
        }

        template<class Dur>
        auto local_date( sys_time<Dur> t ) const -> local_days {
            return local_days(
                days( local_date_s( std::chrono::floor<seconds>( t )
                        .time_since_epoch()
                        .count() ) ) );
        }

        // clang-format off

        /// Formats a time to the given buffer. For format specifiers, see: https://en.cppreference.com/w/cpp/chrono/c/strftime.html
        size_t format_to_s( string_view format, sysseconds_t t, char* buff, size_t count ) const;
        /// Formats a time to the given buffer. For format specifiers, see: https://en.cppreference.com/w/cpp/chrono/c/strftime.html
        size_t format_to(   string_view format, sys_seconds  t, char* buff, size_t count ) const { return format_to_s( format, t.time_since_epoch().count(), buff, count ); }
        /// Formats a time with std::strftime specifiers. See: https://en.cppreference.com/w/cpp/chrono/c/strftime.html
        string format_s(    string_view format, sysseconds_t t ) const;
        /// Formats a time with std::strftime specifiers. See: https://en.cppreference.com/w/cpp/chrono/c/strftime.html
        string format(      string_view format, sys_seconds  t ) const { return format_s( format, t.time_since_epoch().count() ); }


        /// Formats as `%Y-%m-%d %H:%M:%S %Z`. Example: `2025-11-06 17:50:00 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        size_t format_to_s( sysseconds_t t, char* buff, size_t count, char dateSep = '-', char dateTimeSep = ' ', char abbrevSep = ' ' ) const noexcept;
        /// Formats as `%Y-%m-%d %H:%M:%S %Z`. Example: `2025-11-06 17:50:00 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        size_t format_to(   sys_seconds  t, char* buff, size_t count, char dateSep = '-', char dateTimeSep = ' ', char abbrevSep = ' ' ) const { return format_to_s( t.time_since_epoch().count(), buff, count, dateSep, dateTimeSep, abbrevSep ); }
        /// Formats as `%Y-%m-%d HH:MM:SS %Z`. Example: `2025-11-06 17:50:00 EST`
        string format_s(    sysseconds_t t, char dateSep = '-', char dateTimeSep = ' ', char abbrevSep = ' ' ) const;
        /// Formats as `%Y-%m-%d %H:%M:%S %Z`. Example: `2025-11-06 17:50:00 EST`
        string format(      sys_seconds  t, char dateSep = '-', char dateTimeSep = ' ', char abbrevSep = ' ' ) const { return format_s( t.time_since_epoch().count(), dateSep, dateTimeSep, abbrevSep ); }


        /// Formats as `%Y%m%d %H%M%S %Z`. Example: `20251106 175000 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        size_t format_compact_to_s( sysseconds_t t, char* p, size_t count, char dateTimeSep = ' ', char abbrevSep = ' ' ) const noexcept;
        /// Formats as `%Y%m%d %H%M%S %Z`. Example: `20251106 175000 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        size_t format_compact_to(   sys_seconds  t, char* p, size_t count, char dateTimeSep = ' ', char abbrevSep = ' ' ) const { return format_compact_to_s( t.time_since_epoch().count(), p, count, dateTimeSep, abbrevSep ); }
        /// Formats as `%Y%m%d %H%M%S %Z`. Example: `20251106 175000 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        string format_compact_s(    sysseconds_t t, char dateTimeSep = ' ', char abbrevSep = ' ' ) const;
        /// Formats as `%Y%m%d %H%M%S %Z`. Example: `20251106 175000 EST`. Writes output to buffer. Returns bytes written. Truncates output if needed.
        string format_compact(      sys_seconds  t, char dateTimeSep = ' ', char abbrevSep = ' ' ) const { return format_compact_s( t.time_since_epoch().count(), dateTimeSep, abbrevSep ); }


        /// Formats as `%Y-%m-%d[dateTimeSep]%H:%M:%S%z`, with the option to use an alternative date separator. Example: 2025-11-06T17:50:00-05
        size_t format_iso8601_to_s( sysseconds_t t, char* buff, size_t count, char dateSep = '-', char dateTimeSep = 'T' ) const noexcept;
        /// Formats as `%Y-%m-%d[dateTimeSep]%H:%M:%S%z`, with the option to use an alternative date separator. Example: 2025-11-06T17:50:00-05
        size_t format_iso8601_to(   sys_seconds  t, char* buff, size_t count, char dateSep = '-', char dateTimeSep = 'T' ) const { return format_iso8601_to_s( t.time_since_epoch().count(), buff, count, dateSep, dateTimeSep );}
        /// Formats as `%Y-%m-%d[dateTimeSep]%H:%M:%S%z`, with the option to use an alternative date separator. Example: 2025-11-06T17:50:00-05
        string format_iso8601_s(    sysseconds_t t, char dateSep = '-', char dateTimeSep = 'T' ) const;
        /// Formats as `%Y-%m-%d[dateTimeSep]%H:%M:%S%z`, with the option to use an alternative date separator. Example: 2025-11-06T17:50:00-05
        string format_iso8601(      sys_seconds  t, char dateSep = '-', char dateTimeSep = 'T' ) const { return format_iso8601_s( t.time_since_epoch().count(), dateSep, dateTimeSep );}


        /// Formats a time (expressed as seconds and nanoseconds) to the given buffer.
        /// For format specifiers, see: https://en.cppreference.com/w/cpp/chrono/c/strftime.html
        ///
        /// Preconditions: nanos is in the range `[0, 999999999]`, and precision<=9.
        /// The fractional component will formatted as a decimal followed by the given digits,
        /// and it will be placed immediately after the 'seconds' component.
        ///
        /// When formatting, if precision is less than 9, the value is truncated. Eg, if nanos
        /// is 999999999, but precision is 3, the fractional component is `.999`.
        ///
        /// If precision is 0, no fractional component is written, and the decimal point is omitted.
        ///
        /// Examples:
        ///
        /// - `"%Y%m%d %H:%M:%S-%Z"` with `t=1762442685`, `nanos=029380000` and `prec=4` would produce "20251106 08:24:45.0293-MST" for America/Denver
        /// - `"%F %T %Z"` with `t=1762442685`, `nanos=029380000` and `precision=4` would produce "2025-11-06 08:24:45.0293-MST" for America/Denver
        ///
        /// In the latter example, `%T` expands to `%H:%M:%S`.
        size_t format_precise_to_s( string_view format, sysseconds_t t, u32 nanos, int precision, char* buff, size_t count ) const;

        /// Format the given time, with the requested precision (up to 9 digits)
        template<class Dur>
        size_t format_precise_to( string_view format, sys_time<Dur> t, int precision, char* buff, size_t count ) const {
            auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
            auto nanos = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
            return format_precise_to_s( format, sec.count(), nanos.count(), precision, buff, count );
        }

        /// Formats a time (expressed as seconds and nanoseconds) to the given buffer. For more information,
        /// see the equivalent version of `format_to_s`.
        string format_precise_s( string_view format, sysseconds_t t, u32 nanos, int precision ) const;

        /// Format the given time, with the requested precision (up to 9 digits)
        template<class Dur>
        string format_precise( string_view format, sys_time<Dur> t, int precision ) const {
            auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
            auto nanos = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
            return format_precise_s( format, sec.count(), nanos.count(), precision );
        }

        /// Format the given time. If the input time has a precision higher than
        /// seconds, or it's fractional (or floating-point), then up to 9 digits
        /// will be included after the seconds component, as necessary to
        /// represent the input time.
        template<class Dur>
        size_t format_to( string_view format, sys_time<Dur> t, char* buff, size_t count ) const {
            constexpr int prec = detail::getNecessaryPrecision<Dur>();

            // If the required amount of precision is 0, format w/ seconds.
            // Otherwise, compute the necessary amount of precision, and format
            // with that.
            if constexpr( prec == 0 )
            {
                return format_to_s( format, seconds( t.time_since_epoch() ).count(), buff, count );
            }
            else
            {
                auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
                auto nanos = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
                return format_precise_to_s( format, sec.count(), nanos.count(), prec, buff, count );
            }
        }

        /// Format the given time. If the input time has a precision higher than
        /// seconds, or it's fractional (or floating-point), then up to 9 digits
        /// will be included after the seconds component, as necessary to
        /// represent the input time.
        template<class Dur>
        string format( string_view format, sys_time<Dur> t ) const {
            constexpr int prec = detail::getNecessaryPrecision<Dur>();

            if constexpr( prec == 0 )
            {
                return format_s( format, seconds( t.time_since_epoch() ).count() );
            }
            else
            {
                auto sec = std::chrono::floor<seconds>( t.time_since_epoch() );
                auto nanos = std::chrono::floor<nanoseconds>( t.time_since_epoch() - sec );
                return format_precise_s( format, sec.count(), nanos.count(), prec );
            }
        }

        // clang-format on


        struct Impl;

      private:

        static sec_t _rawTime( local_seconds s ) {
            return s.time_since_epoch().count();
        }
        static sec_t _rawTime( sys_seconds s ) {
            return s.time_since_epoch().count();
        }
        template<class Dur>
        static sec_t _rawTime( local_time<Dur> s ) {
            return std::chrono::floor<seconds>( s ).time_since_epoch().count();
        }
        template<class Dur>
        static sec_t _rawTime( sys_time<Dur> s ) {
            return std::chrono::floor<seconds>( s ).time_since_epoch().count();
        }

        static local_seconds _local( sec_t s ) {
            return local_seconds( seconds( s ) );
        }
        static sys_seconds _sys( sec_t s ) {
            return sys_seconds( seconds( s ) );
        }

        string name_;
    };

    using time_zone = TimeZone;

    std::string tzdb_version();

    /// Set the path to use for the timezone database. Will throw an exception
    /// if the timezone database is already loaded.
    void set_install( std::string path );

    std::string get_install();

    time_zone const* locate_zone( string_view name );

    time_zone const* current_zone();
} // namespace vtz
