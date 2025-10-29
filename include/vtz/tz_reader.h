#pragma once

#include "vtz/span.h"
#include <algorithm>
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
#include <vtz/tz_reader/ZoneState.h>
#include <vtz/tz_reader/ZoneUntil.h>

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


    struct ZoneTransition {
        sysseconds_t when; // UTC time of change
        ZoneState    state;

        ZoneTransition() = default;
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

        /// Resolve the time when the rule would take effect, based on the
        /// current state of the zone
        constexpr sysseconds_t resolve( ZoneTime time ) const noexcept {
            return at.resolveAt( date, time );
        }

        /// Resolve the ZoneTransition when the rule would take effect, based
        /// on the current state of the zone.
        ///
        /// The rule will take effect based on the zone's _CURRENT_ stdoff or
        /// walloff, so we need to pass in the full ZoneTime object.
        constexpr ZoneTransition resolveTrans(
            ZoneTime time, ZoneFormat format ) const noexcept {
            return ZoneTransition{
                resolve( time ),
                ZoneState{
                    time.stdoff,
                    time.stdoff.save( save ),
                    format,
                    letter,
                },
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
    };


    size_t dumpActive( RuleEntry const* active,
        size_t                          activeCount,
        size_t                          year,
        RuleTrans*                      p );

    /// Return the earliest active rule with a save of 0. Returns nullptr
    /// if no active rule with a save of 0 is found
    inline RuleEntry const* earliestActiveWithSaveZero(
        RuleEntry const* active, size_t activeCount, i32 year ) noexcept {
        size_t    result       = -1;
        sysdays_t earliestDate = MAX_DAYS;
        for( size_t i = 0; i < activeCount; ++i )
        {
            if( active[i].save.save != 0 ) continue;
            auto date = active[i].resolveDate( year );
            if( date <= earliestDate )
            {
                earliestDate = date;
                result       = i;
            }
        }
        return result < activeCount  // did we find an entry?
                   ? active + result // yes: return pointer to entry
                   : nullptr         // no: return nullptr
            ;
    }

    /// Implements a State Machine such that we can pull rule transitions
    struct RuleTransIter {
        /// Index (if iterating through historical transitions, holds index into
        /// historical buffer. If iterating through active transitions, holds
        /// index into active buffer.)
        size_t i;

        /// true if we're still pulling historical rules
        bool inHistorical;

        /// Holds the current year when we're iterating through active entries.
        /// Initialized to the first active year.
        i32 year;

        /// Historical entries buffer
        RuleTrans const* hist_;
        size_t           histSize_;

        /// Rules which are active going forward into perpetuity
        RuleEntry const* active_;
        size_t           activeSize_;

        /// Saves the first active year (for record keeping purposes, eg when
        /// it's necessary to retrieve the initial letter)
        i32 firstActiveYear_;

        /// Evaluated transitions from active entries buffer
        std::unique_ptr<RuleTrans[]> activeBuff;

        RuleTransIter( RuleTrans const* hist,
            size_t                      histSize,
            RuleEntry const*            active,
            size_t                      activeSize,
            int                         firstActiveYear ) noexcept
        : i( 0 )
        , inHistorical( true )
        , year( firstActiveYear )
        , hist_( hist )
        , histSize_( histSize )
        , active_( active )
        , activeSize_( activeSize )
        , firstActiveYear_( firstActiveYear ) {}

        RuleTransIter( RuleEvalResult const& r ) noexcept
        : RuleTransIter{
            r.historical.data(),
            r.historical.size(),
            r.active.data(),
            r.active.size(),
            r.yearEnd,
        } {}


        /// From 'How to Read the tz Database Source Files':
        /// > If switching to a named rule before any transition has happened,
        /// > assume standard time (SAVE zero), and use the LETTER data from
        /// > the earliest transition with a SAVE of zero.
        ///
        /// This obtains the initial letter, corresponding to the first entry
        /// with a SAVE of 0.
        ///
        /// Returns `RuleLetter("-")` if there are no entries with a save of 0
        RuleLetter initialLetter() const noexcept {
            for( size_t i = 0; i < histSize_; ++i )
            {
                if( hist_[i].save.save == 0 ) { return hist_[i].letter; }
            }
            /// NOTE: we are living in a state of sin by doing this... active
            /// transitions are not necessarily sorted by the one we enter
            /// earliest
            if( auto* result = earliestActiveWithSaveZero(
                    active_, activeSize_, firstActiveYear_ ) )
                return result->letter;

            return RuleLetter( "-" );
        }


        std::optional<RuleTrans> next() {
            if( inHistorical )
            {
                // If we have historical rules, then we can just pull from that
                if( i < histSize_ ) return hist_[i++];
                i            = activeSize_;
                inHistorical = false;
                activeBuff.reset( new RuleTrans[activeSize_] );
            }
            // If index == activeSize_, we need to refill the 'active' buffer
            // so that we have more values to pull from
            if( i == activeSize_ )
            {
                if( activeSize_ == 0 )
                {
                    // Because there are no active entries, there are no more
                    // transitions in this rule.
                    return std::nullopt;
                }

                dumpActive( active_, activeSize_, year++, activeBuff.get() );
                i = 0;
            }
            return activeBuff[i++];
        }
    };


    class ZoneTransIter {
        RuleTransIter ruleIter_;
        RuleTrans     next_;
        RuleLetter    currentLetter_;
        RuleSave      currentSave_;
        bool          isDone;

      public:

        ZoneTransIter( RuleEvalResult const& r )
        : ruleIter_( r )
        , currentLetter_( ruleIter_.initialLetter() )
        , currentSave_( 0 )
        , isDone( false ) {
            if( auto next = ruleIter_.next() )
            {
                // We need to keep track of the last transition state
                next_ = *next;
            }
            else
            {
                // A rule without any entries is erroneous
                throw std::runtime_error( "Rule contained no entries" );
            }
        }

        /// Advance the iterator until the next state is strictly after the
        /// current state.
        ///
        /// Return the current state - the one prescribed by the rule at the
        /// given input time
        [[nodiscard]] ZoneState advanceTo( sysseconds_t when,
            FromUTC                                     stdoff,
            ZoneFormat const&                           format,
            ZoneTime                                    oldTime ) {
            // We are going to advance until the next state is strictly after
            // 'when'
            for( ;; )
            {
                // The current wall offset is determined by the current save.
                // We need this to determine the time when the next transition
                // will occur.
                auto walloff = stdoff.save( currentSave_ );

                auto nextTransTime = next_.resolve( oldTime );
                if( nextTransTime > when ) break;

                // Update the current state
                currentSave_   = next_.save;
                currentLetter_ = next_.letter;

                // Try to advance
                if( auto x = ruleIter_.next() )
                {
                    // We have a value; we can update 'next_'
                    next_ = *x;
                }
                else
                {
                    // We're done - no more values
                    isDone = true;
                    break;
                }
            }
            // Return the current state
            return ZoneState( stdoff, currentSave_, format, currentLetter_ );
        }

        RuleSave   currentSave() const noexcept { return currentSave_; }
        RuleLetter currentLetter() const noexcept { return currentLetter_; }

        /// Return the next ZoneTransition, if one exists prior to the untilTime
        std::optional<ZoneTransition> next(
            sysseconds_t untilTime, FromUTC stdoff, ZoneFormat const& format ) {
            // Record the current walloff. This is needed because the time
            // when the next transition will occur will be in terms of the
            // current walloff.
            FromUTC walloff = stdoff.save( currentSave_ );

            // If the next time would be after the untilTime, we don't have a
            // value. Do not update the iterator
            if( next_.resolve( { stdoff, walloff } ) >= untilTime )
                return std::nullopt;

            // Update the current save and letter
            currentSave_   = next_.save;
            currentLetter_ = next_.letter;

            // We will be returning 'next' as the result, and then updating
            // 'next_'
            auto tmp = next_;

            if( auto x = ruleIter_.next() )
            {
                // We have a value, so: we can update next_ to be that value
                next_ = *x;
            }
            else
            {
                // Mark ourselves as done
                if( isDone ) { return std::nullopt; }
                isDone = true;
            }
            return tmp.resolveTrans( { stdoff, walloff }, format );
        }
    };


    RuleEvalResult evaluateRules( vector<RuleEntry> const& entries );

    struct AbbrBlock {
        u32 data_;

        constexpr size_t index() const noexcept { return data_ >> 4; }
        constexpr size_t size() const noexcept { return data_ & 0xf; }

        static AbbrBlock make( size_t i, size_t s ) noexcept {
            return { u32( i ) << 4 | u32( s & 0xf ) };
        }
    };

    struct ZoneStates {
        /// Return the _last_ value less than or equal to 'when'.
        /// Return -1 if no such value exists
        static ptrdiff_t _find( span<i64 const> s, i64 when ) {
            auto it = std::upper_bound( s.begin(), s.end(), when );
            return ( it - s.begin() ) - 1;
        }

        sysseconds_t     safeCycleTime;
        vector<ZoneAbbr> abbrTable_;

        AbbrBlock abbrInitial_;
        i32       walloffInitial_;
        i32       stdoffInitial_;

        vector<sysseconds_t> abbrTrans_;
        vector<AbbrBlock>    abbr_;

        vector<sysseconds_t> walloffTrans_;
        vector<i32>          walloff_;

        vector<sysseconds_t> stdoffTrans_;
        vector<i32>          stdoff_;


        vector<ZoneTransition> getTransitions() const;

        ZoneState initial() const noexcept {
            return {
                FromUTC( stdoffInitial_ ),
                FromUTC( walloffInitial_ ),
                abbrTable_[abbrInitial_.index()],
            };
        }

        i32 const& stdoff( sysseconds_t t ) const noexcept {
            auto i = _find( stdoffTrans_, t );
            return i >= 0 ? stdoff_[i] : stdoffInitial_;
        }

        i32 const& walloff( sysseconds_t t ) const noexcept {
            auto i = _find( walloffTrans_, t );
            return i >= 0 ? walloff_[i] : walloffInitial_;
        }

        AbbrBlock const& abbr( sysseconds_t t ) const noexcept {
            auto i = _find( abbrTrans_, t );
            return i >= 0 ? abbr_[i] : abbrInitial_;
        }


        ZoneState getState( sysseconds_t t ) const {
            return {
                FromUTC( stdoff( t ) ),
                FromUTC( walloff( t ) ),
                abbrTable_[abbr( t ).index()],
            };
        }

        string formatLocal( sysseconds_t time ) const {
            auto const& state = getState( time );
            return localToString( time, state.walloff, state.abbr );
        }

        static ZoneStates makeStatic( ZoneState state ) {
            return {
                0,
                { state.abbr },
                AbbrBlock::make( 0, state.abbr.size() ),
                state.walloff.off,
                state.stdoff.off,
            };
        }
    };

    struct TZDataFile {
        RuleMap rules;
        ZoneMap zones;
        LinkMap links;

        RuleEvalResult evaluateRules( string_view rule ) const {
            return vtz::evaluateRules( rules.at( rule ) );
        }
        ZoneStates getZoneStates( string_view name, i32 endYear ) const;

        bool operator==( TZDataFile const& rhs ) const noexcept {
            return rules == rhs.rules    //
                   && zones == rhs.zones //
                   && links == rhs.links;
        }

        /// List all zones by name
        vector<string> listZones() const {
            vector<string> result( zones.size() );
            size_t         i = 0;
            for( auto const& [name, _] : zones ) result[i++] = string( name );
            return result;
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
