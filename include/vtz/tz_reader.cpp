#include <vtz/date_types.h>

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <vtz/strings.h>
#include <vtz/tz_reader.h>

#include <fmt/format.h>
#include <stdexcept>

namespace vtz {
    struct ParseError {
        char const* expected; ///< What was expected
        char const* but;      ///< Reason why input was bad
        OptTok      token;    ///< token where failure occurred
    };

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

        constexpr inline bool isD10( int x ) noexcept {
            return 0 <= x && x < 10;
        }
        constexpr inline bool isD6( int x ) noexcept { return 0 <= x && x < 6; }


        constexpr i32 _hms( i32 h, i32 m = 0, i32 s = 0 ) noexcept {
            return h * 3600 + m * 60 + s;
        }

        // Parse 1 digit
        TrivialOpt<i32> parse1( char const* src ) noexcept {
            i32 value = src[0] - '0';
            return { value, isD10( value ) };
        }

        // Parse 2 digits.
        TrivialOpt<i32> parse2( char const* src ) noexcept {
            i32 v0 = src[0] - '0';
            i32 v1 = src[1] - '0';
            return { v0 * 10 + v1, isD10( v0 ) && isD10( v1 ) };
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

    rule_year_t parseYearTo( OptTok tok, rule_year_t only ) {
        if( tok == "ma" || tok == "max" ) { return Y_MAX; }
        if( tok == "o" || tok == "only" ) { return only; }
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
        char const* p = tok.data();

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
        char const* end_ = p + size;

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
                        case '<': return RuleOn::before( day, *dow );
                        case '>': return RuleOn::after( day, *dow );
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
            size_t      size = tok.size();
            char const* p    = tok.data();
            if( size )
            {
                if( size < 8 )
                {
                    RuleLetter r{};
                    memcpy( r.buff_, p, size );
                    r.size_ = size;
                    return r;
                }
            }
            using S7 = FixStr<7>;

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

                        auto ymd = on.eval( y, m );
                        y        = ymd.year;
                        m        = ymd.mon();
                        u8 d     = u8( ymd.day );

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
                            return ZoneUntil{
                                y, m, d, at
                            }; // 'UNTIL' field given year, month, day, and
                               // STDOFF
                        }
                        return ZoneUntil{
                            y, m, d
                        }; // 'UNTIL' field given year, month, day
                    }
                    return ZoneUntil{ y,
                        m }; // 'UNTIL' field only given a year and month
                }
                return ZoneUntil{ y }; // 'UNTIL' field only given a year
            }
            return ZoneUntil{};        // 'UNTIL' field is empty; reached end of
                                       // Zone entry
        }
    } // namespace


    Link parseLink( TokenIter tok_iter ) {
        Link link;
        link.canonical = tok_iter.next();
        link.alias     = tok_iter.next();
        return link;
    }

    ZoneEntry parseZoneEntry( TokenIter tok_iter ) {
        ZoneEntry e;
        e.stdoff = parseZoneOff( tok_iter.next() );
        e.rules  = tok_iter.next();
        e.format = tok_iter.next();
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

    std::string RuleOn::string() const {
        switch( kind() )
        {
        case RuleOn::DAY: return fmt::format( "{}", day() );
        case RuleOn::DOW_BEFORE: return fmt::format( "{}<={}", dow(), day() );
        case RuleOn::DOW_AFTER: return fmt::format( "{}>={}", dow(), day() );
        case RuleOn::DOW_LAST: return fmt::format( "last{}", dow() );
        }

        throw std::runtime_error( "RuleOn::string(): bad kind()" );
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
    , rules( rules )
    , format( format )
    , until( parseZoneUntil( until ) ) {}


    std::string format_as( ZoneUntil until ) {
        if( until.year )
        {
            if( until.mon )
            {
                if( until.day )
                {
                    if( until.at )
                    {
                        return fmt::format( "{:>4} {} {:>2} {}",
                            *until.year,
                            *until.mon,
                            *until.day,
                            *until.at );
                    }
                    return fmt::format(
                        "{:>4} {} {:>2}", *until.year, *until.mon, *until.day );
                }
                return fmt::format( "{:>4} {}", *until.year, *until.mon );
            }
            return fmt::format( "{:>4}", *until.year );
        }
        return "(none)";
    }


    ZoneStates TZDataFile::getZoneStates(
        string_view name, i32 startYear, i32 endYear ) const {
        //
    }
} // namespace vtz
