
#include <vtz/civil.h>
#include <vtz/impl/bit.h>
#include <vtz/tz.h>
#include <vtz/format.h>

#include <charconv>

namespace vtz {

    class FormatError : public std::exception {
        char const* what_;

      public:

        FormatError( char const* what ) noexcept
        : what_( what ) {}

        char const* what() const noexcept override { return what_; }
    };

    FormatError buffer_too_small() noexcept {
        return FormatError( "format error: destination buffer is too small" );
    }

    // Write a year, assuming the year is positive and fits in 4 digits, and
    // the bugger is big enough
    VTZ_INLINE static char* _write_year4( char* p, i64 year ) noexcept {
        u16  y      = u16( year );
        auto y1000  = y / 1000;
        y          %= 1000;
        auto y100   = y / 100;
        y          %= 100;
        auto y10    = y / 10;
        y          %= 10;
        auto y1     = y;
        p[0]        = char( '0' + y1000 );
        p[1]        = char( '0' + y100 );
        p[2]        = char( '0' + y10 );
        p[3]        = char( '0' + y1 );
        return p + 4;
    }
    /// Writes the year. Assumes there are at least 22 characters of space in
    /// the buffer
    VTZ_INLINE static char* _write_year( char* p, i64 year ) noexcept {
        // if the year is nonnegative and fits in 4 digits, write as 4-digit
        // year
        if( u64( year ) < 10000 ) VTZ_LIKELY
            return _write_year4( p, year );
        // fallback to std::to_chars
        return std::to_chars( p, p + 22, year ).ptr;
    }

    /// Writes the last 2 digits of the year, as a decimal number
    VTZ_INLINE static char* _write_year_short( char* p, i64 year ) noexcept {
        u8 x = u8( std::abs( year ) % 100 );
        p[0] = '0' + x / 10;
        p[1] = '0' + x % 10;
        return p + 2;
    }

    /// %m specifier - write the month as a decimal in the range [01, 12]
    VTZ_INLINE static char* _write_mon( char* p, u8 mon ) noexcept {
        p[0] = '0' + ( mon >= 10 );
        p[1] = '0' + ( mon % 10 );
        return p + 2;
    }

    /// writes day of the month as a decimal number (range [01,31])
    VTZ_INLINE static char* _write_dom_d( char* p, u8 day ) noexcept {
        char c0 = '0' + day / 10;
        char c1 = '0' + day % 10;
        p[0]    = c0;
        p[1]    = c1;
        return p + 2;
    }

    /// writes day of the month as a decimal number (range [1,31]).
    /// Single digit is preceded by a space.
    VTZ_INLINE static char* _write_dom_e( char* p, u8 day ) noexcept {
        char c0 = '0' + day / 10;
        char c1 = '0' + day % 10;
        if( day < 10 ) c0 = ' ';
        p[0] = c0;
        p[1] = c1;
        return p + 2;
    }

    /// equivalent to "%H:%M"
    VTZ_INLINE static char* _write_iso_hhmm( char* p, int h, int m ) noexcept {
        p[0] = char( '0' + h / 10 );
        p[1] = char( '0' + h % 10 );
        p[2] = ':';
        p[3] = char( '0' + m / 10 );
        p[4] = char( '0' + m % 10 );
        return p + 5;
    }

    /// equivalent to "%H%M%S" (the ISO 8601 time format)
    VTZ_INLINE static char* _write_hhmmsscompact(
        char* p, int h, int m, int s ) noexcept {
        p[0] = char( '0' + h / 10 );
        p[1] = char( '0' + h % 10 );
        p[2] = char( '0' + m / 10 );
        p[3] = char( '0' + m % 10 );
        p[4] = char( '0' + s / 10 );
        p[5] = char( '0' + s % 10 );
        return p + 6;
    }

