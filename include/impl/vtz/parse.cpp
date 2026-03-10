

#include <cstddef>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vtz/civil.h>
#include <vtz/impl/bit.h>
#include <vtz/impl/macros.h>
#include <vtz/impl/math.h>
#include <vtz/strings.h>
#include <vtz/util/microfmt.h>

#include <vtz/parse.h>

using vtz::i32;
using vtz::i64;
using vtz::u32;

namespace {
    /// Holds a zone offset (%z).
    ///
    /// INTENT:
    ///
    /// We are going to add `tz->parse` in the future.
    ///
    /// Let's suppose someone calls `tz->parse( "%F %T", ... )`. This will:
    ///
    /// 1. Parse `%F %T` to obtain some number of local seconds
    /// 2. Convert local seconds to sys seconds
    /// 3. Return the number of sys seconds.
    ///
    /// But consider this - what if the format is '%F %T %z'? In this case,
    /// the UTC offset is known, so after parsing we can compute the number
    /// of sys_seconds by subtracting the UTC offset, and we don't need to
    /// do a timezone conversion.
    ///
    /// In that case, we should instead do:
    ///
    /// 1. Parse `%F %T` to obtain some number of local seconds
    /// 2. Subtract the UTC offset
    /// 3. Return the number of sys seconds.
    ///
    /// And the correct thing to do is to _bypass the timezone logic, because
    /// the offset is specified in the timestamp_.
    ///
    /// This is why we need to be able to check `has_value()`.

    class opt_z {
        u32 r_{};

      public:

        opt_z() = default;
        constexpr opt_z( i32 offset ) noexcept
        : r_( ( u32( offset ) << 1 ) | 1u ) {}

        constexpr bool has_value() const noexcept { return bool( r_ & 1u ); }
        constexpr explicit operator bool() const noexcept { return r_ & 1u; }

        /// Returns the value, or 0 if no value is held. Implementing 'z' this
        /// way allows us to avoid a branch if we don't need to be able to check
        /// for %z
        constexpr i32 value() const noexcept { return i32( r_ ) >> 1; }
    };

    static_assert( !opt_z().has_value() );
    // Default-constructed has value of 0 (this is always safe)
    static_assert( opt_z().value() == 0 );
    static_assert( opt_z( 0 ).has_value() );
    static_assert( opt_z( 0 ).value() == 0 );
    static_assert( opt_z( -10 ).value() == -10 );
    static_assert( opt_z( 10 ).value() == 10 );

    /// Holds two functions to obtain the parse result.
    /// One for the (date, time of day) case, and one for the timestamp case.
    ///
    /// This allows us to avoid splitting the timestamp into constituent parts,
    /// only to recombine them.

    template<class F_Date, class F_Seconds>
    struct _finish_parse
    : F_Date
    , F_Seconds {
        using F_Date::operator();
        using F_Seconds::operator();

        constexpr static bool is_validate = false;
    };
    template<class F1, class F2>
    _finish_parse( F1, F2 ) -> _finish_parse<F1, F2>;


    /// Used for doing validation. This is primarily useful for when we've
    /// already obtained the result from a format specifier representing a
    /// source-of-truth (aka, '%s'), but we want to check the rest of the
    /// string.
    ///
    /// (really ugly but oh well)

    template<class T>
    struct parse_validate {
        T           result;
        char const* f;
        char const* p;

        T operator()( i32, i32, i32, opt_z ) { return result; }
        T operator()( i64, i32 ) { return result; }

        constexpr static bool is_validate = true;
    };
} // namespace


namespace vtz {
    template<class F>
    auto _do_parse( string_view format, string_view input, F func )
        -> decltype( func( i32(), i32(), i32(), opt_z() ) );

    /// Consume the input, run parsing logic, but discard anything computed.
    /// Just return the result.
    ///
    /// Params `f` and `p` are where parsing left off.

    template<class T>
    static T _parse_validate( string_view format,
        string_view                       date_str,
        T                                 result,
        char const*                       f,
        char const*                       p ) {
        return _do_parse( format,
            date_str,
            parse_validate<T>{
                static_cast<T&&>( result ),
                f,
                p,
            } );
    }

