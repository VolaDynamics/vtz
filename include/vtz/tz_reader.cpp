#include <vtz/dates.h>

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
        OptTok      token;
    };

    namespace {
        Mon parseMonth( OptTok tok ) {
            if( tok.size() == 3 )
            {
                auto result = _impl::_parseMon( tok.data() );
                if( result != _impl::MONTH_BAD ) return result;
            }

            throw ParseError{
                "Expected month",
                tok.has_value() ? "is not a valid month name"
                                : "month is missing",
                tok,
            };
        }

        rule_year_t parseYear( OptTok tok ) {
            i16         year{};
            char const* begin = tok.data();
            char const* end   = tok.data() + tok.size();

            auto result = std::from_chars( begin, end, year );
            if( result.ec == std::errc{} && result.ptr == end && year > 0 )
                return rule_year_t( year );

            throw ParseError{
                "Expected year of form 'YYYY'",
                tok.has_value() ? "is not a valid year" : "year is missing",
                tok,
            };
        }

        rule_year_t parseYearTo( OptTok tok ) {
            if( tok == "max" ) { return Y_MAX; }
            if( tok == "only" ) { return Y_ONLY; }
            i16         year{};
            char const* begin = tok.data();
            char const* end   = tok.data() + tok.size();

            auto result = std::from_chars( begin, end, year );
            if( result.ec == std::errc{} && result.ptr == end && year > 0 )
                return rule_year_t( year );

            throw ParseError{
                "Expected year of form 'YYYY' or literal strings 'max' or "
                "'only'",
                tok.has_value() ? "is not a valid year" : "year is missing",
                tok,
            };
        }

        RuleOn parseRuleOn( OptTok tok ) {
            using namespace _impl;

            size_t      size = tok.size();
            char const* p    = tok.data();
            char const* end_ = p + size;

            if( size == 7 )
            {
                constexpr auto _last = _load4( "last" );
                if( _load4( p ) == _last )
                {
                    DOW dow = _parseDOW( p + 4 );
                    if( dow != DOW_BAD ) { return RuleOn::last( dow ); }
                }
            }

            if( size >= 6 )
            {
                // If size >= 6, Input should be of form [DOW]>=[value]
                DOW dow = _parseDOW( p );

                u8 day{};

                auto result = std::from_chars( p + 5, end_, day );

                // this should be either '>=' or '<='
                auto GE_or_LE = _load2( p + 3 );

                // true if 'DOW' is good and the 'DOM' is good
                bool goodDOW = dow != _impl::DOW_BAD; // check DOW is valid
                bool goodDOM
                    = day
                      && day < 32 // check that day is a valid day of the month
                      && result.ptr == end_;

                if( goodDOW && goodDOM )
                {
                    switch( GE_or_LE )
                    {
                    case _load2( ">=" ): return RuleOn::after( day, dow );
                    case _load2( "<=" ): return RuleOn::before( day, dow );
                    default: break;
                    }
                }
            }
            else if( size <= 2 )
            {
                u8   day{};
                auto result = std::from_chars( p, end_, day );
                bool isGood = day && day < 32 && result.ptr == end_;
                if( isGood ) { return RuleOn::on( day ); }
            }

            throw ParseError{ "Expected rule describing when within the month "
                              "a transition occurs",
                tok.has_value()
                    ? "could not be parsed. Valid forms are: 'last[DOW]', "
                      "'[DOW]>=[DOM]', '[DOW]<=[DOM]', or '[DOM]', where "
                      "'[DOW]' represents "
                      "Sun, Mon, Tue, Wed, Thu, Fri, or Sat, and DOM is an "
                      "integer 1-31."
                    : "token was missing",
                tok };
        }

        constexpr inline bool isD10( int x ) noexcept {
            return 0 <= x && x < 10;
        }
        constexpr inline bool isD6( int x ) noexcept { return 0 <= x && x < 6; }


        constexpr i32 _hms( i32 h, i32 m = 0, i32 s = 0 ) noexcept {
            return h * 3600 + m * 60 + s;
        }

    } // namespace

    i32 parseHHMMSSOffset( char const* p, size_t size, int sign ) noexcept {
        switch( size )
        {
        // Single digit (represents hour)
        case 1:
            {
                i32 H0 = p[0] - '0';
                if( isD10( H0 ) ) return sign * _hms( H0 );
                return OFFSET_NPOS;
            }
        // Two digits (represents hour)
        case 2:
            {
                i32 H0 = p[0] - '0';
                i32 H1 = p[1] - '0';
                if( isD10( H0 ) && isD10( H1 ) )
                    return sign * _hms( H0 * 10 + H1 );
                return OFFSET_NPOS;
            }
        // H:MM
        case 4:
            {
                i32 H0 = p[0] - '0';
                if( p[1] != ':' ) break;
                i32 M0 = p[2] - '0';
                i32 M1 = p[3] - '0';
                if( isD10( H0 ) && isD6( M0 ) && isD10( M1 ) )
                    return sign * _hms( H0, M0 * 10 + M1 );
                return OFFSET_NPOS;
            }
        // HH:MM
        case 5:
            {
                i32 H0 = p[0] - '0';
                i32 H1 = p[1] - '0';
                if( p[2] != ':' ) break;
                i32 M0 = p[3] - '0';
                i32 M1 = p[4] - '0';
                if( isD10( H0 ) && isD10( H1 ) && isD6( M0 ) && isD10( M1 ) )
                    return sign * _hms( H0 * 10 + H1, M0 * 10 + M1 );
                return OFFSET_NPOS;
            }
        // H:MM:SS
        case 7:
            {
                i32 H0 = p[0] - '0';
                if( p[1] != ':' ) break;
                i32 M0 = p[2] - '0';
                i32 M1 = p[3] - '0';
                if( p[4] != ':' ) break;
                i32 S0 = p[5] - '0';
                i32 S1 = p[6] - '0';
                if( isD10( H0 )    //
                    && isD6( M0 )  //
                    && isD10( M1 ) //
                    && isD6( S0 )  //
                    && isD10( S1 ) //
                )
                    return sign * _hms( H0, M0 * 10 + M1, S0 * 10 + S1 );
                return OFFSET_NPOS;
            }
        // HH:MM:SS
        case 8:
            {
                if( p[2] != ':' ) break;
                if( p[5] != ':' ) break;
                i32 H0 = p[0] - '0';
                i32 H1 = p[1] - '0';
                i32 M0 = p[3] - '0';
                i32 M1 = p[4] - '0';
                i32 S0 = p[6] - '0';
                i32 S1 = p[7] - '0';
                if( isD10( H0 )    //
                    && isD10( H1 ) //
                    && isD6( M0 )  //
                    && isD10( M1 ) //
                    && isD6( S0 )  //
                    && isD10( S1 ) //
                )
                    return sign
                           * _hms( H0 * 10 + H1, M0 * 10 + M1, S0 * 10 + S1 );
                return OFFSET_NPOS;
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

        constexpr char const* bad_LETTER
            = "is too large to be valid for the LETTER/S field.";

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

        ZoneOff parseZoneOff( OptTok tok ) {
            size_t      size = tok.size();
            char const* p    = tok.data();

            auto result = parseSignedHHMMSSOffset( p, size );
            if( result != OFFSET_NPOS ) return ZoneOff{ result };

            // throw failure
            throw ParseError{
                "Expected STDOFF offset", tok ? bad_STDOFF : no_token, tok
            };
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
        e.until  = stripTrailingDelim( tok_iter.rest() );
        return e;
    }

    Rule parseRule( TokenIter tok_iter ) {
        return Rule{
            tok_iter.next(),
            parseYear( tok_iter.next() ),
            /// For the 'TO' year, the values 'only' and 'max' must also be
            /// accepted
            parseYearTo( tok_iter.next() ),
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

    void parseEntry( TZDataFile& file, string_view line, LineIter& lines ) {
        line = stripComment( line );
        if( line.empty() ) return;

        auto tok_iter = TokenIter( line );
        // Either 'Zone' or 'Rule'
        auto what = tok_iter.next();
        if( !what.has_value() ) return;
        if( what == "Rule" )
        {
            file.rules.push_back( parseRule( tok_iter ) );
            return;
        }
        if( what == "Link" )
        {
            file.links.push_back( parseLink( tok_iter ) );
            return;
        }

        if( what == "Zone" )
        {
            Zone z;
            z.name = tok_iter.next();
            z.ents.push_back( parseZoneEntry( tok_iter ) );
            for( ;; )
            {
                // Get the next char of the next line. We're going to
                // inspect it to see if we're still in a zone
                auto next_ch = firstOrNil( lines.rest() );
                if( next_ch == '#' )
                {
                    (void)lines.next();
                    continue;
                }
                if( isDelim( next_ch ) )
                {
                    line = lines.next();
                    line = stripComment( line );
                    line = stripLeadingDelim( line );
                    if( line.empty() ) continue;
                    z.ents.push_back( parseZoneEntry( TokenIter( line ) ) );
                    continue;
                }
                break;
            }
            file.zones.push_back( z );
            return;
        }

        throw std::runtime_error( fmt::format( "Unable to parse '{}'", line ) );
    }

    TZDataFile parseTZData( string_view input, string_view filename ) {
        try
        {
            auto       lines = LineIter( input );
            TZDataFile file;
            while( auto line = lines.next() )
            { parseEntry( file, line, lines ); }
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


    std::string format_as( RuleOn r ) {
        switch( r.kind() )
        {
        case RuleOn::DAY: return fmt::format( "{}", r.day() );
        case RuleOn::DOW_BEFORE:
            return fmt::format( "{}<={}", r.dow(), r.day() );
        case RuleOn::DOW_AFTER:
            return fmt::format( "{}>={}", r.dow(), r.day() );
        case RuleOn::DOW_LAST: return fmt::format( "last{}", r.dow() );
        }
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

    std::string format_as( ZoneOff off ) { return toHHMMSS( off.offset ); }


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
} // namespace vtz
