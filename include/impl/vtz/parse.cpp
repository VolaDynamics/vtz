

#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vtz/civil.h>
#include <vtz/strings.h>
#include <vtz/util/microfmt.h>

#include <vtz/parse.h>

namespace vtz {
    template<class F>
    auto _do_parse( string_view format, string_view input, F func )
        -> decltype( func( i32(), i32(), i32() ) );

    sysdays_t parse_date_d( string_view fmt, string_view date_str ) {
        return _do_parse( fmt, date_str, []( i32 date, i32 time, i32 nanos ) {
            (void)time;
            (void)nanos;
            return date;
        } );
    }

    sec_t parse_time_s( string_view fmt, string_view date_str ) {
        return _do_parse( fmt, date_str, []( i32 date, i32 time, i32 nanos ) {
            (void)nanos;
            return date * 86400ll + time;
        } );
    }

    nanos_t parse_time_ns( string_view fmt, string_view date_str ) {
        return _do_parse( fmt, date_str, []( i32 date, i32 time, i32 nanos ) {
            return ( date * 86400ll + time ) * 1000000000ll + nanos;
        } );
    }
} // namespace vtz


namespace {
    using namespace vtz;

    namespace {
        struct parse_fail {
            char const* p;
            char const* msg;
        };
    } // namespace


    bool is_whitespace( char ch ) {
        return ch == ' '      //
               || ch == '\t'  //
               || ch == '\n'  //
               || ch == '\r'  //
               || ch == '\xb' //
               || ch == '\xc';
    }

    template<class Int>
    VTZ_INLINE bool parse_digit_to( char ch, Int& dest ) noexcept {
        int  x       = ch - '0';
        bool good    = size_t( x ) < 10;
        Int  new_val = dest * 10 + x;
        if( good ) { dest = new_val; }
        return good;
    }

    VTZ_INLINE int parse_year( char const*& p, char const* end ) {
        int result = 0;

        if( p != end && parse_digit_to( *p, result ) )
            ++p;
        else
            throw parse_fail{ p, "Expected digit" };
        if( p != end && parse_digit_to( *p, result ) ) ++p;
        if( p != end && parse_digit_to( *p, result ) ) ++p;
        if( p != end && parse_digit_to( *p, result ) ) ++p;
        return result;
    }


    VTZ_INLINE int parse_d2( char const*& p, char const* end ) {
        int result = 0;

        if( p != end && parse_digit_to( *p, result ) )
            ++p;
        else
            throw parse_fail{ p, "Expected digit" };
        if( p != end && parse_digit_to( *p, result ) ) ++p;
        return result;
    }

    VTZ_INLINE int parse_d3( char const*& p, char const* end ) {
        int result = 0;

        if( p != end && parse_digit_to( *p, result ) )
            ++p;
        else
            throw parse_fail{ p, "Expected digit" };
        if( p != end && parse_digit_to( *p, result ) ) ++p;
        if( p != end && parse_digit_to( *p, result ) ) ++p;
        return result;
    }

    i32 parse_frac_as_nanos( char const*& p, char const* end ) noexcept {
        // If there are fewer than 2 characters left, or there's no decimal
        // point, there's nothing to parse. return 0
        if( end - p < 2 || *p != '.' ) return 0;

        i32 frac = 0;
        if( !parse_digit_to( p[1], frac ) )
        {
            // If we couldn't parse the digit, don't consume the '.', just
            // return 0
            return 0;
        }
        // We've consumed 2 characters
        p += 2;
        // we have 1 digit now.
        if( p == end || !parse_digit_to( *p, frac ) ) return frac * 100000000;
        ++p; // we have 2 digits now
        if( p == end || !parse_digit_to( *p, frac ) ) return frac * 10000000;
        ++p; // we have 3 digits now
        if( p == end || !parse_digit_to( *p, frac ) ) return frac * 1000000;
        ++p; // we have 4 digits now
        if( p == end || !parse_digit_to( *p, frac ) ) return frac * 100000;
        ++p; // we have 5 digits now
        if( p == end || !parse_digit_to( *p, frac ) ) return frac * 10000;
        ++p; // we have 6 digits now
        if( p == end || !parse_digit_to( *p, frac ) ) return frac * 1000;
        ++p; // we have 7 digits now
        if( p == end || !parse_digit_to( *p, frac ) ) return frac * 100;
        ++p; // we have 8 digits now
        if( p == end || !parse_digit_to( *p, frac ) ) return frac * 10;
        ++p; // we have 9 digits now; stop parsing
        return frac;
    }