    sys_days_t parse_d( string_view fmt, string_view date_str ) {
        return _do_parse( fmt,
            date_str,
            _finish_parse{
                []( i32 date, i32 time, i32 nanos, opt_z z ) {
                    (void)nanos;
                    return date + math::div_floor<86400>( time - z.value() );
                },
                []( i64 sec, i32 nanos ) {
                    (void)nanos;
                    return sys_days_t( math::div_floor<86400>( sec ) );
                },
            } );
    }

    sec_t parse_s( string_view fmt, string_view date_str ) {
        return _do_parse( fmt,
            date_str,
            _finish_parse{
                []( i32 date, i32 time, i32 nanos, opt_z z ) {
                    (void)nanos;
                    return date * 86400ll + time - z.value();
                },
                []( i64 sec, i32 ) { return sec; },
            } );
    }

    nanos_t parse_ns( string_view fmt, string_view date_str ) {
        return _do_parse( fmt,
            date_str,
            _finish_parse{
                []( i32 date, i32 time, i32 nanos, opt_z z ) {
                    return ( date * 86400ll + time - z.value() ) * 1000000000ll
                           + nanos;
                },
                []( i64 sec, i32 nanos ) { return sec * 1000000000ll + nanos; },
            } );
    }

    parse_precise_result parse_precise(
        string_view fmt, string_view time_str ) {
        return _do_parse( fmt,
            time_str,
            _finish_parse{
                []( i32 date, i32 time, u32 nanos, opt_z z ) {
                    return parse_precise_result{
                        date * 86400ll + time - z.value(),
                        nanos,
                    };
                },
                []( i64 sec, i32 nanos ) {
                    return parse_precise_result{ sec, u32( nanos ) };
                },
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


    // Attempts to consume the given suffix in a case-insensitive way.
    //
    // On success, updates p to point past the consumed suffix.
    //
    // suffix must be lowercase.
    template<size_t N>
    VTZ_INLINE int _consume_suffix( char const*& p,
        char const*                              end,
        int                                      result,
        char const ( &suffix )[N] ) noexcept {
        constexpr intptr_t suffix_len = intptr_t( N ) - 1;

        if( ( end - p ) < suffix_len )
        {
            // Cannot consume suffix; just return
            return result;
        }

        for( size_t i = 0; i < suffix_len; ++i )
        {
            // mismatch - don't consume suffix, just return
            if( u8( u8( p[i] ) | 32u ) != u8( suffix[i] ) ) return result;
        }
        // We can consume suffix and return
        p += suffix_len;
        return result;
    }


    /// Parse a month name as per the C locale. Accepts either an abbreviated
    /// month name, or a full month name.
    ///
    /// The match is done in a case-insensitive manner. Eg, jan, JAN, and Jan
    /// are all treated the same.
    ///
    /// 'Feb' -> 3 characters are consumed, return month=2
    /// 'February' -> 8 characters are consumed, return month=2
    ///
    /// If a month name is incomplete, only the abbreviation will be consumed.
    /// Eg:
    ///
    /// 'Febru' -> 3 characters are consumed, result is month=2. p points to
    /// 'ru'

    VTZ_INLINE int parse_month_name( char const*& p, char const* end ) {
        auto rem = end - p;
        using _impl::_load3;
        using _impl::_load4;

        if( rem >= 3 ) VTZ_LIKELY
        {
            // We use `| 32u` to convert to lowercase here
            // This changes non-letter characters, but that's okay, because
            // a non-letter will never be changed into a letter.
            //
            // So, when we construct the key into the switch, anything that
            // is not a valid month will not be matched.
            u32 p0 = u8( p[0] ) | 32u;
            u32 p1 = u8( p[1] ) | 32u;
            u32 p2 = u8( p[2] ) | 32u;

            // this is used to determine which month we're on, based on the
            // abbreviation
            u32 key = p0 | ( p1 << 8 ) | ( p2 << 16 );

            // Handle month abbreviations
            p += 3; // Optimistically update p
            switch( key )
            {
            // january -> "jan", "uary"
            case _load3( "jan" ): return _consume_suffix( p, end, 1, "uary" );
            // february -> "feb", "ruary"
            case _load3( "feb" ): return _consume_suffix( p, end, 2, "ruary" );
            // march -> "mar", "ch"
            case _load3( "mar" ): return _consume_suffix( p, end, 3, "ch" );
            // april -> "apr", "il"
            case _load3( "apr" ): return _consume_suffix( p, end, 4, "il" );
            // may (no suffix for may)
            case _load3( "may" ): return 5;
            // june -> "jun", "e"
            case _load3( "jun" ): return _consume_suffix( p, end, 6, "e" );
            // july -> "jul", "y"
            case _load3( "jul" ): return _consume_suffix( p, end, 7, "y" );
            // august -> "aug", "ust"
            case _load3( "aug" ): return _consume_suffix( p, end, 8, "ust" );
            // september -> "sep", "tember"
            case _load3( "sep" ): return _consume_suffix( p, end, 9, "tember" );
            // october -> "oct", "ober"
            case _load3( "oct" ): return _consume_suffix( p, end, 10, "ober" );
            // november -> "nov", "ember"
            case _load3( "nov" ): return _consume_suffix( p, end, 11, "ember" );
            // december -> "dec", "ember"
            case _load3( "dec" ): return _consume_suffix( p, end, 12, "ember" );
            }
            // Whoops! We failed - set p back
            p -= 3;
        }

        throw parse_fail{ p,
            "Expected C Locale Month abbreviation (Jan, Feb, Mar, ...)" };
    }

    /// Parse a weekday name as per the C locale. Accepts either an abbreviated
    /// weekday name, or a full weekday name.
    ///
    /// The match is done in a case-insensitive manner.
    ///
    /// 'Mon' -> 3 characters are consumed, return 1
    /// 'Monday' -> 6 characters are consumed, return 1
    ///
    /// Returns the weekday as a number in the range [0-6], where Sunday is 0.
    ///
    /// See `parse_month_name` for more exposition on how parsing works.

    VTZ_INLINE int parse_weekday_name( char const*& p, char const* end ) {
        auto rem = end - p;
        using _impl::_load3;

        if( rem >= 3 ) VTZ_LIKELY
        {
            u32 p0 = u8( p[0] ) | 32u;
            u32 p1 = u8( p[1] ) | 32u;
            u32 p2 = u8( p[2] ) | 32u;

            u32 key = p0 | ( p1 << 8 ) | ( p2 << 16 );

            p += 3;
            switch( key )
            {
            // sunday -> "sun", "day"
            case _load3( "sun" ): return _consume_suffix( p, end, 0, "day" );
            // monday -> "mon", "day"
            case _load3( "mon" ): return _consume_suffix( p, end, 1, "day" );
            // tuesday -> "tue", "sday"
            case _load3( "tue" ): return _consume_suffix( p, end, 2, "sday" );
            // wednesday -> "wed", "nesday"
            case _load3( "wed" ): return _consume_suffix( p, end, 3, "nesday" );
            // thursday -> "thu", "rsday"
            case _load3( "thu" ): return _consume_suffix( p, end, 4, "rsday" );
            // friday -> "fri", "day"
            case _load3( "fri" ): return _consume_suffix( p, end, 5, "day" );
            // saturday -> "sat", "urday"
            case _load3( "sat" ): return _consume_suffix( p, end, 6, "urday" );
            }
            p -= 3;
        }

        throw parse_fail{ p, "Expected C Locale Weekday (Sun, Mon, Tue, ...)" };
    }


    /// Parse an AM/PM designator in a case-insensitive manner.
    /// Returns 0 for AM, 12 for PM (to be added to the hour).

    VTZ_INLINE int parse_am_pm( char const*& p, char const* end ) {
        if( end - p >= 2 )
        {
            u32 c0  = u8( p[0] ) | 32u;
            u32 c1  = u8( p[1] ) | 32u;
            u32 key = c0 | ( c1 << 8 );

            p += 2; // Optimistically update 'p'

            switch( key )
            {
            case _impl::_load2( "am" ): return 0;
            case _impl::_load2( "pm" ): return 12;
            }

            p -= 2;
        }

        throw parse_fail{ p, "Expected AM or PM" };
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


    /// Parse a %z specifier of the form `[+|-]hh[mm]`
    VTZ_INLINE i32 parse_z( char const*& p, char const* end ) {
        auto rem = end - p;

        if( rem >= 5 )
        {
            // p[0] should be '+' or '-'. So you can compute the sign by doing
            // ',' - p[0] see: https://www.asciitable.com/
            i32 sign = ',' - p[0];
            i32 h0   = p[1] - '0';
            i32 h1   = p[2] - '0';
            i32 m0   = p[3] - '0';
            i32 m1   = p[4] - '0';

            bool hh_good = size_t( h0 ) < 10 && size_t( h1 ) < 10;
            // [00, 59] permitted for minute
            bool mm_good   = size_t( m0 ) < 6 && size_t( m1 ) < 10;
            bool sign_good = sign * sign == 1;

            i32 hh = ( 10 * h0 + h1 ) * 3600;
            i32 mm = ( 10 * m0 + m1 ) * 60;

            if( sign_good && hh_good )
            {
                if( mm_good )
                {
                    p += 5;
                    return sign * ( hh + mm );
                }

                p += 3;
                return sign * hh;
            }
        }
        else if( rem >= 3 )
        {
            i32 sign = ',' - p[0];
            i32 h0   = p[1] - '0';
            i32 h1   = p[2] - '0';

            bool hh_good   = size_t( h0 ) < 10 && size_t( h1 ) < 10;
            bool sign_good = sign * sign == 1;

            i32 hh = ( 10 * h0 + h1 ) * 3600;

            if( sign_good && hh_good )
            {
                p += 3;
                return sign * hh;
            }
        }

        throw parse_fail{ p, "Expected tz offset of form [+|-]hh[mm]" };
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


    // parse 2 digits, permitting a leading space in the event of a single digit
    VTZ_INLINE int parse_d2_allow_space( char const*& p, char const* end ) {
        int result = 0;

        if( p != end && parse_digit_to( *p, result ) ) VTZ_LIKELY
        {
            // Happy path - we have 1 or 2 digits, which we can parse into the
            // result
            ++p;
            if( p != end && parse_digit_to( *p, result ) ) ++p;
            return result;
        }
        else
        {
            // Sad path - check
            if( p != end )
            {
                // Consume leading space
                if( *p != ' ' )
                    throw parse_fail{ p, "Expected digit or leading space" };
                ++p;

                // Parse the digit
                if( p != end && parse_digit_to( *p, result ) )
                {
                    ++p;
                    return result;
                }
                // otherwise, fall through to error...
            }
            throw parse_fail{ p, "Expected digit" };
        }
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

    sys_days_t date_from_tm( std::tm const& t ) noexcept {
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
    -> decltype( func( i32(), i32(), i32(), opt_z() ) ) {
    if( format.size() == 0 )
    {
        // Format string is empty. So: date is epoch date, time of day is 0,
        // nanos is 0, timezone is not specified
        return func( 0, 0, 0, opt_z() );
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

    // This holds the timezone offset (if present)
    // if no %z flag is specified, z.has_value() will be false
    auto z = opt_z();

    // If we're running the validator, we want to skip into the middle of the
    // input, to finish validation. the reason for this is that error messages
    // should still be reported relative to `format` and `input`, so those
    // shouldn't be changed when passed to the validator
    if constexpr( F::is_validate )
    {
        f = func.f;
        p = func.p;
    }


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
            // PARSE LITERAL SPECIFIERS

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
            parse_year_within_century:
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


            // PARSE MONTH

            // parse month as decimal number (range [01,12]), leading 0s
            // permitted but not required
            case 'm': month = parse_d2_allow_space( p, p_end ); continue;

            // parse a month, either the abbreviated name ('Jan') or the full
            // name ('January'). Uses C locale.
            case 'b':
            case 'B':
            case 'h': month = parse_month_name( p, p_end ); continue;


            // PARSE DAY OF THE YEAR/MONTH

            // parses the day of the year as a decimal number (range
            // [001,366]), leading 0s permitted but not required
            case 'j': doy = parse_d3( p, p_end ); continue;
            // parses the day of the month as a decimal number (range
            // [01,31]), leading 0s permitted but not required
            case 'e': // (synonym of 'd')
            case 'd': dom = parse_d2_allow_space( p, p_end ); continue;

            // DAY OF THE WEEK

            // weekday name (abbreviated or full), consumed but not used
            case 'a':
            case 'A': (void)parse_weekday_name( p, p_end ); continue;

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
                if( p < p_end )
                {
                    int weekday = *p++ - '1';
                    if( 0 <= weekday && weekday < 7 ) continue;
                }
                throw parse_fail{ p - 1, "Expected weekday (range [1-7])" };


            // HOUR, MINUTE, SECOND

            // parses the hour as a decimal number, 24 hour clock (range
            // [00-23]), leading 0s permitted but not required
            case 'k': // Because of GNU extensions, '%k' is accepted as a
                      // synonym of %H
            case 'H': hr = parse_d2_allow_space( p, p_end ); continue;

            // parses the hour as a decimal number, 12 hour clock (range
            // [01-12]). Adds (value % 12) so that combined with %p it
            // produces the correct 24-hour value.
            case 'l': // Because of GNU extensions, '%l' is accepted as a
                      // synonym of %I
            case 'I': hr += parse_d2_allow_space( p, p_end ) % 12; continue;
            // parses AM/PM designator (case-insensitive). Adds 0 for AM,
            // 12 for PM.
            case 'P': // Because of GNU extensions, '%P' is accepted as a
                      // synonym of '%p'
            case 'p': hr += parse_am_pm( p, p_end ); continue;
            // minute
            case 'M': mi = parse_d2( p, p_end ); continue;
            // second
            case 'S':
                {
                    se    = parse_d2( p, p_end );
                    nanos = parse_frac_as_nanos( p, p_end );
                    continue;
                }


            // PARSE TIMEZONE DESIGNATION/OFFSET

            // Parses the time zone abbreviation or name, taken as the
            // longest sequence of characters that only contains the
            // characters A through Z, a through z, 0 through 9, -, +, _,
            // and /.
            case 'Z':
                {
                    while( p != p_end )
                    {
                        char c        = *p;
                        bool is_zchar = ( 'A' <= c && c <= 'Z' )    //
                                        || ( 'a' <= c && c <= 'z' ) //
                                        || ( '0' <= c && c <= '9' ) //
                                        || c == '-'                 //
                                        || c == '+'                 //
                                        || c == '_'                 //
                                        || c == '/';
                        if( !is_zchar ) break;
                        ++p;
                    }
                    continue;
                }
            // Parses offset from UTC in the format `[+|-]hh[mm]`
            case 'z':
                z = parse_z( p, p_end );
                continue;


                // PARSE SECONDS SINCE THE UNIX EPOCH

            // reads seconds since the unix epoch. If a decimal point appears
            // after '%s', parses as nanoseconds.
            case 's':
                {
                    int64_t sec{};
                    auto    result = std::from_chars( p, p_end, sec );
                    if( result.ec != std::errc{} )
                    {
                        throw parse_fail{ p,
                            "Expected seconds since the unix epoch" };
                    }
                    p = result.ptr;

                    nanos = parse_frac_as_nanos( p, p_end );


                    if( f == format.data() + format.size() )
                    { return func( sec, nanos ); }

                    // Check the remainder of the string, but return whatever
                    // was discovered from %s
                    return _parse_validate(
                        format, input, func( sec, nanos ), f, p );
                }


            // PARSE COMPOUND SPECIFIERS

            // equivalent to %H:%M
            case 'R':
                {
                    hr = parse_d2_allow_space( p, p_end );
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
                    month = parse_d2_allow_space( p, p_end );
                    if( p == p_end || *p != '-' )
                        throw parse_fail{ p, "Expected '-'" };
                    ++p;
                    dom = parse_d2_allow_space( p, p_end );
                    continue;
                }

            // preferred time representation for the current locale
            // For the C locale this is '%T'
            // Test with: env LC_ALL=C date '+%x %X'
            case 'X':
            // %H:%M:%S
            case 'T':
                {
                    hr = parse_d2_allow_space( p, p_end );
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
            // equivalent to %I:%M:%S %p (12-hour clock with AM/PM)
            case 'r':
                {
                    hr += parse_d2_allow_space( p, p_end ) % 12;
                    if( p == p_end || *p != ':' )
                        throw parse_fail{ p, "Expected ':'" };
                    ++p;
                    mi = parse_d2( p, p_end );
                    if( p == p_end || *p != ':' )
                        throw parse_fail{ p, "Expected ':'" };
                    ++p;
                    se    = parse_d2( p, p_end );
                    nanos = parse_frac_as_nanos( p, p_end );
                    if( p == p_end || *p != ' ' )
                        throw parse_fail{ p, "Expected ' '" };
                    ++p;
                    hr += parse_am_pm( p, p_end );
                    continue;
                }

            // preferred date and time representation for the current locale
            // For the C locale this is '%a %b %e %H:%M:%S %Y'
            // Test with: env LC_ALL=C date '+%c'
            case 'c':
                {
                    (void)parse_weekday_name( p, p_end );
                    if( p == p_end || *p != ' ' )
                        throw parse_fail{ p, "Expected ' '" };
                    ++p;
                    month = parse_month_name( p, p_end );
                    if( p == p_end || *p != ' ' )
                        throw parse_fail{ p, "Expected ' '" };
                    ++p;
                    dom = parse_d2_allow_space( p, p_end );
                    if( p == p_end || *p != ' ' )
                        throw parse_fail{ p, "Expected ' '" };
                    ++p;
                    hr = parse_d2_allow_space( p, p_end );
                    if( p == p_end || *p != ':' )
                        throw parse_fail{ p, "Expected ':'" };
                    ++p;
                    mi = parse_d2( p, p_end );
                    if( p == p_end || *p != ':' )
                        throw parse_fail{ p, "Expected ':'" };
                    ++p;
                    se    = parse_d2( p, p_end );
                    nanos = parse_frac_as_nanos( p, p_end );
                    if( p == p_end || *p != ' ' )
                        throw parse_fail{ p, "Expected ' '" };
                    ++p;
                    year = ::parse_year( p, p_end );
                    continue;
                }

            // preferred date representation for the current locale
            // For the C locale, this is '%m/%d/%y' (???)
            // Test with: env LC_ALL=C date '+%x %X'
            case 'x':
            // equivalent to '%m/%d/%y' (American Date Format) (Evil-coded)
            case 'D':
                {
                    month = parse_d2_allow_space( p, p_end );
                    if( p == p_end || *p != '/' )
                        throw parse_fail{ p, "Expected '/'" };
                    ++p;
                    dom = parse_d2_allow_space( p, p_end );
                    if( p == p_end || *p != '/' )
                        throw parse_fail{ p, "Expected '/'" };
                    ++p;
                    // NOTE: parsing of %y touches some state in the function.
                    // It is easiest to just go to this block directly
                    goto parse_year_within_century;
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

                        return func( date_from_tm( tm ),
                            time_from_tm( tm ),
                            0,
                            opt_z() );
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

        sys_days_t date = bool( doy ) // Check if we have an ordinal date
                                      // use ordinal date
                              ? resolve_civil_ordinal( year, doy )
                              // use (year, month, day)
                              : resolve_civil( year, month, dom );

        i32 time_of_day = i32( hr ) * 3600 + i32( mi ) * 60 + i32( se );

        return func( date, time_of_day, nanos, z );
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