    /// equivalent to "%H:%M:%S" (the ISO 8601 time format)
    VTZ_INLINE static char* _write_hhmmss(
        char* p, int h, int m, int s ) noexcept {
        p[0] = char( '0' + h / 10 );
        p[1] = char( '0' + h % 10 );
        p[2] = ':';
        p[3] = char( '0' + m / 10 );
        p[4] = char( '0' + m % 10 );
        p[5] = ':';
        p[6] = char( '0' + s / 10 );
        p[7] = char( '0' + s % 10 );
        return p + 8;
    }
    /// Write a 2-digit int. Assumes value is < 100
    VTZ_INLINE static char* _write2Digit( char* p, u8 x ) noexcept {
        p[0] = char( '0' + x / 10 );
        p[1] = char( '0' + x % 10 );
        return p + 2;
    }
    /// Write a 3-digit int. Assumes value is < 1000
    VTZ_INLINE static char* _write3Digit( char* p, u16 x ) noexcept {
        p[0]  = char( '0' + x / 100 );
        x    %= 100;
        p[1]  = char( '0' + x / 10 );
        p[2]  = char( '0' + x % 10 );
        return p + 3;
    }

    /// equivalent to "%Y-%m-%d" (the ISO 8601 date format)
    /// Assumes there is at least 28 characters of space in the buffer
    /// (21 characters needed for excessively large negative/positive year)
    VTZ_INLINE static char* _write_iso_date(
        char* p, i64 y, int m, int d ) noexcept {
        if( u64( y ) < 10000 ) VTZ_LIKELY
        {
            p    = _write_year4( p, y );
            *p++ = '-';
            p    = _write_mon( p, u8( m ) );
            *p++ = '-';
            p    = _write_dom_d( p, u8( d ) );
            return p;
        }
        p    = _write_year( p, y );
        *p++ = '-';
        p    = _write_mon( p, u8( m ) );
        *p++ = '-';
        p    = _write_dom_d( p, u8( d ) );
        return p;
    }

    namespace {
        struct WriteNOOP {
            VTZ_INLINE static size_t dump( char const*, size_t size ) {
                return size;
            }
        };
        struct WriteToString {
            VTZ_INLINE static std::string dump( char const* s, size_t size ) {
                return std::string( s, size );
            }
        };
        struct WriteToBuff {
            char*  buff;
            size_t count;

            VTZ_INLINE size_t dump( char const* s, size_t size ) noexcept {
                // clamp size
                if( size > count ) size = count;
                // write to buffer
                _vtz_memcpy( buff, s, size );
                // return size
                return size;
            }
        };
    } // namespace

