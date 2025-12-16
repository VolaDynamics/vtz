#include <cerrno>
#include <fmt/base.h>
#include <limits>
#include <string_view>
#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/files.h>
#include <vtz/span.h>
#include <vtz/strings.h>
#include <vtz/tz_cache.h>
#include <vtz/tz_reader.h>
#include <vtz/tz_reader/FromUTC.h>
#include <vtz/tz_reader/LeapTable.h>
#include <vtz/tz_reader/ZoneFormat.h>
#include <vtz/tz_reader/ZoneRule.h>
#include <vtz/tzfile.h>

#include <algorithm>
#include <charconv>
#include <cstdlib>

#include <fmt/format.h>
#include <stdexcept>

template<>
struct ankerl::unordered_dense::hash<vtz::ZoneAbbr> {
    using is_avalanching = void;

    [[nodiscard]] auto operator()( vtz::ZoneAbbr const& x ) const noexcept
        -> uint64_t {
        static_assert(
            std::has_unique_object_representations_v<vtz::ZoneAbbr> );
        return detail::wyhash::hash( &x, sizeof( x ) );
    }
};

namespace vtz {
    namespace {
        /// Number of days in 400 years. Every 400 year period contains the same
        /// number of days, because leap years follow a 400-year cycle.
        constexpr i64 DAYS_400_YEARS = 365ll * 400 + 97;

        /// Number of seconds in 400 years. Every 400 year period contains the
        /// same number of seconds, because leap years follow a 400-year cycle,
        /// so each 400 year period is the same length.
        constexpr i64 SECS_400_YEARS = DAYS_400_YEARS * 86400ll;

        constexpr char const* no_token    = "no token was given";
        constexpr char const* empty_input = "input was empty";
        constexpr char const* bad_save
            = "is not a valid SAVE. SAVE must fits one of the following "
              "forms: "
              "[-]H, [-]HH, [-]H:MM, [-]HH:MM, [-]H:MM:SS, or [-]HH:MM:SS";
        constexpr char const* bad_stdoff
            = "is not a valid STDOFF. STDOFF must fits one of the following "
              "forms: "
              "[-]H, [-]HH, [-]H:MM, [-]HH:MM, [-]H:MM:SS, or [-]HH:MM:SS";
        constexpr char const* bad_at
            = "is not a valid AT token. AT must fits one of the following "
              "forms: "
              "[-]H[sguzw], [-]HH[sguzw], [-]H:MM[sguzw], [-]HH:MM[sguzw], "
              "[-]H:MM:SS[sguzw], or [-]HH:MM:SS[sguzw]";
        constexpr char const* exp_rule_on
            = "Expected rule describing when within the month "
              "a transition occurs";
        constexpr char const* bad_rule_on
            = "could not be parsed. Valid forms are: 'last[DOW]', "
              "'[DOW]>=[DOM]', '[DOW]<=[DOM]', or '[DOM]', where "
              "'[DOW]' represents "
              "Sun, Mon, Tue, Wed, Thu, Fri, or Sat, and DOM is an "
              "integer 1-31.";

        constexpr char const* bad_letter
            = "is too large to be valid for the LETTER/S field.";

        constexpr char const* bad_zone_rule
            = "is not a valid value for the 'RULES' column. A Zone Rule must "
              "be either a hyphen (\"-\"), indicating a null value; it must be "
              "an amount of time (eg, \"1:00\"), which we set our clocks ahead "
              "by; or it must be an alphabetic string which names a rule.";

        constexpr static char const* TZ_expected_rule
            = "Expected rules describing when zone transition occurs";
        constexpr static char const* end_of_string = "reached end of string";


        /// Return true if the character is an ascii letter (either
        /// lowercase or uppercase)
        constexpr auto is_alphabetic = []( char ch ) noexcept -> bool {
            return ( 'a' <= ch && ch <= 'z' )     // ch is lowercase
                   || ( 'A' <= ch && ch <= 'Z' ); // ch is uppercase;
        };

        /// Return true if the character is an ascii letter (either
        /// lowercase or uppercase) or an underscore
        constexpr auto is_alpha = []( char ch ) noexcept -> bool {
            return ( 'a' <= ch && ch <= 'z' )    // ch is lowercase
                   || ( 'A' <= ch && ch <= 'Z' ) // ch is uppercase
                   || ( ch == '_' );             // ch is a '_'
        };

        constexpr inline bool is_d10( int x ) noexcept {
            return 0 <= x && x < 10;
        }


        constexpr i32 _hms( i32 h, i32 m = 0, i32 s = 0 ) noexcept {
            return h * 3600 + m * 60 + s;
        }

        // Parse 4 digits
        TrivialOpt<i32> parse4( char const* src ) noexcept {
            i32 v0 = src[0] - '0';
            i32 v1 = src[1] - '0';
            i32 v2 = src[2] - '0';
            i32 v3 = src[3] - '0';
            return {
                v0 * 1000 + v1 * 100 + v2 * 10 + v3,
                is_d10( v0 ) && is_d10( v1 ) && is_d10( v2 ) && is_d10( v3 ),
            };
        }

        // Parse 1 or 2 digits. Anything else is bad.
        constexpr TrivialOpt<i32> parse1or2(
            char const* src, size_t size ) noexcept {
            if( size == 2 )
            {
                i32 v0 = src[0] - '0';
                i32 v1 = src[1] - '0';
                return { v0 * 10 + v1, is_d10( v0 ) && is_d10( v1 ) };
            }
            if( size == 1 )
            {
                i32 v0 = src[0] - '0';
                return { v0, is_d10( v0 ) };
            }
            return { 0, false };
        }

        u32 eat_u32_or_throw( char const*& p, char const* end ) {
            u32  x;
            auto result = std::from_chars( p, end, x );
            if( result.ec == std::errc{} )
            {
                p = result.ptr;
                return x;
            }
            throw ParseError{ "Expected unsigned int",
                "error occurred when parsing",
                OptTok( p, end - p ) };
        }
    } // namespace

    std::string ParseError::getErrorMessage(
        string_view input, string_view filename ) const {
        auto loc = Location::where_ptr( input, token.data() );

        if( token.has_value() )
        {
            return fmt::format( "Error @ {}:{}: {} but {} {}",
                filename,
                loc.str(),
                expected,
                escape_string( token ),
                but );
        }
        else
        {
            return fmt::format( "Error @ {}:{}: {} but {}",
                filename,
                loc.str(),
                expected,
                but );
        }
    }

    rule_year_t parse_year( OptTok tok ) {
        char const* begin = tok.data();
        size_t      size  = tok.size();
        if( size == 4 )
        {
            if( auto y = parse4( begin ) ) { return *y; }
        }
        else
        {
            char const* end = begin + size;

            i16  year{};
            auto result = std::from_chars( begin, end, year );
            if( result.ec == std::errc{} && result.ptr == end && year > 0 )
                return rule_year_t( year );
        }
        throw ParseError{
            "Expected year of form 'YYYY'",
            tok.has_value() ? "is not a valid year" : "year is missing",
            tok,
        };
    }

    rule_year_t parse_year_to( OptTok tok, rule_year_t from ) {
        if( tok == "ma" || tok == "max" ) { return Y_MAX; }
        if( tok == "o" || tok == "only" ) { return from; }
        char const* begin = tok.data();
        size_t      size  = tok.size();

        if( size == 4 )
        {
            if( auto y = parse4( begin ) ) { return *y; }
        }
        else
        {
            i16         year{};
            char const* end = begin + size;

            auto result = std::from_chars( begin, end, year );
            if( result.ec == std::errc{} && result.ptr == end && year > 0 )
                return rule_year_t( year );
        }

        throw ParseError{
            "Expected year of form 'YYYY' or literal strings 'max' or "
            "'only'",
            tok.has_value() ? "is not a valid year" : "year is missing",
            tok,
        };
    }

