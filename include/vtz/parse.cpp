

#include <vtz/strings.h>
#include <ctime>
#include <fmt/format.h>
#include <iomanip>
#include <sstream>
#include <string>
#include <vtz/civil.h>

namespace vtz {

    namespace {
        struct ParseFail {
            char const* p;
            char const* msg;
        };
    } // namespace


    VTZ_INLINE static bool isWhitespace( char ch ) {
        return ch == ' '      //
               || ch == '\t'  //
               || ch == '\n'  //
               || ch == '\r'  //
               || ch == '\xb' //
               || ch == '\xc';
    }

    VTZ_INLINE static bool parseDigitTo( char ch, i64& dest ) noexcept {
        int  x      = ch - '0';
        bool good   = size_t( x ) < 10;
        i64  newVal = dest * 10 + x;
        if( good ) { dest = newVal; }
        return good;
    }

    VTZ_INLINE static i64 parseYear( char const*& p, char const* end ) {
        i64 result = 0;

        if( p != end && parseDigitTo( *p, result ) )
            ++p;
        else
            throw ParseFail{ p, "Expected digit" };
        if( p != end && parseDigitTo( *p, result ) ) ++p;
        if( p != end && parseDigitTo( *p, result ) ) ++p;
        if( p != end && parseDigitTo( *p, result ) ) ++p;
        return result;
    }


    VTZ_INLINE i64 parseD2( char const*& p, char const* end ) {
        i64 result = 0;

        if( p != end && parseDigitTo( *p, result ) )
            ++p;
        else
            throw ParseFail{ p, "Expected digit" };
        if( p != end && parseDigitTo( *p, result ) ) ++p;
        return result;
    }

    VTZ_INLINE i64 parseD3( char const*& p, char const* end ) {
        i64 result = 0;

        if( p != end && parseDigitTo( *p, result ) )
            ++p;
        else
            throw ParseFail{ p, "Expected digit" };
        if( p != end && parseDigitTo( *p, result ) ) ++p;
        if( p != end && parseDigitTo( *p, result ) ) ++p;
        return result;
    }

    VTZ_INLINE i64 parseFracAsNanos(
        char const*& p, char const* end ) noexcept {
        // If there are fewer than 2 characters left, or there's no decimal
        // point, there's nothing to parse. return 0
        if( end - p < 2 || *p != '.' ) return 0;

        i64 frac = 0;
        if( !parseDigitTo( p[1], frac ) )
        {
            // If we couldn't parse the digit, don't consume the '.', just
            // return 0
            return 0;
        }
        // We've consumed 2 characters
        p += 2;
        // we have 1 digit now.
        if( p == end || !parseDigitTo( *p, frac ) ) return frac * 100000000;
        ++p; // we have 2 digits now
        if( p == end || !parseDigitTo( *p, frac ) ) return frac * 10000000;
        ++p; // we have 3 digits now
        if( p == end || !parseDigitTo( *p, frac ) ) return frac * 1000000;
        ++p; // we have 4 digits now
        if( p == end || !parseDigitTo( *p, frac ) ) return frac * 100000;
        ++p; // we have 5 digits now
        if( p == end || !parseDigitTo( *p, frac ) ) return frac * 10000;
        ++p; // we have 6 digits now
        if( p == end || !parseDigitTo( *p, frac ) ) return frac * 1000;
        ++p; // we have 7 digits now
        if( p == end || !parseDigitTo( *p, frac ) ) return frac * 100;
        ++p; // we have 8 digits now
        if( p == end || !parseDigitTo( *p, frac ) ) return frac * 10;
        ++p; // we have 9 digits now; stop parsing
        return frac;
    }

    template<int MIN_VAL, int MAX_VAL>
    VTZ_INLINE i64 parseD1( char const*& p, char const* end ) {
        if( p != end )
        {
            char ch = *p;
            int  v  = ch - '0';
            if( MIN_VAL <= v && v <= MAX_VAL ) { return v; }
        }
        throw std::runtime_error( fmt::format(
            "Expected 1 digit in range {}-{}", MIN_VAL, MAX_VAL ) );
    }

    template<class F>
    static auto withCstr( string_view sv, F&& func ) {
        constexpr size_t BUFF_SIZE = 128;
        if( sv.size() < BUFF_SIZE )
        {
            alignas( 16 ) char buff[BUFF_SIZE];

            _vtz_memcpy( buff, sv.data(), sv.size() );

            buff[sv.size()] = '\0';
            return func( const_cast<char const*>( buff ) );
        }
        else
        {
            // It's too large - just use a std::string
            return func( std::string( sv ).c_str() );
        }
    }