    template<class TimeZoneT, class F, class WriteFractional>
    auto _do_strformat( TimeZoneT const& tz,
        char const*                      format,
        size_t                           format_size,
        sysseconds_t                     t,
        WriteFractional                  write_frac,
        F                                func ) {
        constexpr size_t MAX_SPECIFIERS = 64;
        constexpr size_t MAX_SIZE       = MAX_SPECIFIERS * 2;
        // In the worst case, the year is 13 bytes. (this occurs if, eg, the
        // input is INT64_MIN) in which case the year would be around
        // -292471208678 - this gives the length:
        //
        //     len(str(-2**63 // (365 * 86400)))
        //
        // The longest expansion is %F, which expands to %Y-%m-%d, so (13+6), so
        // 19 characters in length
        constexpr size_t REQUIRED_BUFF_SIZE = 19 * MAX_SPECIFIERS + 1;
        // Make buffer size a power of 2
        constexpr size_t BUFF_SIZE
            = 1ull << ( 1 + _blog2_fallback( REQUIRED_BUFF_SIZE ) );

        static_assert( BUFF_SIZE >= REQUIRED_BUFF_SIZE );

        if( format_size >= MAX_SIZE )
            throw FormatError(
                "format_time(): input format string is too long" );

        if( format_size == 0 ) return func.dump( "", 0 );

        auto gmtoff         = i32( tz.offset_s( t ) );
        auto t_local        = t + gmtoff;
        auto parts          = vtz::math::div_floor2<86400>( t_local );
        auto date           = sysdays_t( parts.quot );
        auto time_intraday  = u32( parts.rem );
        int  hr             = time_intraday / 3600;
        time_intraday      %= 3600;
        int mi              = time_intraday / 60;
        time_intraday      %= 60;
        int  sec            = time_intraday;
        auto ymd            = to_civil( date );

        // We're going to handle as many format specifiers as we can ourselves,
        // so that we can avoid a call to strftime.
        //
        // How big do we need to make this buffer?
        // Well, we know there are at most (format_size/2) specifiers.
        // This means a max of 64 specifiers.
        char fmt_str[BUFF_SIZE];

        char*  p = fmt_str;
        size_t i = 0;
        // Iterate until 1 before the end. This ensures we don't do an
        // out-of-bounds read on a format specifier
        size_t len            = format_size - 1;
        bool   needs_strftime = false;
        while( i < len )
        {
            char c = format[i++];
            if( c != '%' )
            {
                // Not a format specifier? Copy the character verbatim.
                *p++ = c;
                continue;
            }
            // We've encountered a format specifier, so we consume an
            // additional character
            char next = format[i++];
            switch( next )
            {
            // writes literal %. The full conversion specification must be %%.
            case '%': *p++ = '%'; continue;
            // writes newline character
            case 'n': *p++ = '\n'; continue;
            // writes horizontal tab character
            case 't': *p++ = '\t'; continue;
            // writes year as a decimal number, e.g. 2017
            case 'Y': p = _write_year( p, ymd.year ); continue;
            // writes last 2 digits of year as a decimal number (range
            // [00,99])
            case 'y': p = _write_year_short( p, ymd.year ); continue;
            // writes month as a decimal number (range [01,12])
            case 'm': p = _write_mon( p, u8( ymd.month ) ); continue;
            // 	writes day of the month as a decimal number (range
            // [01,31])
            case 'd': p = _write_dom_d( p, u8( ymd.day ) ); continue;
            // writes day of the year as a decimal number (range [001,366])
            case 'j':
                {
                    auto yday = u16( date - resolve_civil( ymd.year, 1, 1 ) );

                    p = _write3Digit( p, 1 + yday );
                    continue;
                }
            // writes weekday as a decimal number, where Sunday is 0 (range
            // [0-6])
            case 'w':
                {
                    static_assert( int( DOW::Sun ) == 0 );
                    auto dow = int( dow_from_days( date ) );
                    *p++     = char( '0' + dow );
                    continue;
                }
            // 	writes weekday as a decimal number, where Monday is 1 (ISO 8601
            // format) (range [1-7])
            case 'u':
                {
                    auto dow = int( dow_from_days( date ) );
                    if( dow == 0 ) dow = 7;
                    *p++ = char( '0' + dow );
                    continue;
                }
            // writes hour as a decimal number, 12 hour clock (range [01,12])
            case 'I':
                {
                    int x = hr % 12;
                    if( x == 0 ) x = 12;
                    p = _write2Digit( p, u8( x ) );
                    continue;
                }
            // writes day of the month as a decimal number (range [1,31]).
            // Single digit is preceded by a space.
            case 'e': p = _write_dom_e( p, u8( ymd.day ) ); continue;
            // equivalent to "%Y-%m-%d" (the ISO 8601 date format)
            case 'F':
                p = _write_iso_date( p, ymd.year, ymd.month, ymd.day );
                continue;
            // equivalent to "%H:%M"
            case 'R': p = _write_iso_hhmm( p, hr, mi ); continue;
            // 	writes hour as a decimal number, 24 hour clock (range
            // [00-23])
            case 'H': p = _write2Digit( p, u8( hr ) ); continue;
            // writes minute as a decimal number (range [00,59])
            case 'M': p = _write2Digit( p, u8( mi ) ); continue;
            // writes second as a decimal number (range [00,60]). Will also
            // write a fractional component after the second.
            case 'S': p = write_frac( _write2Digit( p, u8( sec ) ) ); continue;
            // equivalent to "%H:%M:%S" (the ISO 8601 time format)
            case 'T':
                p = write_frac( _write_hhmmss( p, hr, mi, sec ) );
                continue;
            // writes offset from UTC in the ISO 8601 format (e.g. -0430)
            case 'z': p += write_shortest_offset( gmtoff, p ); continue;
            case 'Z': p += tz.abbrev_to_s( t, p ); continue;
            default:
                {
                    *p++           = c;
                    *p++           = next;
                    needs_strftime = true;
                    continue;
                }
            }
        }
        // If the format string did not end in a format specifier, make sure
        // to copy the final character.
        if( i == len ) { *p++ = format[i]; }

        if( !needs_strftime ) VTZ_LIKELY
        {
            // If we were able to handle all the specifiers, there's nothing
            // left to format. This means we don't need to do a call to
            // strftime, and we can exit early!
            return func.dump( fmt_str, size_t( p - fmt_str ) );
        }

        // Add null terminator to format string
        *p = '\0';

        auto stdoff = tz.stdoff_s( t );
        bool is_dst = gmtoff != stdoff;

        auto tm_value = std::tm{
            sec,
            mi,
            hr,
            ymd.day,
            ymd.month - 1, // the 'tm' struct uses 0-11 for the month
            // tm_year stores years since 1900
            ymd.year - 1900,
            // days since sunday - 0-6
            int( dow_from_days( date ) - DOW::Sun ),
            // days since January 1 – [​0​, 365]
            int( date - resolve_civil( ymd.year, 1, 1 ) ),
            // true if daylight savings time is in effect
            is_dst,
        };

        // Fill in any remaining format specifiers with strftime.
        // We will use this if there are any locale-dependent format specifiers
        // (asize from %Z)

        char   buffer[BUFF_SIZE];
        size_t count
            = std::strftime( buffer, sizeof( buffer ), fmt_str, &tm_value );
        return func.dump( buffer, count );
    }

