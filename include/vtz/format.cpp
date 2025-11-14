
#include <vtz/bit.h>
#include <vtz/civil.h>
#include <vtz/tz.h>

namespace vtz {

    class FormatError : public std::exception {
        char const* what_;

      public:

        FormatError( char const* what ) noexcept
        : what_( what ) {}

        char const* what() const noexcept override { return what_; }
    };

    FormatError bufferTooSmall() noexcept {
        return FormatError( "format error: destination buffer is too small" );
    }

    // Write a year, assuming the year is positive and fits in 4 digits, and
    // the bugger is big enough
    VTZ_INLINE static char* _writeYear4( char* p, i64 year ) noexcept {
        u16  y      = u16( year );
        auto y1000  = y / 1000;
        y          %= 1000;
        auto y100   = y / 100;
        y          %= 100;
        auto y10    = y / 10;
        y          %= 10;
        auto y1     = y;
        p[0]        = '0' + y1000;
        p[1]        = '0' + y100;
        p[2]        = '0' + y10;
        p[3]        = '0' + y1;
        return p + 4;
    }
    /// Writes the year. Assumes there are at least 22 characters of space in
    /// the buffer
    VTZ_INLINE static char* _writeYear( char* p, i64 year ) noexcept {
        // if the year is nonnegative and fits in 4 digits, write as 4-digit
        // year
        if( u64( year ) < 10000 ) [[likely]]
            return _writeYear4( p, year );
        // fallback to std::to_chars
        return std::to_chars( p, p + 22, year ).ptr;
    }

    /// Writes the last 2 digits of the year, as a decimal number
    VTZ_INLINE static char* _writeYearShort( char* p, i64 year ) noexcept {
        u8 x = u8( std::abs( year ) % 100 );
        p[0] = '0' + x / 10;
        p[1] = '0' + x % 10;
        return p + 2;
    }

    /// %m specifier - write the month as a decimal in the range [01, 12]
    VTZ_INLINE static char* _writeMon( char* p, u8 mon ) noexcept {
        p[0] = '0' + ( mon >= 10 );
        p[1] = '0' + ( mon % 10 );
        return p + 2;
    }

    /// writes day of the month as a decimal number (range [01,31])
    VTZ_INLINE static char* _writeDOM_d( char* p, u8 day ) noexcept {
        char c0 = '0' + day / 10;
        char c1 = '0' + day % 10;
        p[0]    = c0;
        p[1]    = c1;
        return p + 2;
    }

    /// writes day of the month as a decimal number (range [1,31]).
    /// Single digit is preceded by a space.
    VTZ_INLINE static char* _writeDOM_e( char* p, u8 day ) noexcept {
        char c0 = '0' + day / 10;
        char c1 = '0' + day % 10;
        if( day < 10 ) c0 = ' ';
        p[0] = c0;
        p[1] = c1;
        return p + 2;
    }

    /// equivalent to "%H:%M"
    VTZ_INLINE static char* _writeISO_HHMM( char* p, int h, int m ) noexcept {
        p[0] = '0' + h / 10;
        p[1] = '0' + h % 10;
        p[2] = ':';
        p[3] = '0' + m / 10;
        p[4] = '0' + m % 10;
        return p + 5;
    }

    /// equivalent to "%H%M%S" (the ISO 8601 time format)
    VTZ_INLINE static char* _writeHHMMSScompact(
        char* p, int h, int m, int s ) noexcept {
        p[0] = '0' + h / 10;
        p[1] = '0' + h % 10;
        p[2] = '0' + m / 10;
        p[3] = '0' + m % 10;
        p[4] = '0' + s / 10;
        p[5] = '0' + s % 10;
        return p + 6;
    }