    template<class F>
    auto with_cstr( string_view sv, F&& func ) {
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

    sysdays_t date_from_tm( std::tm const& t ) noexcept {
        int year = 1900 + t.tm_year;
        if( t.tm_mday || t.tm_mon )
        {
            int mday = t.tm_mday;
            // If the mday wasn't specified, assume the 1st
            if( mday == 0 ) mday = 1;
            return resolve_civil( year, t.tm_mon + 1, mday );
        }
        else
        {
            // Use the day of the year. if the day of the year is 0, this
            // will just give us the first of the year.
            return resolve_civil_ordinal( t.tm_year, t.tm_yday + 1 );
        }
    }

    /// Get the time intraday in seconds
    i32 time_from_tm( std::tm const& t ) noexcept {
        return i32( t.tm_hour ) * 3600 + t.tm_min * 60 + t.tm_sec;
    }
} // namespace


template<class F>
auto vtz::_do_parse( string_view format, string_view input, F func )
    -> decltype( func( i32(), i32(), i32() ) ) {
    if( format.size() == 0 )
    {
        // Format string is empty. So: date is epoch date, time of day is 0,
        // nanos is 0
        return func( 0, 0, 0 );
    }

    char const* f = format.data();
    char const* p = input.data();

    // We want to be able to consume an additional character when
    // parsing the format string
    size_t len = format.size() - 1;

    char const* p_end  = p + input.size();
    char const* f_back = f + len;

    bool has_c       = false;
    bool has_small_y = false;

    i32 year  = 1970;
    int month = 1;
    int dom   = 1;
    int doy   = 0;
    int hr    = 0;
    int mi    = 0;
    int se    = 0;
    i32 nanos = 0;

    try
    {
        while( f < f_back && p < p_end )
        {
            char c = *f++;
            if( c != '%' )
            {
                if( c == *p++ ) continue;
                // Mismatched character
                throw parse_fail{ p - 1,
                    "Character doesn't match input format string" };
            }

            // We've encountered a format specifier, so we consume an
            // additional character
            char next = *f++;
            switch( next )
            {
            // LITERAL SPECIFIERS

            // Expect literal '%'
            case '%':
                if( '%' == *p++ ) continue;
                throw parse_fail{ p - 1, "Expected literal '%'" };
            // Matches any whitespace
            case 'n':
            case 't':
                while( p < p_end && is_whitespace( *p ) ) ++p;
                continue;

            // PARSE YEAR

            // Expect Year
            case 'Y': year = ::parse_year( p, p_end ); continue;
            case 'y':
                {
                    has_small_y = true;
                    auto y      = parse_d2( p, p_end );
                    // If we have the first two digits, just add the last
                    // two
                    if( has_c ) { year += y; }
                    else
                    {
                        // Range [69,99] results in values 1969 to 1999,
                        // range [00,68] results in 2000-2068
                        year = y + ( y < 69 ? 2000 : 1900 );
                    }
                    continue;
                }
            // parses the first 2 digits of year as a decimal number (range
            // [00,99])
            case 'C':
                {
                    has_c = true;
                    if( has_small_y )
                    {
                        // if '%y' was parsed, treat as number 0-99
                        if( year < 2000 ) { year -= 1900; }
                        else { year -= 2000; }
                        // add in two digits for '%C'
                        year += parse_d2( p, p_end ) * 100;
                    }
                    else
                    {
                        // Overwrite the year with the correct two digits
                        year = parse_d2( p, p_end ) * 100;
                    }

                    continue;
                }

            // MONTH

            // parse month as decimal number (range [01,12]), leading 0s
            // permitted but not required
            case 'm': month = parse_d2( p, p_end ); continue;

            // DAY OF THE YEAR/MONTH

            // parses the day of the year as a decimal number (range
            // [001,366]), leading 0s permitted but not required
            case 'j': doy = parse_d3( p, p_end ); continue;
            // parses the day of the month as a decimal number (range
            // [01,31]), leading 0s permitted but not required
            case 'e': // (synonym of 'd')
            case 'd': dom = parse_d2( p, p_end ); continue;

            // DAY OF THE WEEK

            // weekday 0-6, sunday is 0
            case 'w':
                {
                    if( p < p_end )
                    {
                        int weekday = *p++ - '0';
                        if( 0 <= weekday && weekday < 7 ) continue;
                    }
                    throw parse_fail{ p - 1, "Expected weekday (range [0-6])" };
                }
            // weekday 1-7, monday is 1
            case 'u':
                {
                    if( p < p_end )
                    {
                        int weekday = *p++ - '1';
                        if( 0 <= weekday && weekday < 7 ) continue;
                    }
                    throw parse_fail{ p - 1, "Expected weekday (range [1-7])" };
                }

            // HOUR, MINUTE, SECOND

            // parses the hour as a decimal number, 24 hour clock (range
            // [00-23]), leading 0s permitted but not required
            case 'H': hr = parse_d2( p, p_end ); continue;
            // minute
            case 'M': mi = parse_d2( p, p_end ); continue;
            // second
            case 'S':
                {
                    se    = parse_d2( p, p_end );
                    nanos = parse_frac_as_nanos( p, p_end );
                    continue;
                }
            // %H:%M
            case 'R':
                {
                    hr = parse_d2( p, p_end );
                    if( p == p_end || *p != ':' )
                        throw parse_fail{ p, "Expected ':'" };
                    ++p;
                    mi = parse_d2( p, p_end );
                    continue;
                }
            // '%F' is equivalent to %Y-%m-%d
            case 'F':
                {
                    year = ::parse_year( p, p_end );
                    if( p == p_end || *p != '-' )
                        throw parse_fail{ p, "Expected '-'" };
                    ++p;
                    month = parse_d2( p, p_end );
                    if( p == p_end || *p != '-' )
                        throw parse_fail{ p, "Expected '-'" };
                    ++p;
                    dom = parse_d2( p, p_end );
                    continue;
                }
            // %H:%M:%S
            case 'T':
                {
                    hr = parse_d2( p, p_end );
                    if( p == p_end || *p != ':' )
                        throw parse_fail{ p, "Expected ':'" };
                    ++p;
                    mi = parse_d2( p, p_end );
                    if( p == p_end || *p != ':' )
                        throw parse_fail{ p, "Expected ':'" };
                    ++p;
                    se    = parse_d2( p, p_end );
                    nanos = parse_frac_as_nanos( p, p_end );
                    continue;
                }
            // Parses the time zone abbreviation or name, taken as the
            // longest sequence of characters that only contains the
            // characters A through Z, a through z, 0 through 9, -, +, _,
            // and /.
            case 'Z':
                {
                    while( p != p_end )
                    {
                        char c        = *p;
                        bool is_zchar = 'A' <= c && c <= 'Z'    //
                                        || 'a' <= c && c <= 'z' //
                                        || '0' <= c && c <= '9' //
                                        || c == '-'             //
                                        || c == '+'             //
                                        || c == '_'             //
                                        || c == '/';
                        if( !is_zchar ) break;
                        ++p;
                    }
                    continue;
                }

            default:
                {
                    return with_cstr( format, [&]( char const* format_cstr ) {
                        auto ss = std::istringstream( std::string( input ) );

                        std::tm tm{};
                        ss >> std::get_time( &tm, format_cstr );

                        if( ss.fail() )
                        {
                            throw std::runtime_error(
                                util::join( "parse_time(): Unable to parse ",
                                    escape_string( input ),
                                    " as ",
                                    escape_string( format_cstr ),
                                    ". NOTE: fallback to standard library "
                                    "required because of '%",
                                    next,
                                    "' specifier" ) );
                        }

                        return func(
                            date_from_tm( tm ), time_from_tm( tm ), 0 );
                    } );
                }
            }
        }

        if( f <= f_back )
        {
            // We reached the end of the input before the full format string
            // was consumed
            if( f < f_back )
                throw parse_fail{ p,
                    "End of input reached before format string was consumed" };
            // check last literal character
            if( *f != *p )
                throw parse_fail{ p,
                    "Character doesn't match input format string" };
            ++p;
        }

        sysdays_t date = bool( doy ) // Check if we have an ordinal date
                                     // use ordinal date
                             ? date = resolve_civil_ordinal( year, doy )
                             // use (year, month, day)
                             : date = resolve_civil( year, month, dom );

        i32 time_of_day = i32( hr ) * 3600 + i32( mi ) * 60 + i32( se );

        return func( date, time_of_day, nanos );
    }
    catch( parse_fail const& p )
    {
        auto pos = p.p - input.data();
        if( size_t( pos ) < input.size() )
        {
            char ch = *p.p;
            throw std::runtime_error( util::join( "Error @ input[",
                pos,
                "] (char='",
                ch,
                "'). Unable to parse ",
                escape_string( input ),
                " with format=",
                escape_string( format ),
                ": ",
                p.msg ) );
        }
        else
        {
            throw std::runtime_error(
                util::join( "Error: unexpected end of input. Unable to parse ",
                    escape_string( input ),
                    " with format=",
                    escape_string( format ),
                    ": ",
                    p.msg ) );
        }
    }
}