    size_t time_zone::format_to_s(
        string_view format, sysseconds_t t, char* buff, size_t count ) const {
        return _do_strformat(
            *this,
            format.data(),
            format.size(),
            t,
            // There is no fractional component, so write_frac is a noop
            []( char* s ) noexcept -> char* { return s; },
            WriteToBuff{ buff, count } );
    }


    struct DummyTimeZone {
        /// NOOP - offset is 0
        static sec_t offset_s( sysseconds_t ) noexcept { return 0; }
        static sec_t stdoff_s( sysseconds_t ) noexcept { return 0; }
        /// NOOP - no abbreviation to write
        static size_t abbrev_to_s( sec_t, char* ) noexcept { return 0; }
    };

    std::string format_date_d( string_view fmt, sysdays_t days ) {
        return _do_strformat(
            DummyTimeZone{},
            fmt.data(),
            fmt.size(),
            days_to_seconds( days ),
            // There is no fractional component, so write_frac is a noop
            []( char* s ) noexcept -> char* { return s; },
            WriteToString{} );
    }

    struct DummyTimeZoneUTC {
        /// UTC offset is 0
        static sec_t offset_s( sysseconds_t ) noexcept { return 0; }
        static sec_t stdoff_s( sysseconds_t ) noexcept { return 0; }
        /// Writes "UTC"
        static size_t abbrev_to_s( sec_t, char* p ) noexcept {
            p[0] = 'U';
            p[1] = 'T';
            p[2] = 'C';
            return 3;
        }
    };

    std::string format_time_s( string_view fmt, sysseconds_t t ) {
        return _do_strformat(
            DummyTimeZoneUTC{},
            fmt.data(),
            fmt.size(),
            t,
            []( char* s ) noexcept -> char* { return s; },
            WriteToString{} );
    }

    size_t format_time_to_s(
        string_view fmt, sysseconds_t t, char* buff, size_t count ) {
        return _do_strformat(
            DummyTimeZoneUTC{},
            fmt.data(),
            fmt.size(),
            t,
            []( char* s ) noexcept -> char* { return s; },
            WriteToBuff{ buff, count } );
    }

    size_t format_date_to_d(
        string_view fmt, sysdays_t days, char* buff, size_t count ) {
        return _do_strformat(
            DummyTimeZone{},
            fmt.data(),
            fmt.size(),
            days_to_seconds( days ),
            // There is no fractional component, so write_frac is a noop
            []( char* s ) noexcept -> char* { return s; },
            WriteToBuff{ buff, count } );
    }

    auto _write_nanos( u32 nanos, int precision ) noexcept {
        return [nanos, precision]( char* p ) noexcept -> char* {
            // clang-format off
            u32 n = nanos;
            int prec = precision;
            if( prec == 0 ) return p;
            p[0] = '.';
            p[1] = char( '0' + n / 100000000 ); n %= 100000000;
            if( prec == 1 ) return p + 2;
            p[2] = char( '0' + n / 10000000 );  n %= 10000000;
            if( prec == 2 ) return p + 3;
            p[3] = char( '0' + n / 1000000 );   n %= 1000000;
            if( prec == 3 ) return p + 4;
            p[4] = char( '0' + n / 100000 );    n %= 100000;
            if( prec == 4 ) return p + 5;
            p[5] = char( '0' + n / 10000 );     n %= 10000;
            if( prec == 5 ) return p + 6;
            p[6] = char( '0' + n / 1000 );      n %= 1000;
            if( prec == 6 ) return p + 7;
            p[7] = char( '0' + n / 100 );       n %= 100;
            if( prec == 7 ) return p + 8;
            p[8] = char( '0' + n / 10 );        n %= 10;
            if( prec == 8 ) return p + 9;
            p[9] = char( '0' + n );
            return p + 10;
            // clang-format on
        };
    }