    Mon parse_month( OptTok tok ) {
        using _impl::_load1;
        using _impl::_load2;
        using _impl::_load3;
        using _impl::_parse_mon;

        if( auto m = _parse_mon( tok.data(), tok.size() ) ) return *m;

        throw ParseError{
            "Expected month",
            tok.has_value() ? "is not a valid month name" : "month is missing",
            tok,
        };
    }

    u8 parse_day_of_month( char const* tok, size_t size ) {
        if( size == 1 )
        {
            char   ch  = tok[0];
            size_t dig = size_t( ch - '0' );
            if( dig && dig <= 9 ) return u8( dig );
        }
        else if( size == 2 )
        {
            size_t d10 = size_t( tok[0] - '0' );
            size_t d1  = size_t( tok[1] - '0' );
            if( d10 <= 9 && d1 <= 9 )
            {
                size_t day = d10 * 10 + d1;
                if( day && day < 32 ) return u8( day );
            }
        }

        throw ParseError{
            "Expected day of the month",
            size ? "is not a valid day of the month (day must be 1-31)"
                 : no_token,
            OptTok( tok, size ),
        };
    }

    RuleOn parse_rule_on( OptTok tok ) {
        using namespace _impl;

        size_t      size = tok.size();
        char const* p    = tok.data();

        if( size <= 2 )
        {
            // For size<=2, it can only be a day
            return RuleOn::on( parse_day_of_month( p, size ) );
        }
        else if( size >= 4 )
        {
            constexpr auto _last = _load4( "last" );
            if( _load4( p ) == _last )
            {
                if( OptDOW dow = _parse_dow( p + 4, size - 4 ) )
                    return RuleOn::last( *dow );
            }
            else
            {
                size_t dow_len{};
                if( p[2] == '=' ) // Eg, M>=3 or M<=3
                    dow_len = 1;
                else if( p[3] == '=' )
                    dow_len = 2;
                else if( size > 4 && p[4] == '=' )
                    dow_len = 3;

                if( dow_len )
                {
                    // We know it must be of form
                    // \w{dow_len}[<>]=\d+
                    char   ge_or_le = p[dow_len];
                    OptDOW dow      = _parse_dow( p, dow_len );
                    auto   day      = parse_day_of_month(
                        p + ( dow_len + 2 ), size - ( dow_len + 2 ) );

                    if( dow )
                    {
                        switch( ge_or_le )
                        {
                        case '<': return RuleOn::le( *dow, day );
                        case '>': return RuleOn::ge( *dow, day );
                        }
                    }
                }
            }
        }

        throw ParseError{
            exp_rule_on, tok.has_value() ? bad_rule_on : no_token, tok
        };
    }

    i32 parse_hhmmss_offset( char const* p, size_t size, int sign ) noexcept {
        if( !size ) return OFFSET_NPOS;

        if( size <= 8 )
        {
            char buff[8]{};
            _vtz_memcpy( buff, p, size );
            int sep_pos[8];
            int num_sep = 0;
            // H:M [1]
            // H:M:S [1, 3]
            // H:MM:S [1, 4]
            // H:MM:SS [1, 4]
            // HH:M [2]
            // HH:M:S [2, 4]
            // HH:M:SS [2, 4]
            // HH:MM:S [2, 5]
            // HH:MM:SS [2, 5]
            if( buff[1] == ':' ) sep_pos[num_sep++] = 1;
            if( buff[2] == ':' ) sep_pos[num_sep++] = 2;
            if( buff[3] == ':' ) sep_pos[num_sep++] = 3;
            if( buff[4] == ':' ) sep_pos[num_sep++] = 4;
            if( buff[5] == ':' ) sep_pos[num_sep++] = 5;
            if( num_sep == 0 )
            {
                if( auto hh = parse1or2( buff, size ) )
                    return sign * _hms( *hh );
            }
            else if( num_sep == 1 )
            {
                int  p0 = sep_pos[0];
                auto hh = parse1or2( buff, p0 );
                auto mm = parse1or2( buff + p0 + 1, size - p0 - 1 );
                if( hh && mm ) return sign * _hms( *hh, *mm );
            }
            else if( num_sep == 2 )
            {
                int p0 = sep_pos[0];
                int p1 = sep_pos[1];
                // fmt::println( "input = {} sep={} pos={}, {}",
                //     string_view( p, size ),
                //     num_sep,
                //     p0,
                //     p1 );
                auto hh = parse1or2( buff, p0 );
                auto mm = parse1or2( buff + p0 + 1, p1 - p0 - 1 );
                auto ss = parse1or2( buff + p1 + 1, size - p1 - 1 );
                if( hh && mm && ss ) return sign * _hms( *hh, *mm, *ss );
            }
        }

        return OFFSET_NPOS;
    }

    i32 parse_signed_hhmmss_offset( char const* p, size_t size ) noexcept {
        if( size > 0 )
        {
            char ch = p[0];
            if( is_d10( ch - '0' ) ) return parse_hhmmss_offset( p, size, 1 );
            if( ch == '-' ) return parse_hhmmss_offset( p + 1, size - 1, -1 );
            if( ch == '+' ) return parse_hhmmss_offset( p + 1, size - 1, 1 );
        }
        return OFFSET_NPOS;
    }


    i32 eat_signed_hhmmss( char const*& p, char const* end ) {
        constexpr char const* expected = "Expected offset - [sign]HH[:MM][:SS]";
        if( p == end ) throw ParseError{ expected, empty_input };

        int  sign = 1;
        char ch   = p[0];

        // Consume and set the sign
        if( ch == '-' || ch == '+' )
        {
            ++p;
            if( ch == '-' ) sign = -1;
        }

        ptrdiff_t i    = 0;
        ptrdiff_t size = end - p;
        while( i < size && ( ( '0' <= p[i] && p[i] <= '9' ) || p[i] == ':' ) )
            ++i;

        i32 offset = parse_hhmmss_offset( p, i, sign );
        if( offset == OFFSET_NPOS )
            throw ParseError{
                expected, "was not an offset", OptTok( p, end - p )
            };
        p += i;
        return offset;
    }

    namespace {

        RuleSave parse_rule_save( OptTok tok ) {
            size_t      size = tok.size();
            char const* p    = tok.data();

            auto result = parse_signed_hhmmss_offset( p, size );
            if( result != OFFSET_NPOS ) return RuleSave{ result };

            // throw failure
            throw ParseError{
                "Expected SAVE offset", tok ? bad_save : no_token, tok
            };
        }

        RuleAt parse_rule_at( OptTok tok ) {
            size_t      size = tok.size();
            char const* p    = tok.data();
            if( size > 0 )
            {
                // Parse the suffix, if present
                char suffix = p[size - 1];
                if( is_d10( suffix - '0' ) )
                {
                    auto off = parse_signed_hhmmss_offset( p, size );
                    if( off != OFFSET_NPOS )
                        return RuleAt{ off, RuleAt::LOCAL_WALL };
                }
                else
                {
                    auto off = parse_signed_hhmmss_offset( p, size - 1 );
                    if( off != OFFSET_NPOS )
                    {
                        switch( suffix )
                        {
                        case 'w': return RuleAt{ off, RuleAt::LOCAL_WALL };
                        case 's': return RuleAt{ off, RuleAt::LOCAL_STANDARD };
                        case 'g':
                        case 'u':
                        case 'z': return RuleAt{ off, RuleAt::UTC };
                        }
                    }
                }
            }

            throw ParseError{
                "Expected AT token", tok ? bad_at : no_token, tok
            };
        }

