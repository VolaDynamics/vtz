#pragma once


#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>
#include <type_traits>
#include <vtz/impl/bit.h>
#include <vtz/endian.h>
#include <vtz/span.h>
#include <vtz/strings.h>
#include <vtz/tz_reader/FromUTC.h>
#include <vtz/tz_reader/ZoneState.h>


namespace vtz {
    /// From tzfile(5): https://man7.org/linux/man-pages/man5/tzfile.5.html
    struct tzfile_header {
        /// Size of file header, in bytes
        constexpr static size_t header_size = 44;

        /// Byte identifying the version of the file's format (either ASCII NUL,
        /// "2", "3", or "4")
        char version;

        /// The number of UT/local indicators stored in the file. (UT is
        /// Universal Time.)
        u32 tzh_ttisutcnt;

        /// The number of standard/wall indicators stored in the file.
        u32 tzh_ttisstdcnt;

        /// The number of leap seconds for which data entries are stored in the
        /// file.
        u32 tzh_leapcnt;

        /// The number of transition times for which data entries are stored in
        /// the file.
        u32 tzh_timecnt;

        /// The number of local time types for which data entries are stored in
        /// the file (must not be zero).
        u32 tzh_typecnt;

        /// The number of bytes of time zone abbreviation strings stored in the
        /// file.
        u32 tzh_charcnt;


        /// Offsets past the header for 32-bit tzfile entries (Version 0)
        std::array<size_t, 8> off_32( size_t acc = 0 ) const noexcept {
            return {
                acc,
                // tzh_timecnt 4-byte transition times
                acc += 4 * tzh_timecnt,
                // tzh_timecnt one-byte unsigned integer values
                acc += 1 * tzh_timecnt,
                // tzh_typecnt ttinfo entries (6 bytes each)
                acc += 6 * tzh_typecnt,
                // tzh_charcnt bytes that represent time zone designations
                acc += 1 * tzh_charcnt,
                // tzh_leapcnt pairs of four-byte values
                acc += 8 * tzh_leapcnt,
                // tzh_ttisstdcnt standard/wall indicators, each stored as a
                // one-byte boolean
                acc += 1 * tzh_ttisstdcnt,
                // tzh_ttisutcnt UT/local indicators, each stored as a one-byte
                // boolean
                acc += 1 * tzh_ttisutcnt,
            };
        }

        /// Size of the content block following the header, for 32-bit
        /// transition times/leap second times
        size_t size_contents32() const noexcept { return off_32()[7]; }

        /// Offsets past the header for 32-bit tzfile entries (Version 2 and
        /// above)
        std::array<size_t, 8> off_64( size_t acc = 0 ) const noexcept {
            // From the documentation:
            //
            // > For version-2-format timezone files, the above header and data
            // > are followed by a second header and data, identical in format
            // > except that eight bytes are used for each transition time or
            // > leap second time.  (Leap second counts remain four bytes.)

            return {
                acc,
                // tzh_timecnt 8-byte transition times
                acc += 8 * tzh_timecnt,
                // tzh_timecnt one-byte unsigned integer values
                acc += 1 * tzh_timecnt,
                // tzh_typecnt ttinfo entries (6 bytes each)
                acc += 6 * tzh_typecnt,
                // tzh_charcnt bytes that represent time zone designations
                acc += 1 * tzh_charcnt,
                // tzh_leapcnt pairs of (8 byte leap second time, 4 byte leapcnt
                // time)
                acc += 12 * tzh_leapcnt,
                // tzh_ttisstdcnt standard/wall indicators, each stored as a
                // one-byte boolean
                acc += 1 * tzh_ttisstdcnt,
                // tzh_ttisutcnt UT/local indicators, each stored as a one-byte
                // boolean
                acc += 1 * tzh_ttisutcnt,
            };
        }

        /// Size of the content block following the header, for 32-bit
        /// transition times/leap second times
        size_t size_contents64() const noexcept { return off_64()[7]; }

        int get_version() const noexcept {
            if( version == '\0' ) return 1;
            return version - '0';
        }

        /// Version is '2' or above
        bool version_is_modern() const noexcept { return get_version() >= 2; }
    };


    struct ttinfo_bytes {
        char bb[6];

        i32 tt_utoff() const noexcept { return endian::load_be<i32>( bb ); }
        u8  tt_isdst() const noexcept { return u8( bb[4] ); }
        u8  tt_desigidx() const noexcept { return u8( bb[5] ); }
    };