    size_t time_zone::format_precise_to_s( string_view format,
        sysseconds_t                                   t,
        u32                                            nanos,
        int                                            precision,
        char*                                          buff,
        size_t                                         count ) const {
        return _do_strformat( *this,
            format.data(),
            format.size(),
            t,
            _write_nanos( nanos, precision ),
            WriteToBuff{ buff, count } );
    }

    std::string time_zone::format_precise_s(
        string_view format, sysseconds_t t, u32 nanos, int precision ) const {
        return _do_strformat( *this,
            format.data(),
            format.size(),
            t,
            _write_nanos( nanos, precision ),
            WriteToString{} );
    }

    std::string format_precise_s(
        string_view fmt, sysseconds_t t, u32 nanos, int precision ) {
        return _do_strformat(
            DummyTimeZoneUTC{},
            fmt.data(),
            fmt.size(),
            t,
            _write_nanos( nanos, precision ),
            WriteToString{} );
    }

    size_t format_precise_to_s( string_view fmt,
        sysseconds_t t,
        u32          nanos,
        int          precision,
        char*        buff,
        size_t       count ) {
        return _do_strformat(
            DummyTimeZoneUTC{},
            fmt.data(),
            fmt.size(),
            t,
            _write_nanos( nanos, precision ),
            WriteToBuff{ buff, count } );
    }

    string time_zone::format_s(
        std::string_view format, sysseconds_t t ) const {
        return _do_strformat(
            *this,
            format.data(),
            format.size(),
            t,
            // There is no fractional component, so write_frac is a noop
            []( char* s ) noexcept -> char* { return s; },
            WriteToString{} );
    }

    template<bool is_sane, bool is_compact, class FWriteAbbrev, class FAction>
    auto _format_local_to( sysseconds_t t_local,
        char* const                     p,
        char                            date_sep,
        char                            date_time_sep,
        FWriteAbbrev                    write_abbrev,
        FAction action ) noexcept( noexcept( action.dump( p,
        write_abbrev( p ) ) ) ) {
        // t = utc_t + gmtoff;
        auto parts          = vtz::math::div_floor2<86400>( t_local );
        auto date           = sysdays_t( parts.quot );
        auto time_intraday  = u32( parts.rem );
        int  hr             = time_intraday / 3600;
        time_intraday      %= 3600;
        int mi              = time_intraday / 60;
        time_intraday      %= 60;
        int  sec            = time_intraday;
        auto ymd            = to_civil( date );

        if constexpr( is_sane )
        {
            // the timestamp itself is 15 characters (if compact), otherwise 19
            // characters
            constexpr size_t stamp_end = is_compact ? 15 : 19;
            if constexpr( is_compact )
            {
                // We were promised that the year is 0000-9999
                (void)_write_year4( p, ymd.year );                // 0..4
                (void)_write_mon( p + 4, u8( ymd.month ) );       // 4..6
                (void)_write_dom_d( p + 6, u8( ymd.day ) );       // 6..8
                p[8] = date_time_sep;                             // 8..9
                (void)_write_hhmmsscompact( p + 9, hr, mi, sec ); // 9..15
            }
            else
            {
                // YYYY-MM-DD HH:MM:SS
                // 0123456789012345678
                (void)_write_year4( p, ymd.year );          // 0..4
                p[4] = date_sep;                            // 4..5
                (void)_write_mon( p + 5, u8( ymd.month ) ); // 5..7
                p[7] = date_sep;                            // 7..8
                (void)_write_dom_d( p + 8, u8( ymd.day ) ); // 8..10
                p[10] = date_time_sep;                      // 10..11
                (void)_write_hhmmss( p + 11, hr, mi, sec ); // 11..19
            }

            return action.dump( p, stamp_end + write_abbrev( p + stamp_end ) );
        }
        else
        {
            // the timestamp is longer than expected. Write the year first,
            // and then write the remainder after the year
            char*  q        = _write_year( p, ymd.year );
            size_t year_len = q - p;

            constexpr size_t stamp_end = is_compact ? 11 : 15;
            if constexpr( is_compact )
            {
                // MMDD HHMMSS
                // 01234567890
                (void)_write_mon( q, u8( ymd.month ) );           // 0..2
                (void)_write_dom_d( q + 2, u8( ymd.day ) );       // 2..4
                q[4] = date_time_sep;                             // 4..5
                (void)_write_hhmmsscompact( q + 5, hr, mi, sec ); // 5..11
            }
            else
            {
                // -MM-DD HH:MM:SS
                // 012345678901234
                q[0] = date_sep;                            // 0..1
                (void)_write_mon( q + 1, u8( ymd.month ) ); // 1..3
                q[3] = date_sep;                            // 3..4
                (void)_write_dom_d( q + 4, u8( ymd.day ) ); // 4..6
                q[6] = date_time_sep;                       // 6..7
                (void)_write_hhmmss( q + 7, hr, mi, sec );  // 7..14
            }

            return action.dump(
                p, year_len + stamp_end + write_abbrev( q + stamp_end ) );
        }
    }

