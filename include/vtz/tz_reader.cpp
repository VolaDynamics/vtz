#include <vtz/strings.h>
#include <vtz/tz_reader.h>

#include <fmt/format.h>
#include <stdexcept>

namespace vtz {
    Link parseLink( TokenIter tok_iter ) {
        Link link;
        link.canonical = tok_iter.next();
        link.alias     = tok_iter.next();
        return link;
    }

    ZoneEntry parseZoneEntry( TokenIter tok_iter ) {
        ZoneEntry e;
        e.stdoff = tok_iter.next();
        e.rules  = tok_iter.next();
        e.format = tok_iter.next();
        e.until  = stripTrailingDelim( tok_iter.rest() );
        return e;
    }

    Rule parseRule( TokenIter tok_iter ) {
        Rule r;
        r.name = tok_iter.next();
        r.from = tok_iter.next();
        r.to   = tok_iter.next();
        tok_iter.next(); // '-'
        r.in     = tok_iter.next();
        r.on     = tok_iter.next();
        r.at     = tok_iter.next();
        r.save   = tok_iter.next();
        r.letter = tok_iter.next();
        return r;
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

    TZDataFile parseTZData( string_view input ) {
        auto       lines = LineIter( input );
        TZDataFile file;
        while( auto line = lines.next() ) { parseEntry( file, line, lines ); }
        return file;
    }
} // namespace vtz