    template<class T>
    struct leap_bytes {
        char bb[sizeof( T ) + 4];
        // time at which the leap second occurs (or the leap second table
        // expires)
        T when() const noexcept { return endian::load_be<T>( bb ); }

        // Signed integer specifying correction. This is the total number of
        // leap seconds to be applied during the time period starting at the
        // given time.
        i32 count() const noexcept {
            return endian::load_be<i32>( bb + sizeof( T ) );
        }
    };


    using endian::span_be;

    /// Loads tzfile header. Throws an exception if bytes.size() < 44, which is
    /// the minimum size of the header.
    ///
    /// See: https://man7.org/linux/man-pages/man5/tzfile.5.html
    inline tzfile_header load_header( span<char const> bytes ) {
        if( bytes.size() < tzfile_header::header_size )
            throw std::runtime_error(
                "tzfile load_header(): input must be at least 44 bytes" );


        constexpr u32 HEADER_PREFIX = _impl::_load4( "TZif" );
        using endian::load_be;

        char const* p = bytes.data();

        if( _impl::_load4( p ) != HEADER_PREFIX )
        {
            throw std::runtime_error(
                "load_header(): expected tzfile header to begin with four-byte "
                "ASCII sequence 'TZif'" );
        }


        // clang-format off
        return {
                                     // 0..4:   magic bytes 'TZif'
            p[4],                    // 4..5:   version byte (either '\0', '2', '3', or '4')
                                     // 5..20:  reserved for future use
            load_be<u32>( p + 20 ), // 20..24: tzh_ttisutcnt:  The number of UT/local indicators stored in the file.
            load_be<u32>( p + 24 ), // 24..28: tzh_ttisstdcnt: The number of standard/wall indicators stored in the file.
            load_be<u32>( p + 28 ), // 28..32: tzh_leapcnt:    The number of leap seconds for which data entries are stored in the file
            load_be<u32>( p + 32 ), // 32..36: tzh_timecnt:    The number of transition times for which data entries are stored in the file
            load_be<u32>( p + 36 ), // 36..40: tzh_typecnt:    The number of local time types for which data entries are stored in the file (must not be zero)
            load_be<u32>( p + 40 ), // 40..44: tzh_charcnt:    The number of bytes of time zone abbreviation strings stored in the file
        };
        // clang-format on
    }

    enum class tzfile_kind : bool {
        legacy = false,
        modern = true,
    };

    template<tzfile_kind type>
    struct basic_tzfile;

    using tzfile32 = basic_tzfile<tzfile_kind::legacy>;
    using tzfile64 = basic_tzfile<tzfile_kind::modern>;

    template<tzfile_kind type>
    struct basic_tzfile : tzfile_header {
        span<char const> data;

        constexpr static bool is_64_bit    = bool( type );
        constexpr static bool is_32_bit    = !bool( type );
        constexpr static int  time_bitness = is_64_bit ? 64 : 32;

        /// Integral type used to hold transition times
        using time_int = std::conditional_t<is_64_bit, i64, i32>;

        /// Holds pairs of values (time of leap second, leap correction)
        /// 32-bit files use i32 for the leap second, 64-bit files use i64 for
        /// the leap second
        using leap_t = leap_bytes<time_int>;

        basic_tzfile( span<char const> bytes )
        : tzfile_header( load_header( bytes ) )
        , data( bytes.subspan( 44 ) ) {
            if( bytes.size() < size_contents() + 44 )
            {
                throw std::runtime_error(
                    "Cannot load tzfile contents - input too small" );
            }
            if constexpr( is_64_bit )
            {
                if( !version_is_modern() )
                    throw std::runtime_error(
                        "Attempting to load legacy tzdata as modern "
                        "tzdata file" );
            }
        }

        tzfile_header const& header() const noexcept { return *this; }

        /// Offsets into the 'data' portion where each entry can be found
        std::array<size_t, 8> off() const noexcept {
            if constexpr( is_32_bit )
                return off_32();
            else
                return off_64();
        }

        endian::span_be<time_int> transition_times() const noexcept {
            return { data.data() + off()[0], tzh_timecnt };
        }

        endian::span_be<u8> type_indices() const noexcept {
            return { data.data() + off()[1], tzh_timecnt };
        }

        endian::span_bytes<ttinfo_bytes> ttinfo() const noexcept {
            return { data.data() + off()[2], tzh_typecnt };
        }

        /// Buffer containing zone designations. Each one is followed by a NUL
        /// terminator
        std::string_view abbrev_buff() const noexcept {
            return { data.data() + off()[3], tzh_charcnt };
        }