    template<bool include_abbr_sep, bool use_stdoff>
    static auto _write_abbrev(
        time_zone const& tz, sysseconds_t t, i32 gmtoff, char abbrev_sep ) {
        if constexpr( use_stdoff )
        {
            if constexpr( include_abbr_sep )
            {
                return [gmtoff, abbrev_sep]( char* p ) noexcept {
                    *p = abbrev_sep;
                    return 1 + write_shortest_offset( gmtoff, p + 1 );
                };
            }
            else
            {
                return [gmtoff]( char* p ) noexcept {
                    return write_shortest_offset( gmtoff, p );
                };
            }
        }
        else
        {
            if constexpr( include_abbr_sep )
            {
                return [&tz, t, abbrev_sep]( char* p ) noexcept {
                    *p = abbrev_sep;
                    return 1 + tz.abbrev_to_s( t, p + 1 );
                };
            }
            else
            {
                return [&tz, t]( char* p ) noexcept {
                    return tz.abbrev_to_s( t, p );
                };
            }
        }
    }

    template<bool is_compact, bool use_stdoff, bool include_abbr_sep>
    VTZ_INLINE static std::string _do_format( time_zone const& tz,
        sysseconds_t                                           t,
        char                                                   date_sep,
        char                                                   date_time_sep,
        char                                                   abbr_sep ) {
        constexpr u64 LONG_TIME = resolve_civil_time( 10000, 1, 1, 0, 0, 0 );

        auto off     = i32( tz.offset_s( t ) ); // offset should be 32-bit
        auto local_t = t + off;

        char buff[64];

        if( u64( local_t ) < LONG_TIME ) VTZ_LIKELY
        {
            return _format_local_to<true, is_compact>( local_t,
                buff,
                date_sep,
                date_time_sep,
                _write_abbrev<include_abbr_sep, use_stdoff>(
                    tz, t, off, abbr_sep ),
                WriteToString{} );
        }
        else
        {
            return _format_local_to<false, is_compact>( local_t,
                buff,
                date_sep,
                date_time_sep,
                _write_abbrev<include_abbr_sep, use_stdoff>(
                    tz, t, off, abbr_sep ),
                WriteToString{} );
        }
    }

    /// Fast formatting - format as:
    ///
    ///     `YYYY<date_sep>MM<date_sep>DD<date_time_sep>HH:MM:SS<abbr_sep>[Abbrev]
    ///
    /// The various flags control if the separators actually appear

