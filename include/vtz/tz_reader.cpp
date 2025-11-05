#include <fmt/base.h>
#include <limits>
#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/span.h>
#include <vtz/strings.h>
#include <vtz/tz_reader.h>
#include <vtz/tz_reader/FromUTC.h>
#include <vtz/tz_reader/ZoneFormat.h>
#include <vtz/tz_reader/ZoneRule.h>

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
        constexpr char const* no_token = "no token was given";
        constexpr char const* bad_SAVE
            = "is not a valid SAVE. SAVE must fits one of the following "
              "forms: "
              "[-]H, [-]HH, [-]H:MM, [-]HH:MM, [-]H:MM:SS, or [-]HH:MM:SS";
        constexpr char const* bad_STDOFF
            = "is not a valid STDOFF. STDOFF must fits one of the following "
              "forms: "
              "[-]H, [-]HH, [-]H:MM, [-]HH:MM, [-]H:MM:SS, or [-]HH:MM:SS";
        constexpr char const* bad_AT
            = "is not a valid AT token. AT must fits one of the following "
              "forms: "
              "[-]H[sguzw], [-]HH[sguzw], [-]H:MM[sguzw], [-]HH:MM[sguzw], "
              "[-]H:MM:SS[sguzw], or [-]HH:MM:SS[sguzw]";
        constexpr char const* exp_RULE_ON
            = "Expected rule describing when within the month "
              "a transition occurs";
        constexpr char const* bad_RULE_ON
            = "could not be parsed. Valid forms are: 'last[DOW]', "
              "'[DOW]>=[DOM]', '[DOW]<=[DOM]', or '[DOM]', where "
              "'[DOW]' represents "
              "Sun, Mon, Tue, Wed, Thu, Fri, or Sat, and DOM is an "
              "integer 1-31.";

        constexpr char const* bad_LETTER
            = "is too large to be valid for the LETTER/S field.";

        constexpr char const* bad_ZONE_RULE
            = "is not a valid value for the 'RULES' column. A Zone Rule must "
              "be either a hyphen (\"-\"), indicating a null value; it must be "
              "an amount of time (eg, \"1:00\"), which we set our clocks ahead "
              "by; or it must be an alphabetic string which names a rule.";


        /// Return true if the character is an ascii letter (either lowercase or
        /// uppercase)
        constexpr auto isAlpha = []( char ch ) noexcept -> bool {
            return ( 'a' <= ch && ch <= 'z' )    // ch is lowercase
                   || ( 'A' <= ch && ch <= 'Z' ) // ch is uppercase
                   || ( ch == '_' );             // ch is a '_'
        };

        constexpr inline bool isD10( int x ) noexcept {
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
                isD10( v0 ) && isD10( v1 ) && isD10( v2 ) && isD10( v3 ),
            };
        }

        // Parse 1 or 2 digits. Anything else is bad.
        constexpr TrivialOpt<i32> parse1or2(
            char const* src, size_t size ) noexcept {
            if( size == 2 )
            {
                i32 v0 = src[0] - '0';
                i32 v1 = src[1] - '0';
                return { v0 * 10 + v1, isD10( v0 ) && isD10( v1 ) };
            }
            if( size == 1 )
            {
                i32 v0 = src[0] - '0';
                return { v0, isD10( v0 ) };
            }
            return { 0, false };
        }
    } // namespace

    rule_year_t parseYear( OptTok tok ) {
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

    rule_year_t parseYearTo( OptTok tok, rule_year_t from ) {
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

    Mon parseMonth( OptTok tok ) {
        using _impl::_load1;
        using _impl::_load2;
        using _impl::_load3;
        using _impl::_parseMon;

        if( auto m = _parseMon( tok.data(), tok.size() ) ) return *m;

        throw ParseError{
            "Expected month",
            tok.has_value() ? "is not a valid month name" : "month is missing",
            tok,
        };
    }

    u8 parseDayOfMonth( char const* tok, size_t size ) {
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

    RuleOn parseRuleOn( OptTok tok ) {
        using namespace _impl;

        size_t      size = tok.size();
        char const* p    = tok.data();

        if( size <= 2 )
        {
            // For size<=2, it can only be a day
            return RuleOn::on( parseDayOfMonth( p, size ) );
        }
        else if( size >= 4 )
        {
            constexpr auto _last = _load4( "last" );
            if( _load4( p ) == _last )
            {
                if( OptDOW dow = _parseDOW( p + 4, size - 4 ) )
                    return RuleOn::last( *dow );
            }
            else
            {
                size_t dowLen{};
                if( p[2] == '=' ) // Eg, M>=3 or M<=3
                    dowLen = 1;
                else if( p[3] == '=' )
                    dowLen = 2;
                else if( size > 4 && p[4] == '=' )
                    dowLen = 3;

                if( dowLen )
                {
                    // We know it must be of form
                    // \w{dowLen}[<>]=\d+
                    char   ge_or_le = p[dowLen];
                    OptDOW dow      = _parseDOW( p, dowLen );
                    auto   day      = parseDayOfMonth(
                        p + ( dowLen + 2 ), size - ( dowLen + 2 ) );

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
            exp_RULE_ON, tok.has_value() ? bad_RULE_ON : no_token, tok
        };
    }

    i32 parseHHMMSSOffset( char const* p, size_t size, int sign ) noexcept {
        if( !size ) return OFFSET_NPOS;

        if( size <= 8 )
        {
            char buff[8]{};
            _vtz_memcpy( buff, p, size );
            int sepPos[8];
            int numSep = 0;
            // H:M [1]
            // H:M:S [1, 3]
            // H:MM:S [1, 4]
            // H:MM:SS [1, 4]
            // HH:M [2]
            // HH:M:S [2, 4]
            // HH:M:SS [2, 4]
            // HH:MM:S [2, 5]
            // HH:MM:SS [2, 5]
            if( buff[1] == ':' ) sepPos[numSep++] = 1;
            if( buff[2] == ':' ) sepPos[numSep++] = 2;
            if( buff[3] == ':' ) sepPos[numSep++] = 3;
            if( buff[4] == ':' ) sepPos[numSep++] = 4;
            if( buff[5] == ':' ) sepPos[numSep++] = 5;
            if( numSep == 0 )
            {
                if( auto hh = parse1or2( buff, size ) )
                    return sign * _hms( *hh );
            }
            else if( numSep == 1 )
            {
                int  p0 = sepPos[0];
                auto hh = parse1or2( buff, p0 );
                auto mm = parse1or2( buff + p0 + 1, size - p0 - 1 );
                if( hh && mm ) return sign * _hms( *hh, *mm );
            }
            else if( numSep == 2 )
            {
                int p0 = sepPos[0];
                int p1 = sepPos[1];
                // fmt::println( "input = {} sep={} pos={}, {}",
                //     string_view( p, size ),
                //     numSep,
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

    i32 parseSignedHHMMSSOffset( char const* p, size_t size ) noexcept {
        if( size > 0 )
        {
            char ch = p[0];
            if( isD10( ch - '0' ) ) return parseHHMMSSOffset( p, size, 1 );
            if( ch == '-' ) return parseHHMMSSOffset( p + 1, size - 1, -1 );
            if( ch == '+' ) return parseHHMMSSOffset( p + 1, size - 1, 1 );
        }
        return OFFSET_NPOS;
    }

    namespace {

        RuleSave parseRuleSave( OptTok tok ) {
            size_t      size = tok.size();
            char const* p    = tok.data();

            auto result = parseSignedHHMMSSOffset( p, size );
            if( result != OFFSET_NPOS ) return RuleSave{ result };

            // throw failure
            throw ParseError{
                "Expected SAVE offset", tok ? bad_SAVE : no_token, tok
            };
        }

        RuleAt parseRuleAt( OptTok tok ) {
            size_t      size = tok.size();
            char const* p    = tok.data();
            if( size > 0 )
            {
                // Parse the suffix, if present
                char suffix = p[size - 1];
                if( isD10( suffix - '0' ) )
                {
                    auto off = parseSignedHHMMSSOffset( p, size );
                    if( off != OFFSET_NPOS )
                        return RuleAt{ off, RuleAt::LOCAL_WALL };
                }
                else
                {
                    auto off = parseSignedHHMMSSOffset( p, size - 1 );
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
                "Expected AT token", tok ? bad_AT : no_token, tok
            };
        }

        RuleLetter parseRuleLetter( OptTok tok ) {
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
                "Expected Letter Token or '-'", tok ? bad_LETTER : no_token, tok
            };
        }

        FromUTC parseZoneOff( OptTok tok ) {
            size_t      size = tok.size();
            char const* p    = tok.data();

            auto result = parseSignedHHMMSSOffset( p, size );
            if( result != OFFSET_NPOS ) return FromUTC{ result };

            // throw failure
            throw ParseError{
                "Expected STDOFF offset", tok ? bad_STDOFF : no_token, tok
            };
        }

        ZoneUntil parseZoneUntil( string_view sv ) {
            constexpr RuleAt midnight = RuleAt( 0, RuleAt::LOCAL_WALL );

            TokenIter iter( sv );

            if( auto year = iter.nextNonComment() )
            {
                auto y = u16( parseYear( year ) );
                if( auto mon = iter.nextNonComment() )
                {
                    auto m = parseMonth( mon );
                    if( auto day = iter.nextNonComment() )
                    {
                        auto on = parseRuleOn( day );

                        auto date = on.resolveDate( y, m );

                        if( auto stdoff = iter.nextNonComment() )
                        {
                            auto at = parseRuleAt( stdoff );
                            // Make sure there are no more tokens
                            if( auto extraToken = iter.nextNonComment() )
                            {
                                throw ParseError{
                                    "Expected no more tokens "
                                    "after 'until' field",
                                    "another token appeared",
                                    extraToken,
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
                    return ZoneUntil{ resolveCivil( y, m, 1 ), midnight };
                }
                // 'UNTIL' field only given a year
                return ZoneUntil{ resolveCivil( y, 1, 1 ), midnight };
            }
            // 'UNTIL' field is empty; reached end of
            // Zone entry
            return ZoneUntil::none();
        }
    } // namespace


    ZoneRule parseZoneRule( char const* p, size_t size ) {
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
        else if( isAlpha( p[0] ) )
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
            auto offset = parseSignedHHMMSSOffset( p, size );
            if( offset != OFFSET_NPOS ) return ZoneRule( offset );
        }

        throw ParseError{
            "Expected ZoneRule", bad_ZONE_RULE, OptTok( p, size )
        };
    }

    ZoneRule parseZoneRule( OptTok tok ) {
        return parseZoneRule( tok.data(), tok.size() );
    }
    ZoneFormat parseZoneFormat( char const* p, size_t size ) {
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

            bool isSpecial = ch == '%' || ch == '/';
            if( !isSpecial )
            {
                result.buff[i] = ch;
                continue;
            }

            if( ch == '/' )
            {
                size_t sz1 = size - ( i + 1 );
                std::copy( p + i + 1, p + size, result.buff + i );
                result.setFmt( ZoneFormat::SLASH, i, sz1 );
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
            char fmtChar = p[i2];
            if( !( fmtChar == 's' || fmtChar == 'z' ) )
                throw ParseError{
                    "Expected ZoneFormat",
                    "is not a recognized specifier (expected either '%s' "
                    "or '%z' if a '%' is present)",
                    OptTok( p + i, 2 ),
                };

            auto fmt = fmtChar == 's' ? ZoneFormat::FMT_S : ZoneFormat::FMT_Z;

            size_t sz1 = size - ( i + 2 );
            std::copy( p + i + 2, p + size, result.buff + i );

            result.setFmt( fmt, i, sz1 );
            return result;
        }

        result.setFmt( ZoneFormat::LITERAL, size, 0 );
        return result;
    }
    ZoneFormat parseZoneFormat( OptTok tok ) {
        return parseZoneFormat( tok.data(), tok.size() );
    }


    Link parseLink( TokenIter tok_iter ) {
        Link link;
        link.canonical = tok_iter.next();
        link.alias     = tok_iter.next();
        return link;
    }

    ZoneEntry parseZoneEntry( TokenIter tok_iter ) {
        ZoneEntry e;
        e.stdoff = parseZoneOff( tok_iter.next() );
        e.rules  = parseZoneRule( tok_iter.next() );
        e.format = parseZoneFormat( tok_iter.next() );
        e.until  = parseZoneUntil( tok_iter.rest() );
        return e;
    }

    RuleEntry parseRuleEntry( TokenIter tok_iter ) {
        rule_year_t from = parseYear( tok_iter.next() );
        return RuleEntry{
            from,
            /// For the 'TO' year, the values 'only' and 'max' must also be
            /// accepted
            parseYearTo( tok_iter.next(), from ),
            // The token immediately before the month was a placeholder that
            // used to be called 'TYPE', but is now just a '-' for backwards
            // compatibility reasons. drop it, then get the month
            parseMonth( tok_iter.drop().next() ),
            parseRuleOn( tok_iter.next() ),
            parseRuleAt( tok_iter.next() ),
            parseRuleSave( tok_iter.next() ),
            parseRuleLetter( tok_iter.next() ),
        };
    }

    string_view nextZoneLine( LineIter& lines ) {
        while( auto nextLine = lines.next() )
        {
            auto line = stripLeadingDelim( *nextLine );
            if( line.empty() || line[0] == '#' ) continue;
            return line;
        }
        throw ParseError{ "Expected more entries in "
                          "Zone specification",
            "End of input was reached",
            lines.rest() };
    }

    Zone parseZone( TokenIter tok_iter, LineIter& lines ) {
        Zone z;
        z.name = tok_iter.next();
        z.ents.reserve( 32 );
        for( ;; )
        {
            auto ent = parseZoneEntry( tok_iter );
            z.ents.push_back( ent );

            // We found the last entry in the zone
            if( !ent.until.has_value() ) break;

            tok_iter = TokenIter( nextZoneLine( lines ) );
        }

        return z;
    }

    TZDataFile parseTZData( string_view input, string_view filename ) {
        try
        {
            auto       lines = LineIter( input );
            TZDataFile file{
                RuleMap( 512 ),
                ZoneMap( 512 ),
                LinkMap( 512 ),
            };

            while( auto nextLine = lines.next() )
            {
                auto line = stripLeadingDelim( *nextLine );
                if( line.empty() ) continue;
                if( line[0] == '#' ) continue;

                auto tok_iter = TokenIter( line );
                // Either 'Zone' or 'Rule'
                auto what = tok_iter.next();
                if( !what.has_value() ) continue;

                if( what == "R" || what == "Rule" )
                {
                    if( auto name = tok_iter.nextNonComment() )
                    {
                        file.rules[name].push_back(
                            parseRuleEntry( tok_iter ) );
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
                    Link link              = parseLink( tok_iter );
                    file.links[link.alias] = link.canonical;
                    continue;
                }

                if( what == "Z" || what == "Zone" )
                {
                    Zone zone = parseZone( tok_iter, lines );

                    file.zones[zone.name] = std::move( zone.ents );
                    continue;
                }

                throw ParseError{
                    "Expected Zone, Rule, or Link",
                    "Did not match any of those.",
                    { line },
                };
            }

            for( auto& [_, ruleEntries] : file.rules )
            {
                auto begin = ruleEntries.data();
                auto end   = ruleEntries.data() + ruleEntries.size();

                if( !std::is_sorted( begin, end, RuleEntry::compareFrom() ) )
                {
                    // This is very rare, but occasionally we end up with Rule
                    // entries which _aren't_ sorted by year (one example being
                    // SanLuis):
                    //
                    // Rule SanLuis 2008 2009 - Mar Sun>=8 0:00 0 -
                    // Rule SanLuis 2007 2008 - Oct Sun>=8 0:00 1:00 -
                    //
                    // These should be sorted by year
                    std::sort( begin, end, RuleEntry::compareFrom() );
                }
            }
            return file;
        }
        catch( ParseError err )
        { //
            auto loc = Location::wherePtr( input, err.token.data() );

            if( err.token.has_value() )
            {
                throw std::runtime_error(
                    fmt::format( "Error @ {}:{}: {} but {} {}",
                        filename,
                        loc.str(),
                        err.expected,
                        escapeString( err.token ),
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


    std::string toHHMMSS( int timeSeconds ) {
        bool isNeg = timeSeconds < 0;
        u32  save  = u32( std::abs( timeSeconds ) );

        u32 hour  = save / 3600;
        save     %= 3600;
        u32 min   = save / 60;
        save     %= 60;
        u32 sec   = save;

        if( isNeg )
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

        return toHHMMSS( s.save );
    }

    std::string format_as( FromUTC off ) { return toHHMMSS( off.off ); }


    std::string format_as( RuleAt r ) {
        auto time = toHHMMSS( r.offset() );
        switch( r.kind() )
        {
        case RuleAt::LOCAL_WALL: return time;
        case RuleAt::LOCAL_STANDARD: return time + 's';
        case RuleAt::UTC: return time + 'u';
        default: return time + "<bad kind>";
        }
    }


    RuleSave::RuleSave( string_view text ) {
        save = parseRuleSave( OptTok( text ) ).save;
    }
    RuleAt::RuleAt( string_view text )
    : RuleAt( parseRuleAt( text ) ) {}


    ZoneEntry::ZoneEntry( string_view stdoff,
        string_view                   rules,
        string_view                   format,
        string_view                   until )

    : stdoff( stdoff )
    , rules( parseZoneRule( rules ) )
    , format( parseZoneFormat( format ) )
    , until( parseZoneUntil( until ) ) {}


    std::string format_as( ZoneUntil until ) {
        if( until.has_value() )
        {
            auto ymd = toCivil( until.date );
            return fmt::format(
                "{:>4} {} {:>2} {}", ymd.year, ymd.mon(), ymd.day, until.at );
        }

        return "(none)";
    }


    size_t dumpActive( RuleEntry const* active,
        size_t                          activeCount,
        size_t                          year,
        RuleTrans*                      p ) {
        // Compute transitions for all active rules for the given year
        for( size_t i = 0; i < activeCount; i++ )
            p[i] = active[i].resolveTrans( year );

        // Ensure the added transitions are sorted by date
        std::sort( p, p + activeCount, RuleTrans::compareDate() );
        return activeCount;
    }


    namespace {
        [[nodiscard]] constexpr RuleEntry const* findNewlyActive( size_t year,
            RuleEntry const*                                             begin,
            RuleEntry const* end ) noexcept {
            RuleEntry const* cursor = begin;
            while( cursor != end && cursor->from == year ) ++cursor;
            return cursor;
        }

        [[nodiscard]] constexpr RuleEntry const* pullNewlyActive(
            vector<RuleEntry>& active,
            size_t             year,
            RuleEntry const*   begin,
            RuleEntry const*   end ) {
            auto cursor = findNewlyActive( year, begin, end );
            active.insert( active.end(), begin, cursor );
            return cursor;
        }

        [[nodiscard]] constexpr size_t nextExpiryYear(
            RuleEntry const* p, size_t size, size_t initial = -1 ) noexcept {
            for( size_t i = 0; i < size; i++ )
            {
                size_t expiry = size_t( p[i].to );
                initial       = std::min( initial, expiry );
            }
            return initial;
        }

        [[nodiscard]] constexpr size_t nextExpiryYear(
            vector<RuleEntry> const& active,
            size_t                   initialYear = -1 ) noexcept {
            return nextExpiryYear( active.data(), active.size(), initialYear );
        }

        [[nodiscard]] size_t fillTransitionTable( RuleEntry const* active,
            size_t                                                 activeCount,
            size_t                                                 year,
            size_t                                                 yearEnd,
            vector<RuleTrans>&                                     dest ) {
            // Number of new transitions we're adding
            size_t transCount = activeCount * ( yearEnd - year );

            size_t oldDestSize = dest.size();

            // Make sure there's enough space for all the new rules
            dest.resize( dest.size() + transCount );

            // This is where we're putting all the new rules
            RuleTrans* p = dest.data() + oldDestSize;

            for( ; year < yearEnd; ++year )
            {
                // Advance the destination pointer
                p += dumpActive( active, activeCount, year, p );
            }

            return yearEnd;
        }


        /// Fill the transition table with transitions pulled from the active
        /// ruleset. Resulting entries will be sorted according to the date of
        /// the transition.
        ///
        /// Transitions will be appended to the given dest.
        ///
        /// @return yearEnd
        [[nodiscard]] size_t fillTransitionTable(
            vector<RuleEntry> const& active,
            size_t                   year,
            size_t                   yearEnd,
            vector<RuleTrans>&       dest ) {
            return fillTransitionTable(
                active.data(), active.size(), year, yearEnd, dest );
        }
    } // namespace


    constexpr bool ruleNull( string_view rule ) noexcept {
        return rule.size() == 1 && rule[0] == '-';
    }
    constexpr bool ruleNumeric( string_view rule ) noexcept {
        char ch = rule.size() > 0 ? rule[0] : '\0';
        return ch == '+' || ch == '-' || isD10( ch - '0' );
    }

    struct UntilDate {
        sysdays_t    date; ///< Date, as days since epoch
        sysseconds_t T;    ///< DateTime when current state ends
    };

    struct ZTAgglomerator : ZoneStates {
      private:

        ZoneAbbr currentAbbr;
        i32      currentOff;
        i32      currentStdoff;


        map<ZoneAbbr, size_t> abbrLookup;

        AbbrBlock addAbbr( ZoneAbbr const& abbr ) {
            size_t& id = abbrLookup[abbr];
            // If it's a valid id, we just return (id - 1) to get the index
            if( id ) return AbbrBlock::make( id - 1, abbr.size_ );

            // Otherwise we set the id and update the table
            auto result = abbrTable_.size();
            id          = result + 1;
            abbrTable_.push_back( abbr );
            return AbbrBlock::make( result, abbr.size_ );
        }


      public:

        ZoneState current() const noexcept {
            return {
                FromUTC( currentStdoff ),
                FromUTC( currentOff ),
                currentAbbr,
            };
        }

        void setInitial( ZoneState state ) noexcept {
            abbrInitial_    = addAbbr( state.abbr );
            walloffInitial_ = state.walloff.off;
            stdoffInitial_  = state.stdoff.off;

            currentAbbr   = state.abbr;
            currentOff    = state.walloff.off;
            currentStdoff = state.stdoff.off;
        }

        void add( ZoneTransition const& trans ) {
            auto abbr   = trans.state.abbr;
            auto off    = trans.state.walloff.off;
            auto stdoff = trans.state.stdoff.off;
            auto when   = trans.when;

            if( abbr != currentAbbr )
            {
                currentAbbr = abbr;
                abbr_.push_back( addAbbr( abbr ) );
                abbrTrans_.push_back( when );
            }
            if( off != currentOff )
            {
                currentOff = off;
                walloff_.push_back( off );
                walloffTrans_.push_back( when );
            }
            if( stdoff != currentStdoff )
            {
                currentStdoff = stdoff;
                stdoff_.push_back( stdoff );
                stdoffTrans_.push_back( when );
            }
        }

        ZoneStates getStates( i64 safeCycleTime ) && noexcept {
            this->safeCycleTime = safeCycleTime;
            return static_cast<ZoneStates&&>( *this );
        }
    };

    static i64 getSafeCycleTime( span<ZoneEntry const> zoneEntries,
        map<string_view, RuleEvalResult> const&        ruleTable ) {
        // No entries, doesn't matter
        size_t n = zoneEntries.size();
        if( n == 0 ) return 0;

        auto const& lastRule = zoneEntries[n - 1].rules;

        sysdays_t lastRuleCycleDate;

        if( lastRule.isNamed() )
        {
            // If the last entry in the zone refers to a named rule, figure out
            // the date that rule becomes cyclic
            lastRuleCycleDate
                = resolveCivil( ruleTable.at( lastRule.name() ).yearEnd, 1, 1 );
        }
        else
        {
            // The rule does not become cyclic
            lastRuleCycleDate = 0;
        }


        // If there's only one rule, just return the last rule cycle date,
        // converted to seconds If this number is 0, that's fine, we don't care
        if( n == 1 ) { return daysToSeconds( lastRuleCycleDate ); }

        auto untilDate = zoneEntries[n - 2].until.date;

        auto cycleDate = std::max( {
            0,
            2 + untilDate,
            2 + lastRuleCycleDate,
        } );

        return daysToSeconds( cycleDate );
    }

    ZoneStates TZDataFile::getZoneStates(
        string_view name, i32 endYear ) const {
        // Represents a map of rule names to evaluated rules
        using RuleCache = map<string_view, ZoneTransIter>;

        sysseconds_t STOP_TIME = daysToSeconds( resolveCivil( endYear, 1, 1 ) );


        auto const& entries = zones.at( name );
        if( entries.empty() )
            throw std::runtime_error(
                fmt::format( "Error: zone '{}' contained no entries",
                    escapeString( name ) ) );


        auto rules = map<string_view, RuleEvalResult>( 16 );

        ZTAgglomerator agg;

        for( auto const& ent : entries )
        {
            auto const& rule = ent.rules;
            if( rule.isNamed() )
            {
                auto name = rule.name();
                if( !rules.contains( name ) )
                    rules.emplace( name, evaluateRules( name ) );
            }
        }

        auto cache = RuleCache( 16 );
        for( auto const& [k, rule] : rules ) { cache.emplace( k, rule ); }

        i64 safeCycleTime = getSafeCycleTime( entries, rules );

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
            if( rule.isHyphen() )
            {
                // We only have one entry in the zone, and also there is no
                // associated rule, so there are no state changes.
                return ZoneStates::makeStatic( { stdoff, format, "-" } );
            }

            if( rule.isOffset() )
            {
                // The rule only contains an offset. Again, no other entries, so
                // no state changes, so... it's a steady-state zone
                return ZoneStates::makeStatic(
                    { stdoff, rule.save(), format, "-" } );
            }

            auto& iter = cache.at( rule.name() );

            agg.setInitial( ZoneState{ stdoff, format, iter.currentLetter() } );

            while( auto const& maybeTrans
                   = iter.next( STOP_TIME, stdoff, format ) )
            {
                // As long as we have transitions, add them.
                agg.add( *maybeTrans );
            }

            return std::move( agg ).getStates( safeCycleTime );
        }

        sysseconds_t untilT;
        // Handle first rule

        {
            auto const& ent    = entries.front();
            auto const& rule   = ent.rules;
            auto const& stdoff = ent.stdoff;
            auto const& format = ent.format;
            auto const& until  = ent.until;
            if( rule.isHyphen() )
            {
                agg.setInitial( { stdoff, format, "-" } );
                untilT = until.resolve( agg.initial() );
            }
            else if( rule.isOffset() )
            {
                agg.setInitial( { stdoff, rule.save(), format, "-" } );
                untilT = until.resolve( agg.initial() );
            }
            else
            {
                auto& iter = cache.at( rule.name() );

                agg.setInitial( { stdoff, format, iter.currentLetter() } );
                untilT = until.resolve( agg.initial() );

                while( auto const& maybeTrans
                       = iter.next( untilT, stdoff, format ) )
                {
                    auto const& trans = *maybeTrans;
                    agg.add( trans );
                    untilT = until.resolve( trans.state );
                }
            }
        }

        for( size_t i = 1; i < entries.size(); i++ )
        {
            auto const& ent    = entries[i];
            auto const& rule   = ent.rules;
            auto const& stdoff = ent.stdoff;
            auto const& format = ent.format;
            auto        until  = ent.until;

            if( !until.has_value() )
            {
                // We're going from here, until the end year
                until = ZoneUntil{ resolveCivil( endYear, 1, 1 ) };
            }

            if( rule.isHyphen() )
            {
                auto state = ZoneState{ stdoff, format, "-" };
                agg.add( { untilT, state } );
                untilT = until.resolve( state );
            }
            else if( rule.isOffset() )
            {
                auto state = ZoneState{ stdoff, rule.save(), format, "-" };
                agg.add( { untilT, state } );
                untilT = until.resolve( state );
            }
            else
            {
                auto& iter = cache.at( rule.name() );
                auto  state
                    = iter.advanceTo( untilT, stdoff, format, agg.current() );
                agg.add( { untilT, state } );
                untilT = until.resolve( state );

                while( auto const& maybeTrans
                       = iter.next( untilT, stdoff, format ) )
                {
                    auto const& trans = *maybeTrans;
                    agg.add( trans );
                    untilT = until.resolve( trans.state );
                }
            }
        }

        return std::move( agg ).getStates( safeCycleTime );
    }


    RuleEvalResult evaluateRules(
        RuleEntry const* begin, RuleEntry const* end ) {
        if( begin == end )
            throw std::runtime_error(
                "evaluateRules(): Error: given empty rule set" );

        if( !std::is_sorted( begin, end, RuleEntry::compareFrom() ) )
            throw std::runtime_error(
                "Expected Rule to be sorted by 'FROM' year" );

        auto active = vector<RuleEntry>(); ///< Current set of active rules
        auto tt     = vector<RuleTrans>(); ///< Computed Transition Table

        size_t year0 = begin->from;        ///< First year with a rule

        size_t year   = year0;             ///< Cursor holding the current year
        auto   cursor = begin;             ///< Cursor holding the next rule

        // Get the initial set of active rules
        cursor = pullNewlyActive( active, year, cursor, end );

        // We're going to loop as long as there are rules that can expire
        while( cursor != end )
        {
            // Figure out how long we can loop for. We need to stop when we
            // either hit an expiry, or when a new rule is going to be added
            auto endYear = nextExpiryYear( active, cursor->from - 1 ) + 1;

            // Fill rules from the current year to the stop year
            // (we add 1 to the stop year, because we want to include it)
            year = fillTransitionTable( active, year, endYear, tt );

            // Remove any rules that have expired
            active.erase( std::remove_if( active.begin(),
                              active.end(),
                              RuleEntry::isExpired( year ) ),
                active.end() );

            // We have no active rules right now, so we should jump forward
            // to the next year that has a rule.
            //
            // We know that cursor->from >= year, because
            // `nextExpiryYear( ..., initial ) <= initial`
            if( active.empty() ) year = cursor->from;

            // Pull any rules that are newly active
            cursor = pullNewlyActive( active, year, cursor, end );
        }

        static_assert( Y_MAX == -1 );
        static_assert( nextExpiryYear( nullptr, 0 ) == -1 );

        // We want to pull any remaining rules that have a fixed expiry date.
        //
        // When only rules with no expiry remain (or there are no more active
        // rules), `nextExpiryYear( active )` will return -1, so endYear will be
        // 0, so we will break out of the loop.
        while( auto endYear = nextExpiryYear( active ) + 1 )
        {
            year = fillTransitionTable( active, year, endYear, tt );

            // Remove any rules that have expired
            active.erase( std::remove_if( active.begin(),
                              active.end(),
                              RuleEntry::isExpired( year ) ),
                active.end() );
        }

        return {
            std::move( tt ),
            std::move( active ),
            i32( year0 ),
            i32( year ),
        };
    }

    RuleEvalResult evaluateRules( vector<RuleEntry> const& rules ) {
        return evaluateRules( rules.data(), rules.data() + rules.size() );
    }

    string RuleTrans::str() const {
        return fmt::format( "{} @ {} SAVE={} LETTER='{}'",
            toCivil( date ),
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
        case OFFSET: return toHHMMSS( offset() );
        }
    }
    vector<ZoneTransition> ZoneStates::getTransitions() const {
        constexpr static sysseconds_t MAX_TIME
            = std::numeric_limits<sysseconds_t>::max();

        span stt = stdoffTrans_;
        span wtt = walloffTrans_;
        span rtt = abbrTrans_;

        span svv = stdoff_;
        span wvv = walloff_;
        span rvv = abbr_;

        size_t si = 0;
        size_t wi = 0;
        size_t ri = 0;

        auto                   sCurrent = stdoffInitial_;
        auto                   wCurrent = walloffInitial_;
        auto                   rCurrent = abbrInitial_;
        vector<ZoneTransition> result;
        for( ;; )
        {
            auto st = stt.value_or( si, MAX_TIME );
            auto wt = wtt.value_or( wi, MAX_TIME );
            auto rt = rtt.value_or( ri, MAX_TIME );

            auto T = std::min( { st, wt, rt } );
            if( T == MAX_TIME ) break;

            sCurrent = svv.value_or( si, sCurrent );
            wCurrent = wvv.value_or( wi, wCurrent );
            rCurrent = rvv.value_or( ri, rCurrent );

            // Update each index as needed
            si += int( st == T );
            wi += int( wt == T );
            ri += int( rt == T );

            result.push_back( ZoneTransition{
                T,
                ZoneState{
                    FromUTC( sCurrent ),
                    FromUTC( wCurrent ),
                    abbrTable_[rCurrent.index()],
                },
            } );
        }

        return result;
    }
    std::string format_as( AbbrBlock b ) {
        return fmt::format( "(index: {}, size: {})", b.index(), b.size() );
    }
} // namespace vtz
