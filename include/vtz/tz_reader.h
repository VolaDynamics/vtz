#pragma once

#include <vtz/strings.h>

namespace vtz {
    // # Rule	NAME	FROM	TO	-	IN	ON	AT	SAVE
    // LETTER Rule	CA	1948	only	-	Mar	14	2:01
    // 1:00	D Rule	CA	1949	only	-	Jan	 1	2:00
    // 0	S Rule	CA	1950	1966	-	Apr	lastSun	1:00
    // 1:00	D
    struct raw_rule
    {
        string_view name;
        string_view from;
        string_view to;
        string_view in;
        string_view on;
        string_view at;
        string_view save;
        string_view letter;

        bool operator==( raw_rule const& rhs ) const noexcept
        {
            return name == rhs.name    //
                   && from == rhs.from //
                   && to == rhs.to     //
                   && in == rhs.in     //
                   && on == rhs.on     //
                   && at == rhs.at     //
                   && save == rhs.save //
                   && letter == rhs.letter;
        }
    };


    struct raw_link
    {
        string_view canonical;
        string_view alias;
    };

    // # Zone	NAME		STDOFF	RULES	FORMAT	[UNTIL]
    // Zone America/Los_Angeles -7:52:58 -	LMT	1883 Nov 18 20:00u
    //             -8:00	US	P%sT	1946
    //             -8:00	CA	P%sT	1967
    //             -8:00	US	P%sT

    struct raw_zone_entry
    {
        string_view stdoff;
        string_view rules;
        string_view format;
        string_view until;

        bool operator==( raw_zone_entry const& rhs ) const noexcept
        {
            return stdoff == rhs.stdoff    //
                   && rules == rhs.rules   //
                   && format == rhs.format //
                   && until == rhs.until;
        }
    };

    struct raw_zone
    {
        string_view            name;
        vector<raw_zone_entry> ents;

        bool operator==( raw_zone const& rhs ) const noexcept
        {
            return name == rhs.name && ents == rhs.ents;
        }
    };


    struct raw_tzdata_file
    {
        vector<raw_rule> rules;
        vector<raw_zone> zones;
        vector<raw_link> links;

        bool operator==( raw_tzdata_file const& rhs ) const noexcept
        {
            return rules == rhs.rules && zones == rhs.zones;
        }
    };

    raw_rule parse_rule( token_iter tok_iter );

    raw_zone_entry parse_zone_entry( token_iter tok_iter );

    void parse_zone_or_rule(
        raw_tzdata_file& file, string_view line, line_iter& lines );

    inline raw_tzdata_file parse_tzdata( string_view input )
    {
        auto            lines = line_iter( input );
        raw_tzdata_file file;
        while( auto line = lines.next() )
        {
            parse_zone_or_rule( file, line, lines );
        }
        return file;
    }
} // namespace vtz