    /// equivalent to "%H:%M:%S" (the ISO 8601 time format)
    VTZ_INLINE static char* _writeHHMMSS(
        char* p, int h, int m, int s ) noexcept {
        p[0] = '0' + h / 10;
        p[1] = '0' + h % 10;
        p[2] = ':';
        p[3] = '0' + m / 10;
        p[4] = '0' + m % 10;
        p[5] = ':';
        p[6] = '0' + s / 10;
        p[7] = '0' + s % 10;
        return p + 8;
    }
    /// Write a 2-digit int. Assumes value is < 100
    VTZ_INLINE static char* _write2Digit( char* p, u8 x ) noexcept {
        p[0] = '0' + x / 10;
        p[1] = '0' + x % 10;
        return p + 2;
    }
    /// Write a 3-digit int. Assumes value is < 1000
    VTZ_INLINE static char* _write3Digit( char* p, u16 x ) noexcept {
        p[0]  = '0' + x / 100;
        x    %= 100;
        p[1]  = '0' + x / 10;
        p[2]  = '0' + x % 10;
        return p + 3;
    }

    /// equivalent to "%Y-%m-%d" (the ISO 8601 date format)
    /// Assumes there is at least 28 characters of space in the buffer
    /// (21 characters needed for excessively large negative/positive year)
    VTZ_INLINE static char* _writeISO_date(
        char* p, i64 y, int m, int d ) noexcept {
        if( u64( y ) < 10000 ) [[likely]]
        {
            p    = _writeYear4( p, y );
            *p++ = '-';
            p    = _writeMon( p, m );
            *p++ = '-';
            p    = _writeDOM_d( p, d );
            return p;
        }
        p    = _writeYear( p, y );
        *p++ = '-';
        p    = _writeMon( p, m );
        *p++ = '-';
        p    = _writeDOM_d( p, d );
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
        size_t                           formatSize,
        sysseconds_t                     t,
        WriteFractional                  writeFrac,
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

        if( formatSize >= MAX_SIZE )
            throw FormatError(
                "formatTime(): input format string is too long" );

        if( formatSize == 0 ) return func.dump( "", 0 );

        auto gmtoff        = tz.offset_s( t );
        auto tLocal        = t + gmtoff;
        auto parts         = vtz::math::divFloor2<86400>( tLocal );
        auto date          = parts.quot;
        u32  timeIntraday  = parts.rem;
        int  hr            = timeIntraday / 3600;
        timeIntraday      %= 3600;
        int mi             = timeIntraday / 60;
        timeIntraday      %= 60;
        int  sec           = timeIntraday;
        auto ymd           = toCivil( date );

        // We're going to handle as many format specifiers as we can ourselves,
        // so that we can avoid a call to strftime.
        //
        // How big do we need to make this buffer?
        // Well, we know there are at most (formatSize/2) specifiers.
        // This means a max of 64 specifiers.
        char fmtStr[BUFF_SIZE];

        char*  p = fmtStr;
        size_t i = 0;
        // Iterate until 1 before the end. This ensures we don't do an
        // out-of-bounds read on a format specifier
        size_t len           = formatSize - 1;
        bool   needsStrftime = false;
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
            case 'Y': p = _writeYear( p, ymd.year ); continue;
            // writes last 2 digits of year as a decimal number (range
            // [00,99])
            case 'y': p = _writeYearShort( p, ymd.year ); continue;
            // writes month as a decimal number (range [01,12])
            case 'm': p = _writeMon( p, ymd.month ); continue;
            // 	writes day of the month as a decimal number (range
            // [01,31])
            case 'd': p = _writeDOM_d( p, ymd.day ); continue;
            // writes day of the year as a decimal number (range [001,366])
            case 'j':
                {
                    u16 yday = date - resolveCivil( ymd.year, 1, 1 );

                    p = _write3Digit( p, 1 + yday );
                    continue;
                }
            // writes weekday as a decimal number, where Sunday is 0 (range
            // [0-6])
            case 'w':
                {
                    static_assert( int( DOW::Sun ) == 0 );
                    auto dow = int( dowFromDays( date ) );
                    *p++     = '0' + dow;
                    continue;
                }
            // 	writes weekday as a decimal number, where Monday is 1 (ISO 8601
            // format) (range [1-7])
            case 'u':
                {
                    auto dow = int( dowFromDays( date ) );
                    if( dow == 0 ) dow = 7;
                    *p++ = '0' + dow;
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
            case 'e': p = _writeDOM_e( p, ymd.day ); continue;
            // equivalent to "%Y-%m-%d" (the ISO 8601 date format)
            case 'F':
                p = _writeISO_date( p, ymd.year, ymd.month, ymd.day );
                continue;
            // equivalent to "%H:%M"
            case 'R': p = _writeISO_HHMM( p, hr, mi ); continue;
            // 	writes hour as a decimal number, 24 hour clock (range
            // [00-23])
            case 'H': p = _write2Digit( p, hr ); continue;
            // writes minute as a decimal number (range [00,59])
            case 'M': p = _write2Digit( p, mi ); continue;
            // writes second as a decimal number (range [00,60]). Will also
            // write a fractional component after the second.
            case 'S': p = writeFrac( _write2Digit( p, sec ) ); continue;
            // equivalent to "%H:%M:%S" (the ISO 8601 time format)
            case 'T': p = writeFrac( _writeHHMMSS( p, hr, mi, sec ) ); continue;
            // writes offset from UTC in the ISO 8601 format (e.g. -0430)
            case 'z': p += writeShortestOffset( gmtoff, p ); continue;
            case 'Z': p += tz.abbrev_to_s( t, p ); continue;
            default:
                {
                    *p++          = c;
                    *p++          = next;
                    needsStrftime = true;
                    continue;
                }
            }
        }
        // If the format string did not end in a format specifier, make sure
        // to copy the final character.
        if( i == len ) { *p++ = format[i]; }

