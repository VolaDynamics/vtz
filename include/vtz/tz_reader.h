#pragma once

#include <vtz/bit.h>
#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/inplace_optional.h>
#include <vtz/strings.h>

#include <vtz/tz_reader/FromUTC.h>
#include <vtz/tz_reader/RuleAt.h>
#include <vtz/tz_reader/RuleLetter.h>
#include <vtz/tz_reader/RuleOn.h>
#include <vtz/tz_reader/RuleSave.h>
#include <vtz/tz_reader/ZoneFormat.h>
#include <vtz/tz_reader/ZoneRule.h>

#include <ankerl/unordered_dense.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>


namespace vtz {
    using ankerl::unordered_dense::map;
    using std::string;

    using rule_year_t           = i16;
    constexpr rule_year_t Y_MAX = -1;
    static_assert( size_t( Y_MAX ) == ~size_t() );

    constexpr i32 OFFSET_NPOS = INT32_MIN;

    struct ParseError {
        char const* expected; ///< What was expected
        char const* but;      ///< Reason why input was bad
        OptTok      token;    ///< token where failure occurred
    };


    struct alignas( 8 ) ZoneState {
        FromUTC    offset; // offset from UTC
        FixStr<11> abbr;

        bool operator==( ZoneState const& rhs ) const noexcept {
            return B16( *this ) == B16( rhs );
        }
        bool operator!=( ZoneState const& rhs ) const noexcept {
            return B16( *this ) != B16( rhs );
        }
    };


    struct ZoneTransition {
        sysseconds_t when; // UTC time of change
        ZoneState    state;

        constexpr ZoneTransition( i64 when, ZoneState state ) noexcept
        : when( when )
        , state( state ) {}

        constexpr ZoneTransition( NoneType ) noexcept
        : when( INT64_MAX )
        , state() {}

        bool     has_value() const noexcept { return when < INT64_MAX; }
        explicit operator bool() const noexcept { return when < INT64_MAX; }
    };


    struct RuleTrans {
        sysdays_t  date;
        RuleAt     at;
        RuleSave   save;
        RuleLetter letter;

        constexpr sysseconds_t resolve(
            FromUTC stdoff, FromUTC walloff ) const noexcept {
            return at.resolveAt( date, stdoff, walloff );
        }

        constexpr ZoneTransition resolveTrans(
            FromUTC stdoff, ZoneFormat format ) const noexcept {
            FromUTC walloff = stdoff.save( save );
            return ZoneTransition{
                resolve( stdoff, walloff ),
                ZoneState{ walloff,
                    format.format( date, save.save != 0, letter.sv() ) },
            };
        }

        string str() const;

        static RuleTrans fromCivil( int year,
            u32                         mon,
            u32                         day,
            RuleAt                      at,
            RuleSave                    save,
            RuleLetter                  letter ) {
            return RuleTrans{
                resolveCivil( year, mon, day ), at, save, letter
            };
        }

        constexpr bool operator==( RuleTrans const& rhs ) const noexcept {
            return date == rhs.date    //
                   && at == rhs.at     //
                   && save == rhs.save //
                   && letter == rhs.letter;
        }


        constexpr static auto compareDate() noexcept {
            return []( RuleTrans const& lhs, RuleTrans const& rhs ) {
                return lhs.date < rhs.date;
            };
        }
    };

    inline string format_as( RuleTrans const& rule ) { return rule.str(); }


    /// Example rules:
    ///
    /// #Rule NAME    FROM TO    -   IN  ON      AT   SAVE LETTER
    /// Rule  Chicago 1920 only  -   Jun 13      2:00 1:00 D
    /// Rule  Chicago 1920 1921  -   Oct lastSun 2:00 0    S
    /// Rule  Chicago 1921 only  -   Mar lastSun 2:00 1:00 D
    /// Rule  Chicago 1922 1966  -   Apr lastSun 2:00 1:00 D
    /// Rule  Chicago 1922 1954  -   Sep lastSun 2:00 0    S
    /// Rule  Chicago 1955 1966  -   Oct lastSun 2:00 0    S

    struct RuleEntry {
        rule_year_t from;
        rule_year_t to;
        Mon         in;
        RuleOn      on;
        RuleAt      at;
        RuleSave    save;
        RuleLetter  letter;

