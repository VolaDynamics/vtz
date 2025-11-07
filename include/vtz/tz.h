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

namespace vtz {
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

    /// Seconds from epoch
    using sec_t = i64;

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
            if( u64( t ) + tz0_ <= tzMax_ ) [[likely]] return TTutc.lookup( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return TTutc.initial();

            // use zone symmetry to compute state for equivalent time
            return TTutc.lookup( getCyclic( t, cycleTime ) );
        }

        /// For a given system time T, represented as "offsets from UTC", return
        /// the timezone's current offset from UTC, in seconds.
        VTZ_INLINE sec_t to_local_s( sec_t t ) const noexcept {
            // If the time is in-bounds we can use the lookup table
            if( u64( t ) + tz0_ <= tzMax_ ) [[likely]] return t + TTutc.lookup( t );

            // t is _early_: use initial zone state
            if( t < 0 ) return t + TTutc.initial();

            // use zone symmetry to compute state for equivalent time
            return t + TTutc.lookup( getCyclic( t, cycleTime ) );
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

        struct Impl;
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

        string_view abbrFromBlock( u32 block ) const noexcept {
            size_t      size = block & 0xf;
            char const* data = abbrTable[block >> 4].buff_;
            return string_view( data, size );
        }
    };

    using std::chrono::hours;
    using std::chrono::minutes;
    using std::chrono::seconds;
    using days = std::chrono::duration<i32, std::ratio<86400>>;
    using std::chrono::system_clock;
    using std::chrono::time_point;

    template<class Duration>
    using sys_time    = time_point<system_clock, Duration>;
    using sys_seconds = sys_time<seconds>;

    struct local_t {};
    template<class Duration>
    struct local_time : std::chrono::time_point<local_t, Duration> {
        using std::chrono::time_point<local_t, Duration>::time_point;
    };

    using local_seconds = local_time<seconds>;

    class TimeZone
    : private OffTables
    , private AbbrTable
    , private StdoffTable {
      public:

        TimeZone( string_view name, ZoneStates const& states );

        using AbbrTable::abbrev_s;
        using AbbrTable::abbrev_to_s;
        using OffTables::offset_s;
        using OffTables::to_local_s;
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
        auto to_local( sys_time<Dur> t ) const noexcept
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

        // clang-format on

        struct Impl;

      private:

        static local_seconds _local( sec_t s ) {
            return local_seconds( seconds( s ) );
        }
        static sys_seconds _sys( sec_t s ) {
            return sys_seconds( seconds( s ) );
        }

        string name_;
    };

    /// Formats a time in the form %Y%m%d %H%M%S %Z
    ///
    /// This will be 16 characters, plus the length of the timezone abbreviation
    inline string formatSysTimeAsLocalCompact( TimeZone const* tz,
        sysseconds_t                                           t,
        char dateTimeSep = ' ',
        char tzAbbrSep   = ' ' ) {
        char buffer[64];
        auto local = tz->to_local_s( t );
        auto abbr  = tz->abbrev_s( t );
        _write_timestamp_compact( local, buffer, dateTimeSep );
        buffer[15] = tzAbbrSep;
        _vtz_memcpy( buffer + 16, abbr.data(), abbr.size() );
        return string( buffer, 16 + abbr.size() );
    }

    /// Formats a time in the form %Y-%m-%d %H:%M:%S-%Z
    ///
    /// This will be 20 characters, plus the length of the timezone abbreviation
    inline string formatSysTimeAsLocal( TimeZone const* tz,
        sysseconds_t                                    t,
        char                                            dateSep     = '-',
        char                                            dateTimeSep = ' ',
        char                                            tzAbbrSep   = '-' ) {
        char buffer[64];
        auto local = tz->to_local_s( t );
        auto abbr  = tz->abbrev_s( t );
        _write_timestamp( local, buffer, dateSep, dateTimeSep );
        buffer[19] = tzAbbrSep;
        _vtz_memcpy( buffer + 20, abbr.data(), abbr.size() );
        return string( buffer, 20 + abbr.size() );
    }

    using time_zone = TimeZone;


    time_zone const* locate_zone( string_view name );
} // namespace vtz
