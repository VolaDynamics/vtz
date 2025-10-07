#pragma once

#include <cstdint>
#include <vtz/strings.h>

namespace vtz {
    // # Rule	NAME	FROM	TO	-	IN	ON	AT	SAVE
    // LETTER Rule	CA	1948	only	-	Mar	14	2:01
    // 1:00	D Rule	CA	1949	only	-	Jan	 1	2:00
    // 0	S Rule	CA	1950	1966	-	Apr	lastSun	1:00
    // 1:00	D
    struct Rule {
        string_view name;
        string_view from;
        string_view to;
        string_view in;
        string_view on;
        string_view at;
        string_view save;
        string_view letter;

        bool operator==( Rule const& rhs ) const noexcept {
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


    struct Link {
        string_view canonical;
        string_view alias;

        bool operator==( Link rhs ) const noexcept {
            return canonical == rhs.canonical //
                   && alias == rhs.alias;
        }
    };

    // # Zone	NAME		STDOFF	RULES	FORMAT	[UNTIL]
    // Zone America/Los_Angeles -7:52:58 -	LMT	1883 Nov 18 20:00u
    //             -8:00	US	P%sT	1946
    //             -8:00	CA	P%sT	1967
    //             -8:00	US	P%sT

    struct ZoneEntry {
        string_view stdoff;
        string_view rules;
        string_view format;
        string_view until;

        bool operator==( ZoneEntry const& rhs ) const noexcept {
            return stdoff == rhs.stdoff    //
                   && rules == rhs.rules   //
                   && format == rhs.format //
                   && until == rhs.until;
        }
    };

    struct Zone {
        string_view       name;
        vector<ZoneEntry> ents;

        bool operator==( Zone const& rhs ) const noexcept {
            return name == rhs.name && ents == rhs.ents;
        }
    };


    struct TZDataFile {
        vector<Rule> rules;
        vector<Zone> zones;
        vector<Link> links;

        bool operator==( TZDataFile const& rhs ) const noexcept {
            return rules == rhs.rules    //
                   && zones == rhs.zones //
                   && links == rhs.links;
        }
    };

    /// Parse a rule
    Rule parseRule( TokenIter tok_iter );

    /// Parse a zone entry
    ZoneEntry parseZoneEntry( TokenIter tok_iter );

    /// Parses either a rule, zone, or link
    void parseEntry( TZDataFile& file, string_view line, LineIter& lines );

    TZDataFile parseTZData( string_view input );
} // namespace vtz