        RuleLetter parse_rule_letter( OptTok tok ) {
            constexpr RuleLetter hyphen = RuleLetter( "-" );

            size_t      size = tok.size();
            char const* p    = tok.data();
            if( size )
            {
                if( size < 8 )
                {
                    RuleLetter r{};
                    memcpy( r.buff_, p, size );
                    r.size_ = size;
                    // Special case - if the rule letter is "-", then
                    // the thing we insert into our format string should be ""
                    //
                    // This is because the file format requires a token here,
                    // and "-" was chosen to represent a LETTER that is empty.
                    if( r == hyphen ) { r = RuleLetter{}; }
                    return r;
                }
            }

            throw ParseError{
                "Expected Letter Token or '-'", tok ? bad_letter : no_token, tok
            };
        }

        FromUTC parse_zone_off( OptTok tok ) {
            size_t      size = tok.size();
            char const* p    = tok.data();

            auto result = parse_signed_hhmmss_offset( p, size );
            if( result != OFFSET_NPOS ) return FromUTC{ result };

            // throw failure
            throw ParseError{
                "Expected STDOFF offset", tok ? bad_stdoff : no_token, tok
            };
        }

        ZoneUntil parse_zone_until( string_view sv ) {
            constexpr RuleAt midnight = RuleAt( 0, RuleAt::LOCAL_WALL );

            TokenIter iter( sv );

            if( auto year = iter.next_non_comment() )
            {
                auto y = u16( parse_year( year ) );
                if( auto mon = iter.next_non_comment() )
                {
                    auto m = parse_month( mon );
                    if( auto day = iter.next_non_comment() )
                    {
                        auto on = parse_rule_on( day );

                        auto date = on.resolve_date( y, m );

                        if( auto stdoff = iter.next_non_comment() )
                        {
                            auto at = parse_rule_at( stdoff );
                            // Make sure there are no more tokens
                            if( auto extra_token = iter.next_non_comment() )
                            {
                                throw ParseError{
                                    "Expected no more tokens "
                                    "after 'until' field",
                                    "another token appeared",
                                    extra_token,
                                };
                            }
                            // 'UNTIL' field given year, month, day, and STDOFF
                            return ZoneUntil{ date, at };
                        }
                        // 'UNTIL' field given year, month, and day, but no time
                        // use midnight local time
                        return ZoneUntil{ date, midnight };
                    }
                    // 'UNTIL' field given a year and month - use midnight and
                    // the first day of the month
                    return ZoneUntil{ resolve_civil( y, m, 1 ), midnight };
                }
                // 'UNTIL' field only given a year
                return ZoneUntil{ resolve_civil( y, 1, 1 ), midnight };
            }
            // 'UNTIL' field is empty; reached end of
            // Zone entry
            return ZoneUntil::none();
        }
    } // namespace


    ZoneRule parse_zone_rule( char const* p, size_t size ) {
        if( !size )
            throw ParseError{ "Expected ZoneRule", no_token, OptTok( p, 0 ) };

        if( size > 15 )
            throw ParseError{ "Expected ZoneRule",
                "is too long to be a zone rule (expected a name 15 characters "
                "or less)",
                OptTok( p, size ) };


        if( p[0] == '-' && size == 1 )
        {
            /// From "How to Read the tz Database Source Files":
            /// > A hyphen, a kind of null value, means that we have not set our
            /// > clocks ahead of standard time.
            return ZoneRule( ZoneRule::hyphen );
        }
        else if( is_alpha( p[0] ) )
        {
            // The documentation says that the rule name should be 'Alphabetic',
            // but it does not clarify what this means. There are instances of
            // rules which use characters other than letters. For instance:
            //
            // - `E-EurAsia` uses a '-'
            // - `NT_YK` uses a '_'
            // - There is a rule named `_` in tzdata.zi
            //
            // For now, we will allow anything that is not recognized as a
            // offset or hyphen to be used as a rule name.

            RuleName name{};
            _vtz_memcpy( name.buff_, p, size );
            name.size_ = u8( size );
            return ZoneRule( name );
        }
        else
        {
            auto offset = parse_signed_hhmmss_offset( p, size );
            if( offset != OFFSET_NPOS ) return ZoneRule( offset );
        }

        throw ParseError{
            "Expected ZoneRule", bad_zone_rule, OptTok( p, size )
        };
    }

    ZoneRule parse_zone_rule( OptTok tok ) {
        return parse_zone_rule( tok.data(), tok.size() );
    }
    ZoneFormat parse_zone_format( char const* p, size_t size ) {
        if( !size )
            throw ParseError{ "Expected ZoneFormat", no_token, OptTok( p, 0 ) };

        if( size > 14 )
            throw ParseError{
                "Expected ZoneFormat",
                "is too long to be a zone format (expected 14 characters or "
                "less)",
                OptTok( p, size ),
            };

        ZoneFormat result{};

        for( size_t i = 0; i < size; i++ )
        {
            char ch = p[i];

            bool is_special = ch == '%' || ch == '/';
            if( !is_special )
            {
                result.buff[i] = ch;
                continue;
            }

            if( ch == '/' )
            {
                size_t sz1 = size - ( i + 1 );
                std::copy( p + i + 1, p + size, result.buff + i );
                result.set_fmt( ZoneFormat::SLASH, i, sz1 );
                return result;
            }
            size_t i2 = i + 1;
            if( i2 == size )
            {
                throw ParseError{
                    "Expected ZoneFormat",
                    "ended with a '%' (expected either a '%z' or a '%s', a "
                    "isolated '%' is invalid)",
                    OptTok( p, size ),
                };
            }
            char fmt_char = p[i2];
            if( !( fmt_char == 's' || fmt_char == 'z' ) )
                throw ParseError{
                    "Expected ZoneFormat",
                    "is not a recognized specifier (expected either '%s' "
                    "or '%z' if a '%' is present)",
                    OptTok( p + i, 2 ),
                };

            auto fmt = fmt_char == 's' ? ZoneFormat::FMT_S : ZoneFormat::FMT_Z;

            size_t sz1 = size - ( i + 2 );
            std::copy( p + i + 2, p + size, result.buff + i );

            result.set_fmt( fmt, i, sz1 );
            return result;
        }

        result.set_fmt( ZoneFormat::LITERAL, size, 0 );
        return result;
    }
    ZoneFormat parse_zone_format( OptTok tok ) {
        return parse_zone_format( tok.data(), tok.size() );
    }


    Link parse_link( TokenIter tok_iter ) {
        Link link;
        link.canonical = tok_iter.next();
        link.alias     = tok_iter.next();
        return link;
    }

    ZoneEntry parse_zone_entry( TokenIter tok_iter ) {
        ZoneEntry e;
        e.stdoff = parse_zone_off( tok_iter.next() );
        e.rules  = parse_zone_rule( tok_iter.next() );
        e.format = parse_zone_format( tok_iter.next() );
        e.until  = parse_zone_until( tok_iter.rest() );
        return e;
    }

    RuleEntry parse_rule_entry( TokenIter tok_iter ) {
        rule_year_t from = parse_year( tok_iter.next() );
        return RuleEntry{
            from,
            /// For the 'TO' year, the values 'only' and 'max' must also be
            /// accepted
            parse_year_to( tok_iter.next(), from ),
            // The token immediately before the month was a placeholder that
            // used to be called 'TYPE', but is now just a '-' for backwards
            // compatibility reasons. drop it, then get the month
            parse_month( tok_iter.drop().next() ),
            parse_rule_on( tok_iter.next() ),
            parse_rule_at( tok_iter.next() ),
            parse_rule_save( tok_iter.next() ),
            parse_rule_letter( tok_iter.next() ),
        };
    }