    template<bool is_compact, bool use_stdoff, bool include_abbr_sep>
    VTZ_INLINE static size_t _do_format_to( time_zone const& tz,
        sysseconds_t                                         t,
        char*                                                p,
        size_t                                               count,
        char                                                 date_sep,
        char                                                 date_time_sep,
        char abbr_sep ) noexcept {
        ///
        constexpr u64 LONG_TIME = resolve_civil_time( 10000, 1, 1, 0, 0, 0 );

        auto gmtoff  = i32( tz.offset_s( t ) );
        auto local_t = t + gmtoff;
        if( u64( local_t ) < LONG_TIME ) VTZ_LIKELY
        {
            // Required size is (number of characters for timestamp) + (1 for
            // abbr sep) + (max abbr length)
            constexpr size_t REQUIRED_SIZE
                = ( is_compact ? 15 : 19 ) + include_abbr_sep + 7;
            // If we're _before_ LONG_TIME, we know the year is 0000-9999
            // So the length is at most, eg,
            // 9999-12-31T23:59:59 LongAbbr
            if( count >= REQUIRED_SIZE ) VTZ_LIKELY
            {
                // If the buffer is big enough, we can write to the buffer
                // directly
                return _format_local_to<true, is_compact>( local_t,
                    p,
                    date_sep,
                    date_time_sep,
                    _write_abbrev<include_abbr_sep, use_stdoff>(
                        tz, t, gmtoff, abbr_sep ),
                    WriteNOOP{} );
            }
            else
            {
                static_assert( REQUIRED_SIZE < 32 );
                char buff[32];
                return _format_local_to<true, is_compact>( local_t,
                    buff,
                    date_sep,
                    date_time_sep,
                    _write_abbrev<include_abbr_sep, use_stdoff>(
                        tz, t, gmtoff, abbr_sep ),
                    WriteToBuff{ p, count } );
            }
        }
        else
        {
            // Required size is (number of characters for timestamp) + (1 for
            // abbr sep) + (max abbr length).
            //
            // Year may be up to 13 characters, given by:
            //
            //     len(str(-2**63 // (365 * 86400)))
            constexpr size_t REQUIRED_SIZE
                = 13 + ( is_compact ? 11 : 15 ) + include_abbr_sep + 7;
            if( count >= REQUIRED_SIZE )
            {
                // If the buffer is big enough, we can write to the buffer
                // directly
                return _format_local_to<false, is_compact>( local_t,
                    p,
                    date_sep,
                    date_time_sep,
                    _write_abbrev<include_abbr_sep, use_stdoff>(
                        tz, t, gmtoff, abbr_sep ),
                    WriteNOOP{} );
            }
            else
            {
                static_assert( REQUIRED_SIZE < 64 );
                char buff[64];
                return _format_local_to<false, is_compact>( local_t,
                    buff,
                    date_sep,
                    date_time_sep,
                    _write_abbrev<include_abbr_sep, use_stdoff>(
                        tz, t, gmtoff, abbr_sep ),
                    WriteToBuff{ p, count } );
            }
        }
    }

    // clang-format off
    string time_zone::format_iso8601_s( sysseconds_t t, char date_sep, char date_time_sep ) const {
        return _do_format<false, true, false>( *this, t, date_sep, date_time_sep, '\0' );
    }

    size_t time_zone::format_iso8601_to_s( sysseconds_t t, char* buff, size_t count, char date_sep, char date_time_sep ) const noexcept {
        return _do_format_to<false, true, false>( *this, t, buff, count, date_sep, date_time_sep, '\0' );
    }

    string time_zone::format_s( sysseconds_t t, char date_sep, char date_time_sep, char abbrev_sep ) const {
        return _do_format<false, false, true>( *this, t, date_sep, date_time_sep, abbrev_sep );
    }

    size_t time_zone::format_to_s( sysseconds_t t, char* buff, size_t count, char date_sep, char date_time_sep, char abbrev_sep ) const noexcept {
        return _do_format_to<false, false, true>( *this, t, buff, count, date_sep, date_time_sep, abbrev_sep );
    }

    string time_zone::format_compact_s( sysseconds_t t, char date_time_sep, char abbrev_sep ) const {
        return _do_format<true, false, true>( *this, t, '\0', date_time_sep, abbrev_sep );
    }

    size_t time_zone::format_compact_to_s( sysseconds_t t, char* p, size_t count, char date_time_sep, char abbrev_sep ) const noexcept {
        return _do_format_to<true, false, true>( *this, t, p, count, '\0', date_time_sep, abbrev_sep );
    }
    // clang-format on
} // namespace vtz