        /// Resolve the DateTime (in sysseconds_t)
        constexpr sysseconds_t resolveAt(
            i32 year, FromUTC stdoff, FromUTC walloff ) const noexcept {
            return at.resolveAt( on.resolveDate( year, in ), stdoff, walloff );
        }

        constexpr sysdays_t resolveDate( i32 year ) const noexcept {
            return on.resolveDate( year, in );
        }

        // Resolve a transition for the given year
        constexpr RuleTrans resolveTrans( i32 year ) const noexcept {
            return { resolveDate( year ), at, save, letter };
        }

        /// Returns true if the rule has expired by the given year
        constexpr bool expiresBefore( i64 year ) const noexcept {
            // When cast to u64, Y_MAX should be the max possible u64
            // This allows us to do this check without branching on whether or
            // not 'to' == Y_MAX
            static_assert( u64( rule_year_t( Y_MAX ) ) == ~u64() );
            return u64( year ) > u64( to );
        }

        bool operator==( RuleEntry const& rhs ) const noexcept {
            return from == rhs.from    //
                   && to == rhs.to     //
                   && in == rhs.in     //
                   && on == rhs.on     //
                   && at == rhs.at     //
                   && save == rhs.save //
                   && letter == rhs.letter;
        }

        constexpr static auto isExpired( i64 year ) noexcept {
            return [year]( RuleEntry const& rule ) {
                return rule.expiresBefore( year );
            };
        }


        constexpr static auto compareFrom() noexcept {
            return []( RuleEntry const& lhs, RuleEntry const& rhs ) {
                return lhs.from < rhs.from;
            };
        }