    string_view next_zone_line( LineIter& lines ) {
        while( auto next_line = lines.next() )
        {
            auto line = strip_leading_delim( *next_line );
            if( line.empty() || line[0] == '#' ) continue;
            return line;
        }
        throw ParseError{ "Expected more entries in "
                          "Zone specification",
            "End of input was reached",
            lines.rest() };
    }

    Zone parse_zone( TokenIter tok_iter, LineIter& lines ) {
        Zone z;
        z.name = tok_iter.next();
        z.ents.reserve( 32 );
        for( ;; )
        {
            auto ent = parse_zone_entry( tok_iter );
            z.ents.push_back( ent );

            // We found the last entry in the zone
            if( !ent.until.has_value() ) break;

            tok_iter = TokenIter( next_zone_line( lines ) );
        }

        return z;
    }


    /// Append timezone data from the given input
    void append_tzdata(
        TZDataFile& file, string_view input, string_view filename ) {
        try
        {
            auto lines = LineIter( input );

            while( auto next_line = lines.next() )
            {
                auto line = strip_leading_delim( *next_line );
                if( line.empty() ) continue;
                if( line[0] == '#' ) continue;

                auto tok_iter = TokenIter( line );
                // Either 'Zone' or 'Rule'
                auto what = tok_iter.next();
                if( !what.has_value() ) continue;

                if( what == "R" || what == "Rule" )
                {
                    if( auto name = tok_iter.next_non_comment() )
                    {
                        file.rules[name].push_back(
                            parse_rule_entry( tok_iter ) );
                        continue;
                    }
                    else
                    {
                        throw ParseError{
                            "Expected rule name", "name was missing", name
                        };
                    }
                }
                if( what == "L" || what == "Link" )
                {
                    Link link              = parse_link( tok_iter );
                    file.links[link.alias] = link.canonical;
                    continue;
                }

                if( what == "Z" || what == "Zone" )
                {
                    Zone zone = parse_zone( tok_iter, lines );

                    file.zones[zone.name] = std::move( zone.ents );
                    continue;
                }

                throw ParseError{
                    "Expected Zone, Rule, or Link",
                    "Did not match any of those.",
                    { line },
                };
            }

            for( auto& [_, rule_entries] : file.rules )
            {
                auto begin = rule_entries.data();
                auto end   = rule_entries.data() + rule_entries.size();

                if( !std::is_sorted( begin, end, RuleEntry::compare_from() ) )
                {
                    // This is very rare, but occasionally we end up with Rule
                    // entries which _aren't_ sorted by year (one example being
                    // SanLuis):
                    //
                    // Rule SanLuis 2008 2009 - Mar Sun>=8 0:00 0 -
                    // Rule SanLuis 2007 2008 - Oct Sun>=8 0:00 1:00 -
                    //
                    // These should be sorted by year
                    std::sort( begin, end, RuleEntry::compare_from() );
                }
            }
        }
        catch( ParseError err )
        { //
            auto loc = Location::where_ptr( input, err.token.data() );
            if( err.token.has_value() )
            {
                throw std::runtime_error(
                    fmt::format( "Error @ {}:{}: {} but {} {}",
                        filename,
                        loc.str(),
                        err.expected,
                        escape_string( err.token ),
                        err.but ) );
            }
            else
            {
                throw std::runtime_error(
                    fmt::format( "Error @ {}:{}: {} but {}",
                        filename,
                        loc.str(),
                        err.expected,
                        err.but ) );
            }
        }
    }


    static OptSV eat_tz_string_zone_abbr( char const*& p, char const* end ) {
        if( p == end ) return OptSV();
        if( *p == '<' )
        {
            ++p;
            char const* s0 = p;
            while( p != end )
            {
                if( *p++ == '>' )
                {
                    size_t sz = ( p - 1 ) - s0;
                    return string_view( s0, sz );
                }
            }
            throw ParseError{
                "Expected '>' delimiting end of zone abbreviation",
                "Found end of string"
            };
        }
        char const* s0 = p;
        while( p != end )
        {
            if( !is_alphabetic( *p ) ) return string_view( s0, p - s0 );
            ++p;
        }
        return string_view( s0, p - s0 );
    }


    ZoneAbbr to_zone_abbr( string_view sv ) {
        if( sv.size() < ZoneAbbr::max_size )
        {
            ZoneAbbr result;
            _vtz_memcpy( result.buff_, sv.data(), sv.size() );
            result.size_ = sv.size();
            return result;
        }

        throw ParseError{
            "Expected zone abbreviation",
            "is too long",
            sv,
        };
    }

    ZoneAbbr to_zone_abbr( OptSV sv ) {
        if( sv ) return to_zone_abbr( *sv );
        throw ParseError{
            "Expected zone abbreviation",
            "no zone abbreviation was given",
        };
    }

    TZDate eat_tzdate( char const*& p, char const* end ) {
        char const* s0 = p;
        if( p == end ) throw ParseError{ TZ_expected_rule, end_of_string };
        if( *p != ',' )
            throw ParseError{
                TZ_expected_rule, "isn't a valid rule", OptTok( p, end - p )
            };
        ++p;
        if( p == end ) throw ParseError{ TZ_expected_rule, end_of_string };
        if( *p == 'M' )
        {
            // Parse Mm.n.d
            ++p;
            auto m = eat_u32_or_throw( p, end );
            if( p == end || *p != '.' )
                throw ParseError{ "expected week (range [1-5]) following '.'",
                    "didn't match \".[1-5]\"",
                    OptTok( p, end - p ) };
            ++p;
            auto n = eat_u32_or_throw( p, end );
            if( p == end || *p != '.' )
                throw ParseError{
                    "expected day of week (range [0-6]) following '.'",
                    "didn't match \".[0-6]\"",
                    OptTok( p, end - p )
                };
            ++p;
            auto d = eat_u32_or_throw( p, end );
            if( m < 1 || m > 12 )
                throw ParseError{ "Expected Mm.n.d",
                    "had an out-of-bounds month (expected range [1-12])",
                    OptTok( s0, end - s0 ) };
            if( n < 1 || n > 5 )
                throw ParseError{ "Expected Mm.n.d",
                    "had an out-of-bounds week of month (expected range [1-5])",
                    OptTok( s0, end - s0 ) };
            if( d > 6 )
                throw ParseError{ "Expected Mm.n.d",
                    "had an out-of-bounds day of the week (expected range "
                    "[0-6])",
                    OptTok( s0, end - s0 ) };
            return TZDate::make_dom( Mon( m ), n, DOW( d ) );
        }
        else if( *p == 'J' )
        {
            // Parse Julian: Jn
            ++p;
            auto n = eat_u32_or_throw( p, end );
            if( n < 1 || n > 365 )
                throw ParseError{
                    "Expected Rule Date of form Jn where 1 <= n <= 365",
                    "contained an out-of-bounds value for n",
                    OptTok( s0, end - s0 )
                };
            return TZDate::make_julian( n );
        }
        else
        {
            auto n = eat_u32_or_throw( p, end );
            if( n > 365 )
                throw ParseError{ "Expected Rule Date n where 0 <= n <= 365",
                    "contained an out-of-bounds value for n",
                    OptTok( s0, end - s0 ) };
            return TZDate::make_doy( n );
        }
    }

    TZRule eat_tzrule( char const*& p, char const* end ) {
        auto date = eat_tzdate( p, end );
        if( p == end ) { return TZRule{ date, 7200 }; }
        if( *p == '/' )
        {
            ++p;
            return TZRule{ date, eat_signed_hhmmss( p, end ) };
        }
        return TZRule{ date, 7200 };
    }