        if( !needsStrftime ) [[likely]]
        {
            // If we were able to handle all the specifiers, there's nothing
            // left to format. This means we don't need to do a call to
            // strftime, and we can exit early!
            return func.dump( fmtStr, size_t( p - fmtStr ) );
        }

        // Add null terminator to format string
        *p = '\0';

        auto stdoff = tz.stdoff_s( t );
        bool isDST  = gmtoff != stdoff;

        auto tmValue = std::tm{
            sec,
            mi,
            hr,
            ymd.day,
            ymd.month - 1, // the 'tm' struct uses 0-11 for the month
            // tm_year stores years since 1900
            ymd.year - 1900,
            // days since sunday - 0-6
            int( dowFromDays( date ) - DOW::Sun ),
            // days since January 1 – [​0​, 365]
            int( date - resolveCivil( ymd.year, 1, 1 ) ),
            // true if daylight savings time is in effect
            isDST,
        };

        // Fill in any remaining format specifiers with strftime.
        // We will use this if there are any locale-dependent format specifiers
        // (asize from %Z)

        char   buffer[BUFF_SIZE];
        size_t count
            = std::strftime( buffer, sizeof( buffer ), fmtStr, &tmValue );
        return func.dump( buffer, count );
    }

    size_t TimeZone::format_to_s(
        string_view format, sysseconds_t t, char* buff, size_t count ) const {
        return _do_strformat(
            *this,
            format.data(),
            format.size(),
            t,
            // There is no fractional component, so writeFrac is a noop
            []( char* s ) noexcept -> char* { return s; },
            WriteToBuff{ buff, count } );
    }


    struct DummyTimeZone {
        /// NOOP - offset is 0
        static sec_t offset_s( sysseconds_t ) noexcept { return 0; }
        static sec_t stdoff_s( sysseconds_t t ) noexcept { return 0; }
        /// NOOP - no abbreviation to write
        static size_t abbrev_to_s( sec_t, char* ) noexcept { return 0; }
    };

    std::string format_date_d( string_view fmt, sysdays_t days ) {
        return _do_strformat(
            DummyTimeZone{},
            fmt.data(),
            fmt.size(),
            daysToSeconds( days ),
            // There is no fractional component, so writeFrac is a noop
            []( char* s ) noexcept -> char* { return s; },
            WriteToString{} );
    }

    size_t format_date_to_d(
        string_view fmt, sysdays_t days, char* buff, size_t count ) {
        return _do_strformat(
            DummyTimeZone{},
            fmt.data(),
            fmt.size(),
            daysToSeconds( days ),
            // There is no fractional component, so writeFrac is a noop
            []( char* s ) noexcept -> char* { return s; },
            WriteToBuff{ buff, count } );
    }

    auto _writeNanos( u32 nanos, int precision ) noexcept {
        return [nanos, precision]( char* p ) noexcept -> char* {
            // clang-format off
            u32 n = nanos;
            int prec = precision;
            if( prec == 0 ) return p;
            p[0] = '.';
            p[1] = '0' + n / 100000000; n %= 100000000;
            if( prec == 1 ) return p + 2;
            p[2] = '0' + n / 10000000;  n %= 10000000;
            if( prec == 2 ) return p + 3;
            p[3] = '0' + n / 1000000;   n %= 1000000;
            if( prec == 3 ) return p + 4;
            p[4] = '0' + n / 100000;    n %= 100000;
            if( prec == 4 ) return p + 5;
            p[5] = '0' + n / 10000;     n %= 10000;
            if( prec == 5 ) return p + 6;
            p[6] = '0' + n / 1000;      n %= 1000;
            if( prec == 6 ) return p + 7;
            p[7] = '0' + n / 100;       n %= 100;
            if( prec == 7 ) return p + 8;
            p[8] = '0' + n / 10;        n %= 10;
            if( prec == 8 ) return p + 9;
            p[9] = '0' + n;
            return p + 10;
            // clang-format on
        };
    }

    size_t TimeZone::format_precise_to_s( string_view format,
        sysseconds_t                                  t,
        u32                                           nanos,
        int                                           precision,
        char*                                         buff,
        size_t                                        count ) const {
        return _do_strformat( *this,
            format.data(),
            format.size(),
            t,
            _writeNanos( nanos, precision ),
            WriteToBuff{ buff, count } );
    }

    std::string TimeZone::format_precise_s(
        string_view format, sysseconds_t t, u32 nanos, int precision ) const {
        return _do_strformat( *this,
            format.data(),
            format.size(),
            t,
            _writeNanos( nanos, precision ),
            WriteToString{} );
    }

    string TimeZone::format_s( std::string_view format, sysseconds_t t ) const {
        return _do_strformat(
            *this,
            format.data(),
            format.size(),
            t,
            // There is no fractional component, so writeFrac is a noop
            []( char* s ) noexcept -> char* { return s; },
            WriteToString{} );
    }

    template<bool isSane, bool isCompact, class FWriteAbbrev, class FAction>
    auto _format_local_to( sysseconds_t tLocal,
        char* const                     p,
        char                            dateSep,
        char                            dateTimeSep,
        FWriteAbbrev                    writeAbbrev,
        FAction action ) noexcept( noexcept( action.dump( p,
        writeAbbrev( p ) ) ) ) {
        // t = utc_t + gmtoff;
        auto parts         = vtz::math::divFloor2<86400>( tLocal );
        auto date          = parts.quot;
        u32  timeIntraday  = parts.rem;
        int  hr            = timeIntraday / 3600;
        timeIntraday      %= 3600;
        int mi             = timeIntraday / 60;
        timeIntraday      %= 60;
        int  sec           = timeIntraday;
        auto ymd           = toCivil( date );

        if constexpr( isSane )
        {
            // the timestamp itself is 15 characters (if compact), otherwise 19
            // characters
            constexpr size_t stampEnd = isCompact ? 15 : 19;
            if constexpr( isCompact )
            {
                // We were promised that the year is 0000-9999
                (void)_writeYear4( p, ymd.year );                // 0..4
                (void)_writeMon( p + 4, ymd.month );             // 4..6
                (void)_writeDOM_d( p + 6, ymd.day );             // 6..8
                p[8] = dateTimeSep;                              // 8..9
                (void)_writeHHMMSScompact( p + 9, hr, mi, sec ); // 9..15
            }
            else
            {
                // YYYY-MM-DD HH:MM:SS
                // 0123456789012345678
                (void)_writeYear4( p, ymd.year );          // 0..4
                p[4] = dateSep;                            // 4..5
                (void)_writeMon( p + 5, ymd.month );       // 5..7
                p[7] = dateSep;                            // 7..8
                (void)_writeDOM_d( p + 8, ymd.day );       // 8..10
                p[10] = dateTimeSep;                       // 10..11
                (void)_writeHHMMSS( p + 11, hr, mi, sec ); // 11..19
            }

            return action.dump( p, stampEnd + writeAbbrev( p + stampEnd ) );
        }
        else
        {
            // the timestamp is longer than expected. Write the year first,
            // and then write the remainder after the year
            char*  q       = _writeYear( p, ymd.year );
            size_t yearLen = q - p;

            constexpr size_t stampEnd = isCompact ? 11 : 15;
            if constexpr( isCompact )
            {
                // MMDD HHMMSS
                // 01234567890
                (void)_writeMon( q, ymd.month );                 // 0..2
                (void)_writeDOM_d( q + 2, ymd.day );             // 2..4
                q[4] = dateTimeSep;                              // 4..5
                (void)_writeHHMMSScompact( q + 5, hr, mi, sec ); // 5..11
            }
            else
            {
                // -MM-DD HH:MM:SS
                // 012345678901234
                q[0] = dateSep;                           // 0..1
                (void)_writeMon( q + 1, ymd.month );      // 1..3
                q[3] = dateSep;                           // 3..4
                (void)_writeDOM_d( q + 4, ymd.day );      // 4..6
                q[6] = dateTimeSep;                       // 6..7
                (void)_writeHHMMSS( q + 7, hr, mi, sec ); // 7..14
            }

            return action.dump(
                p, yearLen + stampEnd + writeAbbrev( q + stampEnd ) );
        }
    }

    template<bool includeAbbrSep, bool useStdoff>
    static auto _writeAbbrev(
        TimeZone const& tz, sysseconds_t t, i32 gmtoff, char abbrevSep ) {
        if constexpr( useStdoff )
        {
            if constexpr( includeAbbrSep )
            {
                return [gmtoff, abbrevSep]( char* p ) noexcept {
                    *p = abbrevSep;
                    return 1 + writeShortestOffset( gmtoff, p + 1 );
                };
            }
            else
            {
                return [gmtoff]( char* p ) noexcept {
                    return writeShortestOffset( gmtoff, p );
                };
            }
        }
        else
        {
            if constexpr( includeAbbrSep )
            {
                return [&tz, t, abbrevSep]( char* p ) noexcept {
                    *p = abbrevSep;
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

    template<bool isCompact, bool useStdoff, bool includeAbbrSep>
    VTZ_INLINE static std::string _do_format( TimeZone const& tz,
        sysseconds_t                                          t,
        char                                                  dateSep,
        char                                                  dateTimeSep,
        char                                                  abbrSep ) {
        constexpr u64 LONG_TIME = resolveCivilTime( 10000, 1, 1, 0, 0, 0 );

        auto off    = tz.offset_s( t );
        auto localT = t + off;

        char buff[64];

        if( u64( localT ) < LONG_TIME ) [[likely]]
        {
            return _format_local_to<true, isCompact>( localT,
                buff,
                dateSep,
                dateTimeSep,
                _writeAbbrev<includeAbbrSep, useStdoff>( tz, t, off, abbrSep ),
                WriteToString{} );
        }
        else
        {
            return _format_local_to<false, isCompact>( localT,
                buff,
                dateSep,
                dateTimeSep,
                _writeAbbrev<includeAbbrSep, useStdoff>( tz, t, off, abbrSep ),
                WriteToString{} );
        }
    }

    /// Fast formatting - format as:
    ///
    ///     `YYYY<dateSep>MM<dateSep>DD<dateTimeSep>HH:MM:SS<abbrSep>[Abbrev]
    ///
    /// The various flags control if the separators actually appear

    template<bool isCompact, bool useStdoff, bool includeAbbrSep>
    VTZ_INLINE static size_t _do_format_to( TimeZone const& tz,
        sysseconds_t                                        t,
        char*                                               p,
        size_t                                              count,
        char                                                dateSep,
        char                                                dateTimeSep,
        char                                                abbrSep ) noexcept {
        ///
        constexpr u64 LONG_TIME = resolveCivilTime( 10000, 1, 1, 0, 0, 0 );

        auto gmtoff = tz.offset_s( t );
        auto localT = t + gmtoff;
        if( u64( localT ) < LONG_TIME ) [[likely]]
        {
            // Required size is (number of characters for timestamp) + (1 for
            // abbr sep) + (max abbr length)
            constexpr size_t REQUIRED_SIZE
                = ( isCompact ? 15 : 19 ) + includeAbbrSep + 7;
            // If we're _before_ LONG_TIME, we know the year is 0000-9999
            // So the length is at most, eg,
            // 9999-12-31T23:59:59 LongAbbr
            if( count >= REQUIRED_SIZE ) [[likely]]
            {
                // If the buffer is big enough, we can write to the buffer
                // directly
                return _format_local_to<true, isCompact>( localT,
                    p,
                    dateSep,
                    dateTimeSep,
                    _writeAbbrev<includeAbbrSep, useStdoff>(
                        tz, t, gmtoff, abbrSep ),
                    WriteNOOP{} );
            }
            else
            {
                static_assert( REQUIRED_SIZE < 32 );
                char buff[32];
                return _format_local_to<true, isCompact>( localT,
                    buff,
                    dateSep,
                    dateTimeSep,
                    _writeAbbrev<includeAbbrSep, useStdoff>(
                        tz, t, gmtoff, abbrSep ),
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
                = 13 + ( isCompact ? 11 : 15 ) + includeAbbrSep + 7;
            if( count >= REQUIRED_SIZE )
            {
                // If the buffer is big enough, we can write to the buffer
                // directly
                return _format_local_to<false, isCompact>( localT,
                    p,
                    dateSep,
                    dateTimeSep,
                    _writeAbbrev<includeAbbrSep, useStdoff>(
                        tz, t, gmtoff, abbrSep ),
                    WriteNOOP{} );
            }
            else
            {
                static_assert( REQUIRED_SIZE < 64 );
                char buff[64];
                return _format_local_to<false, isCompact>( localT,
                    buff,
                    dateSep,
                    dateTimeSep,
                    _writeAbbrev<includeAbbrSep, useStdoff>(
                        tz, t, gmtoff, abbrSep ),
                    WriteToBuff{ p, count } );
            }
        }
    }

    // clang-format off
    string TimeZone::format_iso8601_s( sysseconds_t t, char dateSep, char dateTimeSep ) const {
        return _do_format<false, true, false>( *this, t, dateSep, dateTimeSep, '\0' );
    }

    size_t TimeZone::format_iso8601_to_s( sysseconds_t t, char* buff, size_t count, char dateSep, char dateTimeSep ) const noexcept {
        return _do_format_to<false, true, false>( *this, t, buff, count, dateSep, dateTimeSep, '\0' );
    }

    string TimeZone::format_s( sysseconds_t t, char dateSep, char dateTimeSep, char abbrevSep ) const {
        return _do_format<false, false, true>( *this, t, dateSep, dateTimeSep, abbrevSep );
    }

    size_t TimeZone::format_to_s( sysseconds_t t, char* buff, size_t count, char dateSep, char dateTimeSep, char abbrevSep ) const noexcept {
        return _do_format_to<false, false, true>( *this, t, buff, count, dateSep, dateTimeSep, abbrevSep );
    }

    string TimeZone::format_compact_s( sysseconds_t t, char dateTimeSep, char abbrevSep ) const {
        return _do_format<true, false, true>( *this, t, '\0', dateTimeSep, abbrevSep );
    }

    size_t TimeZone::format_compact_to_s( sysseconds_t t, char* p, size_t count, char dateTimeSep, char abbrevSep ) const noexcept {
        return _do_format_to<true, false, true>( *this, t, p, count, '\0', dateTimeSep, abbrevSep );
    }
    // clang-format on
} // namespace vtz