        constexpr static auto hasExpiry() noexcept {
            return []( RuleEntry const& rule ) { return rule.to != Y_MAX; };
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


    struct ZoneUntil {
        OptV<u16, 0> year; ///< year
        OptMon       mon;  ///< Month
        OptV<u8, 0>  day;  ///< Day of the month (1-31)
        OptRuleAt    at;   ///< Time of day when it ends


        /// Return the date referred to by this ZoneUntil
        constexpr sysdays_t resolveDate() const noexcept {
            return resolveCivil(
                *year, mon.value_or( Mon::Jan ), day.value_or( 1 ) );
        }

        /// Return the time this 'until' refers to
        constexpr sysseconds_t resolve(
            FromUTC stdoff, FromUTC walloff ) const noexcept {
            // Choose a default 'at' of 12:00 noon
            // TODO: check if this is correct
            constexpr RuleAt DEFAULT_AT
                = RuleAt::hhmmss( RuleAt::LOCAL_WALL, 12 );

            return at.value_or( DEFAULT_AT )
                .resolveAt( resolveDate(), stdoff, walloff );
        }

        u64 _repr() const noexcept {
            u64 result{};
            static_assert( sizeof( ZoneUntil ) == sizeof( u64 ) );
            _vtz_memcpy( &result, this, sizeof( u64 ) );
            return result;
        }

        bool operator==( ZoneUntil const& rhs ) const noexcept {
            return _repr() == rhs._repr();
        }

        constexpr bool has_value() const noexcept { return year.has_value(); }
    };

    string format_as( ZoneUntil );


    // # Zone	NAME		STDOFF	RULES	FORMAT	[UNTIL]
    // Zone America/Los_Angeles -7:52:58 -	LMT	1883 Nov 18 20:00u
    //             -8:00	US	P%sT	1946
    //             -8:00	CA	P%sT	1967
    //             -8:00	US	P%sT

    struct ZoneEntry {
        FromUTC    stdoff;
        ZoneUntil  until;
        ZoneRule   rules;
        ZoneFormat format;

        ZoneEntry() = default;

        constexpr ZoneEntry( FromUTC stdoff,
            ZoneRule                 rules,
            ZoneFormat               format,
            ZoneUntil                until ) noexcept
        : stdoff( stdoff )
        , until( until )
        , rules( rules )
        , format( format ) {}

        ZoneEntry( string_view stdoff,
            string_view        rules,
            string_view        format,
            string_view        until = {} );

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

    using RuleMap = map<string_view, vector<RuleEntry>>;
    using ZoneMap = map<string_view, vector<ZoneEntry>>;
    using LinkMap = map<string_view, string_view>;


    /// Get's the initial state, before any transition has happened.
    ///
    /// From 'How to Read the tz Database Source Files':
    /// > If switching to a named rule before any transition has happened,
    /// > assume standard time (SAVE zero), and use the LETTER data from
    /// > the earliest transition with a SAVE of zero.
    [[nodiscard]] inline std::optional<RuleLetter> getInitialLetter(
        RuleTrans const* trans, size_t transSize ) {
        for( size_t i = 0; i < transSize; ++i )
        {
            if( trans[i].save == 0 ) { return trans[i].letter; }
        }
        return std::nullopt;
    }

    /// Holds 4 pieces of information:
    ///
    /// - List of known transitions. These come from rules that have some
    /// finite end date (TO != max)
    /// - List of active rules. These are rules that will be active
    /// indefinitely, into the future (TO == max)
    /// - yearStart (year when historical transitions start)
    /// - yearEnd (year when historical transitions end)
    ///
    /// For instance, all rules up to 2007 are historical (TO != max),
    /// And rules from 2007 onwards apply indefinitely into the future
    /// (or until a glorious future government renders a judgement of
    /// permanent daylight savings time)
    ///
    /// ```
    /// Rule US 1918 1919 - Mar lastSun 2:00 1:00 D
    /// Rule US 1918 1919 - Oct lastSun 2:00 0    S
    /// ...
    /// Rule US 1987 2006 - Apr Sun>=1  2:00 1:00 D
    /// Rule US 2007 max  - Mar Sun>=8  2:00 1:00 D
    /// Rule US 2007 max  - Nov Sun>=1  2:00 0    S
    /// ```

    struct RuleEvalResult {
        /// Known transitions: contains all transitions up until the current
        /// active rule set
        vector<RuleTrans> historical;
        // Rule Entries that are still active after evaluation (these are
        // entries which have 'max' as the TO fields)
        vector<RuleEntry> active;
        // First year where there's a rule
        i32 yearStart;
        // Year where the historical rule transitions no longer apply
        // (and we only care about active rules)
        i32 yearEnd;

        /// Get's the initial state, before any transition has happened.
        ///
        /// From 'How to Read the tz Database Source Files':
        /// > If switching to a named rule before any transition has happened,
        /// > assume standard time (SAVE zero), and use the LETTER data from
        /// > the earliest transition with a SAVE of zero.
        ///
        /// This obtains the initial letter
        RuleLetter getInitialLetter() const noexcept {
            for( auto const& ent : historical )
            {
                if( ent.save == 0 ) { return ent.letter; }
            }

            for( auto const& rule : active )
            {
                if( rule.save == 0 ) { return rule.letter; }
            }

            // Indicates empty letter
            return RuleLetter();
        }
    };


    RuleEvalResult evaluateRules( vector<RuleEntry> const& entries );

    struct ZoneStates {
        ZoneState              initial;
        vector<ZoneTransition> transitions;
    };

    struct TZDataFile {
        RuleMap rules;
        ZoneMap zones;
        LinkMap links;

        RuleEvalResult evaluateRules( string_view rule ) const {
            return vtz::evaluateRules( rules.at( rule ) );
        }
        ZoneStates getZoneStates(
            string_view name, i32 startYear, i32 endYear ) const;

        bool operator==( TZDataFile const& rhs ) const noexcept {
            return rules == rhs.rules    //
                   && zones == rhs.zones //
                   && links == rhs.links;
        }
    };

    rule_year_t parseYear( OptTok tok );
    rule_year_t parseYearTo( OptTok tok );
    Mon         parseMonth( OptTok tok );
    u8          parseDayOfMonth( OptTok tok );
    RuleOn      parseRuleOn( OptTok tok );

    /// Parse a Zone Rule entry
    ///
    /// This can be either:
    /// - A '-', indicating that there is no associated rule
    /// - A named rule, such as 'US' or 'Indianapolis'
    /// - A offset, such as 1:00
    ZoneRule parseZoneRule( OptTok tok );

    /// Parse a zone format
    ZoneFormat parseZoneFormat( OptTok tok );

    /// Parse a zone entry
    ZoneEntry parseZoneEntry( TokenIter tok_iter );

    /// Parses either a rule, zone, or link
    void parseEntry( TZDataFile& file, string_view line, LineIter& lines );

    TZDataFile parseTZData(
        string_view input, string_view filename = "(none)" );


} // namespace vtz