    static TZString _parse_tz_string( char const* p, size_t size ) {
        char const* end   = p + size;
        ZoneAbbr    abbr1 = to_zone_abbr( eat_tz_string_zone_abbr( p, end ) );
        auto        off1  = FromUTC( -eat_signed_hhmmss( p, end ) );

        if( p == end )
        {
            return TZString{
                abbr1,
                abbr1,
                off1,
                off1,
                TZRule{},
                TZRule{},
            };
        }

        ZoneAbbr abbr2 = to_zone_abbr( eat_tz_string_zone_abbr( p, end ) );
        // From TZ String documentation:
        //
        // > ... If no offset follows dst, the alternative time is assumed
        // > to be one hour ahead of standard time
        //
        // See:
        // https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap08.html#tag_08_03
        auto off2 = off1.save( 3600 );

        if( p == end ) throw ParseError{ TZ_expected_rule, end_of_string };

        if( *p != ',' )
        {
            // Expect that we have an offset of some kind...
            off2 = FromUTC( -eat_signed_hhmmss( p, end ) );
        }

        auto result = TZString{
            abbr1,
            abbr2,
            off1,
            off2,
            eat_tzrule( p, end ),
            eat_tzrule( p, end ),
        };
        if( p != end )
            throw ParseError{
                "Expected end of TZ string",
                "came after what should have been the end",
                OptTok( p, end - p ),
            };

        return result;
    }

    TZString parse_tz_string( string_view sv ) {
        try
        {
            // Parse the tz string (throws a ParseError if parse failure occurs)
            return _parse_tz_string( sv.data(), sv.size() );
        }
        catch( vtz::ParseError const& err )
        {
            throw std::runtime_error(
                err.getErrorMessage( sv, "(TZ string)" ) );
        }
    }

    /// tzdata.zi begins with a line containing the version
    /// This line has `"# version "`, followed by the version string.
    ///
    /// We will try to load it from the file.

    OptSV load_version_from_tzdata_file( string_view content ) {
        constexpr string_view VERSION_PREFIX = "# version ";
        // If the content does not contain the version prefix
        if( !starts_with( content, VERSION_PREFIX ) ) { return OptSV(); }

        auto     remainder = content.substr( VERSION_PREFIX.size() );
        LineIter it( remainder );

        return it.next();
    }

    TZData load_zone_info_from_file( string fp ) {
        auto content = read_file_bytes( fp.c_str() );

        TZData result{
            RuleMap( RULE_BUCKETS ),
            ZoneMap( ZONE_BUCKETS ),
            LinkMap( LINK_BUCKETS ),
        };

        append_tzdata(
            result, string_view( content.data(), content.size() ), fp );

        result.data.emplace_back( std::move( fp ), std::move( content ) );

        return result;
    }


    TZData load_zone_info_from_dir( string dir ) {
        TZData result{
            RuleMap( RULE_BUCKETS ),
            ZoneMap( ZONE_BUCKETS ),
            LinkMap( LINK_BUCKETS ),
        };

        // Attempt to open tzdata.zi. If present, we'll use that, and we don't
        // need to open any of the source files.
        {
            auto fp = join_path( dir, "tzdata.zi" );
            if( auto file = std::fopen( fp.c_str(), "rb" ) )
            {
                auto content = read_file_bytes( file, fp.c_str() );
                auto content_view
                    = string_view( content.data(), content.size() );

                auto version = load_version_from_tzdata_file( content_view )
                                   .value_or( "(unknown version)" );

                result.version = std::string( version );

                append_tzdata( result, content_view, fp );

                result.data.emplace_back(
                    std::move( fp ), std::move( content ) );

                return result;
            }

            // If the file could not be opened for any reason other than
            // it being missing, we want to throw an exception. This
            // will ensure that we catch issues due to permission
            // errors, file already in use, etc
            int errc = errno;
            if( errc != ENOENT ) throw file_error( errc, fp, "opening" );
        }

        // If tzdata.zi wasn't found, we're going to attempt to load each of the
        // timezone source files.
        static constexpr char const* ZONE_FILES[]{
            "africa",
            "antarctica",
            "asia",
            "australasia",
            "backward", // Contains link files
            "etcetera", // Contains gmt zones
            "europe",
            "northamerica",
            "southamerica",
        };

        // Load the version. Throw an exception if we're unable to load the
        // version.
        result.version
            = std::string( LineIter( read_file( join_path( dir, "version" ) ) )
                    .next()
                    .value_or( "(unknown version)" ) );

        size_t num_read = 0;
        int    errc{};
        for( auto filename : ZONE_FILES )
        {
            auto fp = join_path( dir, filename );

            auto file = std::fopen( fp.c_str(), "rb" );
            if( file == nullptr )
            {
                // Save the error code - if we're not able to open any files, we
                // will use it when reporting the error message
                errc = errno;

                // We will permit missing files, so long as we're able to load
                // *some* parts of the timezone database
                if( errc == ENOENT ) { continue; }

                // All other errors will result in an exception being thrown
                throw file_error( errc, fp, "opening" );
            }

            ++num_read;
            auto content = read_file_bytes( file, fp.c_str() );

            append_tzdata(
                result, string_view( content.data(), content.size() ), fp );

            result.data.emplace_back( std::move( fp ), std::move( content ) );
        }

        if( num_read == 0 )
        {
            throw file_error( errc,
                dir.c_str(),
                "attempting to load timezone source files from" );
        }

        return result;
    }

    TZDataFile parse_tzdata( string_view input, string_view filename ) {
        TZDataFile file{
            RuleMap( RULE_BUCKETS ),
            ZoneMap( ZONE_BUCKETS ),
            LinkMap( LINK_BUCKETS ),
        };

        append_tzdata( file, input, filename );

        return file;
    }

    std::string RuleOn::str() const {
        switch( kind() )
        {
        case RuleOn::DAY: return fmt::format( "{}", day() );
        case RuleOn::DOW_LE: return fmt::format( "{}<={}", dow(), day() );
        case RuleOn::DOW_GE: return fmt::format( "{}>={}", dow(), day() );
        case RuleOn::DOW_LAST: return fmt::format( "last{}", dow() );
        }

        throw std::runtime_error( "RuleOn::str(): bad kind()" );
    }


    std::string to_hhmmss( int time_seconds ) {
        bool is_neg = time_seconds < 0;
        u32  save   = u32( std::abs( time_seconds ) );

        u32 hour  = save / 3600;
        save     %= 3600;
        u32 min   = save / 60;
        save     %= 60;
        u32 sec   = save;

        if( is_neg )
        {
            if( sec )
                return fmt::format( "-{:0>2}:{:0>2}:{:0>2}", hour, min, sec );
            else
                return fmt::format( "-{:0>2}:{:0>2}", hour, min );
        }
        else
        {
            if( sec )
                return fmt::format( "{:0>2}:{:0>2}:{:0>2}", hour, min, sec );
            else
                return fmt::format( "{:0>2}:{:0>2}", hour, min );
        }
    }
    std::string format_as( RuleSave s ) {
        if( s.save == 0 ) { return "0"; }

        return to_hhmmss( s.save );
    }

    std::string format_as( FromUTC off ) { return to_hhmmss( off.off ); }


    std::string format_as( RuleAt r ) {
        auto time = to_hhmmss( r.offset() );
        switch( r.kind() )
        {
        case RuleAt::LOCAL_WALL: return time;
        case RuleAt::LOCAL_STANDARD: return time + 's';
        case RuleAt::UTC: return time + 'u';
        default: return time + "<bad kind>";
        }
    }


    RuleSave::RuleSave( string_view text ) {
        save = parse_rule_save( OptTok( text ) ).save;
    }
    RuleAt::RuleAt( string_view text )
    : RuleAt( parse_rule_at( text ) ) {}


    ZoneEntry::ZoneEntry( string_view stdoff,
        string_view                   rules,
        string_view                   format,
        string_view                   until )