    static sysdays_t dateFromTM( std::tm const& t ) noexcept {
        int year = 1900 + t.tm_year;
        if( t.tm_mday || t.tm_mon )
        {
            int mday = t.tm_mday;
            // If the mday wasn't specified, assume the 1st
            if( mday == 0 ) mday = 1;
            return resolveCivil( year, t.tm_mon + 1, mday );
        }
        else
        {
            // Use the day of the year. if the day of the year is 0, this
            // will just give us the first of the year.
            return resolveCivilOrdinal( t.tm_year, t.tm_yday + 1 );
        }
    }

    /// Convert from `tm` struct to seconds since the epoch
    static sec_t timeFromTM( std::tm const& t ) noexcept {
        return t.tm_hour * 3600ll + t.tm_min * 60ll + t.tm_sec;
    }

    template<class F>
    auto doParse( string_view format, string_view input, F func )
        -> decltype( func( i32(), i32(), i32() ) ) try
    {
        if( format.size() == 0 )
        {
            // Format string is empty. So: date is epoch date, time of day is 0,
            // nanos is 0
            return func( 0, 0, 0 );
        }

        char const* f = format.data();
        char const* p = input.data();

        // We want to be able to consume an additional character when parsing
        // the format string
        size_t len = format.size() - 1;

        char const* pEnd  = p + input.size();
        char const* fBack = f + len;

        bool hasC      = false;
        bool hasSmallY = false;

        i64 year  = 1970;
        int month = 1;
        int dom   = 1;
        int doy   = 0;
        int hr    = 0;
        int mi    = 0;
        int se    = 0;
        i32 nanos = 0;
        while( f < fBack && p < pEnd )
        {
            char c = *f++;
            if( c != '%' )
            {
                if( c == *p++ ) continue;
                // Mismatched character
                throw ParseFail{ p,
                    "Character doesn't match input format string" };
            }

            // We've encountered a format specifier, so we consume an additional
            // character
            char next = *f++;
            switch( next )
            {
            // LITERAL SPECIFIERS

            // Expect literal '%'
            case '%':
                if( '%' == *p++ ) continue;
                throw ParseFail{ p - 1, "Expected literal '%'" };
            // Matches any whitespace
            case 'n':
            case 't':
                while( p < pEnd && isWhitespace( *p ) ) ++p;
                continue;

            // PARSE YEAR

            // Expect Year
            case 'Y': year = parseYear( p, pEnd ); continue;
            case 'y':
                {
                    hasSmallY = true;
                    auto y    = parseD2( p, pEnd );
                    // If we have the first two digits, just add the last two
                    if( hasC ) { year += y; }
                    else
                    {
                        // Range [69,99] results in values 1969 to 1999, range
                        // [00,68] results in 2000-2068
                        year = y + ( y < 69 ? 2000 : 1900 );
                    }
                    continue;
                }
            // parses the first 2 digits of year as a decimal number (range
            // [00,99])
            case 'C':
                {
                    hasC = true;
                    if( hasSmallY )
                    {
                        // if '%y' was parsed, treat as number 0-99
                        if( year < 2000 ) { year -= 1900; }
                        else { year -= 2000; }
                        // add in two digits for '%C'
                        year += parseD2( p, pEnd ) * 100;
                    }
                    else
                    {
                        // Overwrite the year with the correct two digits
                        year = parseD2( p, pEnd ) * 100;
                    }

                    continue;
                }

            // MONTH

            // parse month as decimal number (range [01,12]), leading 0s
            // permitted but not required
            case 'm': month = parseD2( p, pEnd ); continue;

            // DAY OF THE YEAR/MONTH

            // parses the day of the year as a decimal number (range [001,366]),
            // leading 0s permitted but not required
            case 'j': doy = parseD3( p, pEnd ); continue;
            // parses the day of the month as a decimal number (range [01,31]),
            // leading 0s permitted but not required
            case 'e': // (synonym of 'd')
            case 'd': dom = parseD2( p, pEnd ); continue;

            // DAY OF THE WEEK

            // weekday 0-6, sunday is 0
            case 'w':
                {
                    if( p < pEnd )
                    {
                        int weekday = *p++ - '0';
                        if( weekday < 0 || weekday > 6 )
                            throw ParseFail{ p - 1,
                                "Expected weekday (range [0-6], Sunday is 0)" };
                    }
                    continue;
                }
            // weekday 1-7, monday is 1
            case 'u':
                {
                    if( p < pEnd )
                    {
                        int weekday = *p++ - '0';
                        if( weekday < 1 || weekday > 7 )
                            throw ParseFail{ p - 1,
                                "Expected weekday (range [1-7], Monday is 1)" };
                    }
                    continue;
                }

            // HOUR, MINUTE, SECOND

            // parses the hour as a decimal number, 24 hour clock (range
            // [00-23]), leading 0s permitted but not required
            case 'H': hr = parseD2( p, pEnd ); continue;
            // minute
            case 'M': mi = parseD2( p, pEnd ); continue;
            // second
            case 'S':
                {
                    se    = parseD2( p, pEnd );
                    nanos = parseFracAsNanos( p, pEnd );
                    continue;
                }
            // %H:%M
            case 'R':
                {
                    hr = parseD2( p, pEnd );
                    if( p == pEnd || *p != ':' )
                        throw ParseFail{ p,
                            "Expected ':' followed by minute (range [00,59])" };
                    ++p;
                    mi = parseD2( p, pEnd );
                    continue;
                }
            // '%F' is equivalent to %Y-%m-%d
            case 'F':
                {
                    year = parseYear( p, pEnd );
                    if( p == pEnd || *p != '-' )
                        throw ParseFail{ p,
                            "Expected '-' followed by month (range [01,12])" };
                    ++p;
                    month = parseD2( p, pEnd );
                    if( p == pEnd || *p != '-' )
                        throw ParseFail{ p,
                            "Expected '-' followed by day (range [01,31])" };
                    ++p;
                    dom = parseD2( p, pEnd );
                    continue;
                }
            // %H:%M:%S
            case 'T':
                {
                    hr = parseD2( p, pEnd );
                    if( p == pEnd || *p != ':' )
                        throw ParseFail{ p,
                            "Expected ':' followed by minute and second" };
                    ++p;
                    mi = parseD2( p, pEnd );
                    if( p == pEnd || *p != ':' )
                        throw ParseFail{ p,
                            "Expected ':' followed by second (range [00,60])" };
                    ++p;
                    se    = parseD2( p, pEnd );
                    nanos = parseFracAsNanos( p, pEnd );
                    continue;
                }
            // Parses the time zone abbreviation or name, taken as the longest
            // sequence of characters that only contains the characters A
            // through Z, a through z, 0 through 9, -, +, _, and /.
            case 'Z':
                {
                    while( p != pEnd )
                    {
                        char c       = *p;
                        bool isZChar = 'A' <= c && c <= 'Z'    //
                                       || 'a' <= c && c <= 'z' //
                                       || '0' <= c && c <= '9' //
                                       || c == '-'             //
                                       || c == '+'             //
                                       || c == '_'             //
                                       || c == '/';
                        if( !isZChar ) break;
                        ++p;
                    }
                    continue;
                }

            default:
                {
                    return withCstr( format, [&]( char const* formatCstr ) {
                        auto ss = std::istringstream( std::string( input ) );

                        std::tm tm{};
                        ss >> std::get_time( &tm, formatCstr );

                        if( ss.fail() )
                        {
                            throw std::runtime_error( fmt::format(
                                "parse_time(): Unable to parse {} as {}. NOTE: "
                                "fallback to standard library required because "
                                "of '%{}' specifier",
                                escapeString( input ),
                                escapeString( formatCstr ),
                                next ) );
                        }

                        return func( dateFromTM( tm ), timeFromTM( tm ), 0 );
                    } );
                }
            }
        }

        if( f <= fBack )
        {
            // We reached the end of the input before the full format string was
            // consumed
            if( f < fBack )
                throw ParseFail{ p,
                    "End of input reached before format string was consumed" };
            // check last literal character
            if( *f != *p )
                throw ParseFail{ p,
                    "Character doesn't match input format string" };
            ++p;
        }

        sysdays_t date = bool( doy ) // Check if we have an ordinal date
                                     // use ordinal date
                             ? date = resolveCivilOrdinal( year, doy )
                             // use (year, month, day)
                             : date = resolveCivil( year, month, dom );

        i32 timeOfDay = i32( hr ) * 3600 + i32( mi ) * 60 + i32( se );

        return func( date, timeOfDay, nanos );
    }
    catch( ParseFail const& p )
    {
        auto pos = p.p - input.data();
        if( size_t( pos ) < input.size() )
        {
            char ch = *p.p;
            throw std::runtime_error(
                fmt::format( "Error @ input[{}] (char='{}'). Unable to parse "
                             "{} with format={}: {}",
                    pos,
                    ch,
                    escapeString( input ),
                    escapeString( format ),
                    p.msg ) );
        }
        else
        {
            throw std::runtime_error(
                fmt::format( "Error: unexpected end of input. Unable to parse "
                             "{} with format={}: {}",
                    escapeString( input ),
                    escapeString( format ),
                    p.msg ) );
        }
    }

    sysdays_t parse_date_d( string_view fmt, string_view dateStr ) {
        return doParse( fmt, dateStr, []( i32 date, i32 time, i32 nanos ) {
            return date;
        } );
    }

    sec_t parse_time_s( string_view fmt, string_view dateStr ) {
        return doParse( fmt, dateStr, []( i32 date, i32 time, i32 nanos ) {
            return date * 86400ll + time;
        } );
    }

    nanos_t parse_time_ns( string_view fmt, string_view dateStr ) {
        return doParse( fmt, dateStr, []( i32 date, i32 time, i32 nanos ) {
            return ( date * 86400ll + time ) * 1000000000ll + nanos;
        } );
    }
} // namespace vtz
