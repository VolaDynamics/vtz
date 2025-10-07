#include <vtz/strings.h>
#include <vtz/tz_reader.h>

#include <fmt/format.h>
#include <stdexcept>

namespace vtz {

    void parse_zone_or_rule(
        raw_tzdata_file& file, string_view line, line_iter& lines )
    {
        line = strip_comment( line );
        if( line.empty() ) return;

        auto tok_iter = token_iter( line );
        // Either 'Zone' or 'Rule'
        auto what = tok_iter.next();
        if( !what.has_value() ) return;
        if( what == "Rule" )
        {
            file.rules.push_back( parse_rule( tok_iter ) );
            return;
        }

        if( what == "Zone" )
        {
            raw_zone z;
            z.name = tok_iter.next();
            z.ents.push_back( parse_zone_entry( tok_iter ) );
            for( ;; )
            {
                // Get the next char of the next line. We're going to
                // inspect it to see if we're still in a zone
                auto next_ch = first_or_nil( lines.peek_input() );
                if( next_ch == '#' )
                {
                    (void)lines.next();
                    continue;
                }
                if( is_delim( next_ch ) )
                {
                    line = lines.next();
                    line = strip_comment( line );
                    line = strip_leading_delim( line );
                    if( line.empty() ) continue;
                    z.ents.push_back( parse_zone_entry( token_iter( line ) ) );
                    continue;
                }
                break;
            }
            file.zones.push_back( z );
            return;
        }

        throw std::runtime_error( fmt::format( "Unable to parse '{}'", line ) );
    }

    raw_zone_entry parse_zone_entry( token_iter tok_iter )
    {
        raw_zone_entry e;
        e.stdoff = tok_iter.next();
        e.rules  = tok_iter.next();
        e.format = tok_iter.next();
        e.until  = strip_trailing_delim( tok_iter.rest() );
        return e;
    }

    raw_rule parse_rule( token_iter tok_iter )
    {
        raw_rule r;
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
} // namespace vtz