    : stdoff( stdoff )
    , rules( parse_zone_rule( rules ) )
    , format( parse_zone_format( format ) )
    , until( parse_zone_until( until ) ) {}


    std::string format_as( ZoneUntil until ) {
        if( until.has_value() )
        {
            auto ymd = to_civil( until.date );
            return fmt::format(
                "{:>4} {} {:>2} {}", ymd.year, ymd.mon(), ymd.day, until.at );
        }

        return "(none)";
    }


    size_t dump_active( RuleEntry const* active,
        size_t                           active_count,
        size_t                           year,
        RuleTrans*                       p ) {
        // Compute transitions for all active rules for the given year
        for( size_t i = 0; i < active_count; i++ )
            p[i] = active[i].resolve_trans( year );

        // Ensure the added transitions are sorted by date
        std::sort( p, p + active_count, RuleTrans::compare_date() );
        return active_count;
    }


    namespace {
        [[nodiscard]] constexpr RuleEntry const* find_newly_active( size_t year,
            RuleEntry const* begin,
            RuleEntry const* end ) noexcept {
            RuleEntry const* cursor = begin;
            while( cursor != end && cursor->from == year ) ++cursor;
            return cursor;
        }

        [[nodiscard]] RuleEntry const* pull_newly_active(
            vector<RuleEntry>& active,
            size_t             year,
            RuleEntry const*   begin,
            RuleEntry const*   end ) {
            auto cursor = find_newly_active( year, begin, end );
            active.insert( active.end(), begin, cursor );
            return cursor;
        }

        [[nodiscard]] constexpr size_t next_expiry_year(
            RuleEntry const* p, size_t size, size_t initial = -1 ) noexcept {
            for( size_t i = 0; i < size; i++ )
            {
                size_t expiry = size_t( p[i].to );
                initial       = std::min( initial, expiry );
            }
            return initial;
        }

        [[nodiscard]] size_t next_expiry_year( vector<RuleEntry> const& active,
            size_t initial_year = -1 ) noexcept {
            return next_expiry_year(
                active.data(), active.size(), initial_year );
        }

        [[nodiscard]] size_t fill_transition_table( RuleEntry const* active,
            size_t             active_count,
            size_t             year,
            size_t             year_end,
            vector<RuleTrans>& dest ) {
            // Number of new transitions we're adding
            size_t trans_count = active_count * ( year_end - year );

            size_t old_dest_size = dest.size();

            // Make sure there's enough space for all the new rules
            dest.resize( dest.size() + trans_count );

            // This is where we're putting all the new rules
            RuleTrans* p = dest.data() + old_dest_size;

            for( ; year < year_end; ++year )
            {
                // Advance the destination pointer
                p += dump_active( active, active_count, year, p );
            }

            return year_end;
        }


        /// Fill the transition table with transitions pulled from the active
        /// ruleset. Resulting entries will be sorted according to the date of
        /// the transition.
        ///
        /// Transitions will be appended to the given dest.
        ///
        /// @return year_end
        [[nodiscard]] size_t fill_transition_table(
            vector<RuleEntry> const& active,
            size_t                   year,
            size_t                   year_end,
            vector<RuleTrans>&       dest ) {
            return fill_transition_table(
                active.data(), active.size(), year, year_end, dest );
        }
    } // namespace


    constexpr bool rule_null( string_view rule ) noexcept {
        return rule.size() == 1 && rule[0] == '-';
    }
    constexpr bool rule_numeric( string_view rule ) noexcept {
        char ch = rule.size() > 0 ? rule[0] : '\0';
        return ch == '+' || ch == '-' || is_d10( ch - '0' );
    }

    struct UntilDate {
        sysdays_t    date; ///< Date, as days since epoch
        sysseconds_t T;    ///< DateTime when current state ends
    };

    struct ZTAgglomerator : ZoneStates {
      private:

        ZoneAbbr current_abbr;
        i32      current_off;
        i32      current_stdoff;


        map<ZoneAbbr, size_t> abbr_lookup;

        AbbrBlock add_abbr( ZoneAbbr const& abbr ) {
            size_t& id = abbr_lookup[abbr];
            // If it's a valid id, we just return (id - 1) to get the index
            if( id ) return AbbrBlock::make( id - 1, abbr.size_ );

            // Otherwise we set the id and update the table
            auto result = abbr_table_.size();
            id          = result + 1;
            abbr_table_.push_back( abbr );
            return AbbrBlock::make( result, abbr.size_ );
        }


      public:

        ZoneState current() const noexcept {
            return {
                FromUTC( current_stdoff ),
                FromUTC( current_off ),
                current_abbr,
            };
        }

        void set_initial( ZoneState state ) noexcept {
            abbr_initial_    = add_abbr( state.abbr );
            walloff_initial_ = state.walloff.off;
            stdoff_initial_  = state.stdoff.off;

            current_abbr   = state.abbr;
            current_off    = state.walloff.off;
            current_stdoff = state.stdoff.off;
        }

        void add( ZoneTransition const& trans ) {
            auto abbr   = trans.state.abbr;
            auto off    = trans.state.walloff.off;
            auto stdoff = trans.state.stdoff.off;
            auto when   = trans.when;

            bool any_trans = false;

            if( abbr != current_abbr )
            {
                any_trans    = true;
                current_abbr = abbr;
                abbr_.push_back( add_abbr( abbr ) );
                abbr_trans_.push_back( when );
            }
            if( off != current_off )
            {
                any_trans   = true;
                current_off = off;
                walloff_.push_back( off );
                walloff_trans_.push_back( when );
            }
            if( stdoff != current_stdoff )
            {
                any_trans      = true;
                current_stdoff = stdoff;
                stdoff_.push_back( stdoff );
                stdoff_trans_.push_back( when );
            }

            if( any_trans ) { tt_.push_back( trans.when ); }
        }

        ZoneStates get_states( i64 safe_cycle_time ) && noexcept {
            this->safe_cycle_time = safe_cycle_time;
            return static_cast<ZoneStates&&>( *this );
        }
    };


    static i64 get_safe_cycle_time(
        span<ZoneEntry const> zone_entries, i32 last_rule_end_year ) {
        // No entries, doesn't matter
        size_t n = zone_entries.size();
        if( n == 0 ) return 0;

        auto const& last_rule = zone_entries[n - 1].rules;

        sysdays_t last_rule_cycle_date;

        if( last_rule.is_named() )
        {
            // If the last entry in the zone refers to a named rule, figure out
            // the date that rule becomes cyclic
            last_rule_cycle_date = resolve_civil( last_rule_end_year, 1, 1 );
        }
        else
        {
            // The rule does not become cyclic
            last_rule_cycle_date = 0;
        }


        // If there's only one rule, just return the last rule cycle date,
        // converted to seconds If this number is 0, that's fine, we don't care
        if( n == 1 ) { return days_to_seconds( last_rule_cycle_date ); }

        auto until_date = zone_entries[n - 2].until.date;

        auto cycle_date = std::max( {
            0,
            2 + until_date,
            2 + last_rule_cycle_date,
        } );

        return days_to_seconds( cycle_date );
    }


    vector<ZoneTransition> TZString::get_states(
        sysseconds_t T, sysseconds_t maxT ) const {
        // If there are no daylight rules, there are no transitions
        if( !has_daylight_rules() ) return {};


        // Occasionally, you will have a zone such as Africa/Casablanca, which
        // is marked as being 'always in daylight savings time'. An example TZ
        // String is:
        //
        // `"XXX-2<+01>-1,0/0,J365/23"`
        //
        // Note here that 'XXX' is never intended to actually show up - standard
        // time ends, and then daylight savings time begins again immediately
        // after, so the time actually spend in standard time in 0.
        //
        // In this case, there should be no generated zone transitions.
        if( resolve_dst( 2001 ) == resolve_std( 2000 ) ) return {};

        auto it = TZStringIter::start_after( *this, T );

        std::vector<ZoneTransition> result;
        for( ;; )
        {
            ZoneTransition trans = it.next();
            if( trans.when >= maxT ) return result;
            result.push_back( trans );
        }
    }