        endian::span_bytes<leap_t> leap() const noexcept {
            return { data.data() + off()[4], tzh_leapcnt };
        }

        endian::span_bytes<u8> isstd() const noexcept {
            return { data.data() + off()[5], tzh_ttisstdcnt };
        }

        endian::span_bytes<u8> isutc() const noexcept {
            return { data.data() + off()[6], tzh_ttisutcnt };
        }

        /// Size of the contents
        size_t size_contents() const noexcept { return off()[7]; }

        /// Return the 'tail' of the file. For legacy tzfiles, this will contain
        /// the modern tzfile. For modern tzfiles, this will contain the posix
        /// TZ string, delimited by newlines.
        span<char const> tail() const noexcept {
            return data.subspan( size_contents() );
        }

        tzfile64 load_modern() const {
            if constexpr( is_32_bit )
            {
                // Load the tail
                return tzfile64( tail() );
            }
            else
            {
                // We're already modern
                return *this;
            }
        }

        OptSV tz_string() const {
            static_assert( is_64_bit,
                "Invoke .load_modern() to obtain a modern tzdata file, before "
                "calling tz_string" );

            auto tail = this->tail();
            if( tail.empty() ) return {};

            auto footer = std::string_view( tail.data(), tail.size() );
            if( footer[0] != '\n' ) { return {}; }

            footer   = footer.substr( 1 );
            auto end = footer.find( '\n' );
            if( end == std::string_view::npos ) { return {}; }

            // Footer found
            return footer.substr( 0, end );
        }

        bool has_transition_times() const noexcept { return tzh_timecnt > 0; }

        string_view get_abbrev_sv( size_t desigidx ) const {
            auto buff = abbrev_buff();

            if( desigidx >= buff.size() )
                throw std::logic_error( "invalid desigidx" );

            char const* p     = buff.data() + desigidx;
            size_t      max_n = buff.size() - desigidx;

            auto result   = string_view( p, max_n );
            auto abbr_end = result.find( '\0' );

            // If there is no null terminator, use the full input
            if( abbr_end == string_view::npos ) return result;

            return string_view( p, abbr_end );
        }

        zone_abbr get_abbrev( size_t desigidx ) const {
            return zone_abbr::from_sv( get_abbrev_sv( desigidx ) );
        }


        /// Get the zone state described by the given type index.
        /// If the corresponding type info indicates that it is currently DST,
        /// then the stdoff is assumed to be one hour behind the walloff

        ZoneState state_from_ti( size_t ti ) const {
            ttinfo_bytes ty = ttinfo()[ti];

            auto utoff  = FromUTC( ty.tt_utoff() );
            auto abbrev = get_abbrev( ty.tt_desigidx() );

            // if we're in daylight savings time, remove the save quantity
            // to get the stdoff this is a HEURISTIC to try and obtain a
            // reasonable stdoff quantity when the zone is always in dst
            auto stdoff
                = ty.tt_isdst() ? utoff.unsave( RuleSave( 3600 ) ) : utoff;
            return {
                stdoff,
                utoff,
                abbrev,
            };
        }


        /// Get the zone state described by the given type index.
        /// If the corresponding type info indicates that it is dst, use the
        /// stdoff given as input. Otherwise, if it is NOT dst, update the input
        /// stdoff

        ZoneState state_from_ti( size_t ti, FromUTC& stdoff ) const {
            ttinfo_bytes ty     = ttinfo()[ti];
            FromUTC      utoff  = FromUTC( ty.tt_utoff() );
            zone_abbr     abbrev = get_abbrev( ty.tt_desigidx() );

            // If we're not within daylight savings time, update the standard
            // offset
            if( !ty.tt_isdst() ) { stdoff = utoff; }
            else
            {
                // if it's now daylight savings time, but the new walloff is the
                // same as the stdoff, we should assume that we have a situation
                // like what occurred on October 3rd, 1999 in Buenos Aires:
                //
                // Argentina entered daylight savings time with save=1:00, and
                // AT THE SAME TIME the standard offset was moved an hour, such
                // that the changes cancelled out.
                //
                // The end result is that the old stdoff is the same as the
                // new wall offset, despite it being in daylight savings time.
                //
                // the tzfile doesn't encode that this is the case, but we
                // should assume that it is
                if( stdoff == utoff ) stdoff = utoff.unsave( RuleSave{ 3600 } );
            }

            return ZoneState{ stdoff, utoff, abbrev };
        }

        ZoneState initial_state() const {
            // The first type entry corresponds to the initial state
            return state_from_ti( 0 );
        }
    };

} // namespace vtz