    template<tzfile_kind KindT>
    ZoneStates load_zone_states_tzfile( basic_tzfile<KindT> const& file ) {
        using namespace endian;

        ZoneState initial = file.initial_state();

        size_t timecnt = file.tzh_timecnt;

        /// If there are no transition times, just get the initial state of the
        /// file

        if( timecnt == 0 ) return ZoneStates::make_static( initial );

        auto TT           = file.transition_times();
        auto type_indices = file.type_indices();
        auto leap         = LeapTable( file.leap() );

        // We are going to keep track of the current stdoff. This will be
        // updated as we compute Zone States
        FromUTC stdoff = initial.stdoff;

        ZTAgglomerator agg;

        agg.set_initial( initial );

        for( size_t i = 0;; )
        {
            i64  T     = leap.remove_leap( TT[i] );
            auto ti    = type_indices[i];
            auto state = file.state_from_ti( ti, stdoff );
            agg.add( ZoneTransition{ T, state } );

            ++i;

            // We are going to exit the loop. But first we want to do some stuff
            // with the values that are still in scope
            if( i < timecnt ) continue;

            i64 safe_cycle_time = T + 1;

            if constexpr( basic_tzfile<KindT>::is_64_bit )
            {
                /// Load the posix tz string.
                TZString posix_tz = parse_tz_string( file.tz_string() );

                auto states = posix_tz.get_states(
                    T, safe_cycle_time + SECS_400_YEARS );

                for( auto st : states ) { agg.add( st ); }
            }

            return std::move( agg ).get_states( safe_cycle_time );
        }
    }

    ZoneStates load_zone_states_tzfile( std::string const& fp ) {
        auto contents = vtz::read_file_bytes( fp );
        auto bytes    = vtz::span<char const>( contents );
        auto file32   = vtz::tzfile32( bytes );

        if( file32.version_is_modern() )
        {
            auto file64 = file32.load_modern();
            return load_zone_states_tzfile( file64 );
        }

        return load_zone_states_tzfile( file32 );
    }

    ZoneStates load_zone_states( span<ZoneEntry const> entries,
        map<string_view, ZoneTransIter>                cache,
        i64                                            safe_cycle_time,
        i32                                            end_year ) {
        sysseconds_t const STOP_TIME
            = end_year > 0
                  // If the end year is specified, use that as the stop time
                  ? days_to_seconds( resolve_civil( end_year, 1, 1 ) )
                  // Otherwise, use the time the rule becomes cyclic, plus 400
                  // years
                  : safe_cycle_time + SECS_400_YEARS;

        if( entries.size() == 0 )
            return ZoneStates::make_static(
                { FromUTC( 0 ), FromUTC( 0 ), ZoneAbbr{ 3, "-00" } } );

        ZTAgglomerator agg;

        // There is no 'until'. Therefore, just evaluate whatever rule is
        // present until the end year is reached
        if( entries.size() == 1 )
        {
            auto const& ent    = entries.front();
            auto const& rule   = ent.rules;
            FromUTC     stdoff = ent.stdoff;
            ZoneFormat  format = ent.format;
            // If there is no associated rule, there are no state transitions -
            // this zone is steady-state
            if( rule.is_hyphen() )
            {
                // We only have one entry in the zone, and also there is no
                // associated rule, so there are no state changes.
                return ZoneStates::make_static( { stdoff, format, "-" } );
            }

            if( rule.is_offset() )
            {
                // The rule only contains an offset. Again, no other entries, so
                // no state changes, so... it's a steady-state zone
                return ZoneStates::make_static(
                    { stdoff, rule.save(), format, "-" } );
            }

            auto& iter = cache.at( rule.name() );

            agg.set_initial(
                ZoneState{ stdoff, format, iter.current_letter() } );

            while( auto const& maybe_trans
                   = iter.next( STOP_TIME, stdoff, format ) )
            {
                // As long as we have transitions, add them.
                agg.add( *maybe_trans );
            }

            return std::move( agg ).get_states( safe_cycle_time );
        }

        sysseconds_t until_t;
        // Handle first rule

        {
            auto const& ent    = entries.front();
            auto const& rule   = ent.rules;
            auto const& stdoff = ent.stdoff;
            auto const& format = ent.format;
            auto const& until  = ent.until;
            if( rule.is_hyphen() )
            {
                agg.set_initial( { stdoff, format, "-" } );
                until_t = until.resolve( agg.initial() );
            }
            else if( rule.is_offset() )
            {
                agg.set_initial( { stdoff, rule.save(), format, "-" } );
                until_t = until.resolve( agg.initial() );
            }
            else
            {
                auto& iter = cache.at( rule.name() );

                agg.set_initial( { stdoff, format, iter.current_letter() } );
                until_t = until.resolve( agg.initial() );

                while( auto const& maybe_trans
                       = iter.next( until_t, stdoff, format ) )
                {
                    auto const& trans = *maybe_trans;
                    agg.add( trans );
                    until_t = until.resolve( trans.state );
                }
            }
        }

        for( size_t i = 1; i < entries.size() - 1; i++ )
        {
            auto const& ent    = entries[i];
            auto const& rule   = ent.rules;
            auto const& stdoff = ent.stdoff;
            auto const& format = ent.format;
            auto        until  = ent.until;

            if( rule.is_hyphen() )
            {
                auto state = ZoneState{ stdoff, format, "-" };
                agg.add( { until_t, state } );
                until_t = until.resolve( state );
            }
            else if( rule.is_offset() )
            {
                auto state = ZoneState{ stdoff, rule.save(), format, "-" };
                agg.add( { until_t, state } );
                until_t = until.resolve( state );
            }
            else
            {
                auto& iter = cache.at( rule.name() );
                auto  state
                    = iter.advance_to( until_t, stdoff, format, agg.current() );
                agg.add( { until_t, state } );
                until_t = until.resolve( state );

                while( auto const& maybe_trans
                       = iter.next( until_t, stdoff, format ) )
                {
                    auto const& trans = *maybe_trans;
                    agg.add( trans );
                    until_t = until.resolve( trans.state );
                }
            }
        }

        // The last entry does not have an 'until' time
        {
            auto const& ent    = entries.back();
            auto const& rule   = ent.rules;
            auto const& stdoff = ent.stdoff;
            auto const& format = ent.format;

            if( rule.is_hyphen() )
            {
                auto state = ZoneState{ stdoff, format, "-" };
                agg.add( { until_t, state } );
            }
            else if( rule.is_offset() )
            {
                auto state = ZoneState{ stdoff, rule.save(), format, "-" };
                agg.add( { until_t, state } );
            }
            else
            {
                auto& iter = cache.at( rule.name() );
                auto  state
                    = iter.advance_to( until_t, stdoff, format, agg.current() );
                agg.add( { until_t, state } );

                while( auto const& maybe_trans
                       = iter.next( STOP_TIME, stdoff, format ) )
                {
                    auto const& trans = *maybe_trans;
                    agg.add( trans );
                }
            }
        }

        return std::move( agg ).get_states( safe_cycle_time );
    }


    ZoneStates TimeZoneCache::compute_states( string_view name ) const {
        // Represents a map of rule names to evaluated rules
        using RuleCache = map<string_view, ZoneTransIter>;

        auto const& entries = data.zones.at( name );

        if( entries.empty() )
            throw std::runtime_error(
                fmt::format( "Error: zone '{}' contained no entries",
                    escape_string( name ) ) );

        auto cache = RuleCache( 16 );

        int last_rule_end_year = 1970;
        for( auto const& ent : entries )
        {
            auto const& rule = ent.rules;
            if( rule.is_named() )
            {
                auto  rule_name    = rule.name();
                auto& rule_entry   = locate_rule( rule_name );
                last_rule_end_year = rule_entry.year_end;
                cache.emplace( rule_name, ZoneTransIter( rule_entry ) );
            }
        }

        return load_zone_states( entries,
            std::move( cache ),
            get_safe_cycle_time( entries, last_rule_end_year ),
            -1 );
    }


    ZoneStates TZDataFile::get_zone_states(
        string_view name, i32 end_year ) const {
        // Represents a map of rule names to evaluated rules
        using RuleCache = map<string_view, ZoneTransIter>;


        auto const& entries = zones.at( name );
        if( entries.empty() )
            throw std::runtime_error(
                fmt::format( "Error: zone '{}' contained no entries",
                    escape_string( name ) ) );


        auto rules = map<string_view, RuleEvalResult>( 16 );

        int last_rule_end_year = 1970;

        for( auto const& ent : entries )
        {
            auto const& rule = ent.rules;
            if( rule.is_named() )
            {
                auto name          = rule.name();
                auto eval          = evaluate_rules( name );
                last_rule_end_year = eval.year_end;
                rules.emplace( name, std::move( eval ) );
            }
        }

        auto cache = RuleCache( 16 );
        for( auto const& [k, rule] : rules ) { cache.emplace( k, rule ); }

        return load_zone_states( entries,
            std::move( cache ),
            get_safe_cycle_time( entries, last_rule_end_year ),
            end_year );
    }


    RuleEvalResult evaluate_rules(
        RuleEntry const* begin, RuleEntry const* end ) {
        if( begin == end )
            throw std::runtime_error(
                "evaluate_rules(): Error: given empty rule set" );

        if( !std::is_sorted( begin, end, RuleEntry::compare_from() ) )
            throw std::runtime_error(
                "Expected Rule to be sorted by 'FROM' year" );

        auto active = vector<RuleEntry>(); ///< Current set of active rules
        auto tt     = vector<RuleTrans>(); ///< Computed Transition Table

        size_t year0 = begin->from;        ///< First year with a rule

        size_t year   = year0;             ///< Cursor holding the current year
        auto   cursor = begin;             ///< Cursor holding the next rule

        // Get the initial set of active rules
        cursor = pull_newly_active( active, year, cursor, end );

        // We're going to loop as long as there are rules that can expire
        while( cursor != end )
        {
            // Figure out how long we can loop for. We need to stop when we
            // either hit an expiry, or when a new rule is going to be added
            auto end_year = next_expiry_year( active, cursor->from - 1 ) + 1;

            // Fill rules from the current year to the stop year
            // (we add 1 to the stop year, because we want to include it)
            year = fill_transition_table( active, year, end_year, tt );

            // Remove any rules that have expired
            active.erase( std::remove_if( active.begin(),
                              active.end(),
                              RuleEntry::is_expired( year ) ),
                active.end() );

            // We have no active rules right now, so we should jump forward
            // to the next year that has a rule.
            //
            // We know that cursor->from >= year, because
            // `next_expiry_year( ..., initial ) <= initial`
            if( active.empty() ) year = cursor->from;

            // Pull any rules that are newly active
            cursor = pull_newly_active( active, year, cursor, end );
        }

        static_assert( Y_MAX == -1 );
        static_assert( next_expiry_year( nullptr, 0 ) == -1 );

        // We want to pull any remaining rules that have a fixed expiry date.
        //
        // When only rules with no expiry remain (or there are no more active
        // rules), `next_expiry_year( active )` will return -1, so end_year will
        // be 0, so we will break out of the loop.
        while( auto end_year = next_expiry_year( active ) + 1 )
        {
            year = fill_transition_table( active, year, end_year, tt );

            // Remove any rules that have expired
            active.erase( std::remove_if( active.begin(),
                              active.end(),
                              RuleEntry::is_expired( year ) ),
                active.end() );
        }

        return {
            std::move( tt ),
            std::move( active ),
            i32( year0 ),
            i32( year ),
        };
    }

    RuleEvalResult evaluate_rules( vector<RuleEntry> const& rules ) {
        return evaluate_rules( rules.data(), rules.data() + rules.size() );
    }

    string RuleTrans::str() const {
        return fmt::format( "{} @ {} SAVE={} LETTER='{}'",
            to_civil( date ),
            at,
            save,
            letter.sv() );
    }
    string ZoneFormat::str() const {
        size_t      sz0 = ( fmt_ >> 2 ) & 0xf;
        size_t      sz1 = ( fmt_ >> 6 ) & 0xf;
        string_view h0( buff, sz0 );
        string_view h1( buff + sz0, sz1 );
        string_view mid;
        switch( tag() )
        {
        case LITERAL: mid = {}; break;
        case SLASH: mid = "/"; break;
        case FMT_S: mid = "%s"; break;
        case FMT_Z: mid = "%z"; break;
        }
        return fmt::format( "{}{}{}", h0, mid, h1 );
    }
    string ZoneRule::str() const {
        switch( kind() )
        {
        case HYPHEN: return "-";
        case NAMED: return string( name() );
        case OFFSET: return to_hhmmss( offset() );
        }

        throw std::runtime_error(
            "ZoneRule::str(): kind() is invalid/corrupt" );
    }
    vector<ZoneTransition> ZoneStates::get_transitions() const {
        constexpr static sysseconds_t MAX_TIME
            = std::numeric_limits<sysseconds_t>::max();

        span stt = stdoff_trans_;
        span wtt = walloff_trans_;
        span rtt = abbr_trans_;

        span svv = stdoff_;
        span wvv = walloff_;
        span rvv = abbr_;

        size_t si = 0;
        size_t wi = 0;
        size_t ri = 0;

        auto                   s_current = stdoff_initial_;
        auto                   w_current = walloff_initial_;
        auto                   r_current = abbr_initial_;
        vector<ZoneTransition> result;
        for( ;; )
        {
            auto st = stt.value_or( si, MAX_TIME );
            auto wt = wtt.value_or( wi, MAX_TIME );
            auto rt = rtt.value_or( ri, MAX_TIME );

            auto T = std::min( { st, wt, rt } );
            if( T == MAX_TIME ) break;

            s_current = svv.value_or( si, s_current );
            w_current = wvv.value_or( wi, w_current );
            r_current = rvv.value_or( ri, r_current );

            // Update each index as needed
            si += int( st == T );
            wi += int( wt == T );
            ri += int( rt == T );

            result.push_back( ZoneTransition{
                T,
                ZoneState{
                    FromUTC( s_current ),
                    FromUTC( w_current ),
                    abbr_table_[r_current.index()],
                },
            } );
        }

        return result;
    }
    std::string format_as( AbbrBlock b ) {
        return fmt::format( "(index: {}, size: {})", b.index(), b.size() );
    }
    RuleEvalResult TZDataFile::evaluate_rules( string_view rule ) const {
        auto it = rules.find( rule );
        if( it == rules.end() )
        {
            throw std::runtime_error( fmt::format(
                "Unable to load rule {}", escape_string( rule ) ) );
        }

        try
        {
            // Attempt to evaluate the rules.
            return vtz::evaluate_rules( it->second );
        }
        catch( std::exception const& ex )
        {
            throw std::runtime_error(
                fmt::format( "Error when loading rule {}. {}",
                    escape_string( rule ),
                    ex.what() ) );
        }
    }

} // namespace vtz
