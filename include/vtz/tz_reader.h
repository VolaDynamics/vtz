#pragma once

#include <algorithm>
#include <utility>
#include <vtz/bit.h>
#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/files.h>
#include <vtz/inplace_optional.h>
#include <vtz/span.h>
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
            return at.resolve_at( date, time );
        }

        /// Resolve the ZoneTransition when the rule would take effect, based
        /// on the current state of the zone.
        ///
        /// The rule will take effect based on the zone's _CURRENT_ stdoff or
        /// walloff, so we need to pass in the full ZoneTime object.
        constexpr ZoneTransition resolve_trans(
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

        static RuleTrans from_civil( int year,
            u32                          mon,
            u32                          day,
            RuleAt                       at,
            RuleSave                     save,
            RuleLetter                   letter ) {
            return RuleTrans{
                resolve_civil( year, mon, day ), at, save, letter
            };
        }

        constexpr bool operator==( RuleTrans const& rhs ) const noexcept {
            return date == rhs.date    //
                   && at == rhs.at     //
                   && save == rhs.save //
                   && letter == rhs.letter;
        }


        constexpr static auto compare_date() noexcept {
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
        constexpr sysseconds_t resolve_at(
            i32 year, FromUTC stdoff, FromUTC walloff ) const noexcept {
            return at.resolve_at(
                on.resolve_date( year, in ), stdoff, walloff );
        }

        constexpr sysdays_t resolve_date( i32 year ) const noexcept {
            return on.resolve_date( year, in );
        }

        // Resolve a transition for the given year
        constexpr RuleTrans resolve_trans( i32 year ) const noexcept {
            return { resolve_date( year ), at, save, letter };
        }

        /// Returns true if the rule has expired by the given year
        constexpr bool expires_before( i64 year ) const noexcept {
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

        constexpr static auto is_expired( i64 year ) noexcept {
            return [year]( RuleEntry const& rule ) {
                return rule.expires_before( year );
            };
        }


        constexpr static auto compare_from() noexcept {
            return []( RuleEntry const& lhs, RuleEntry const& rhs ) {
                return lhs.from < rhs.from;
            };
        }

        constexpr static auto has_expiry() noexcept {
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
    [[nodiscard]] inline std::optional<RuleLetter> get_initial_letter(
        RuleTrans const* trans, size_t trans_size ) {
        for( size_t i = 0; i < trans_size; ++i )
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
    /// - year_start (year when historical transitions start)
    /// - year_end (year when historical transitions end)
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
        i32 year_start;
        // Year where the historical rule transitions no longer apply
        // (and we only care about active rules)
        i32 year_end;
    };


    size_t dump_active( RuleEntry const* active,
        size_t                           active_count,
        size_t                           year,
        RuleTrans*                       p );

    /// Return the earliest active rule with a save of 0. Returns nullptr
    /// if no active rule with a save of 0 is found
    inline RuleEntry const* earliest_active_with_save_zero(
        RuleEntry const* active, size_t active_count, i32 year ) noexcept {
        size_t    result       = -1;
        sysdays_t earliest_date = MAX_DAYS;
        for( size_t i = 0; i < active_count; ++i )
        {
            if( active[i].save.save != 0 ) continue;
            auto date = active[i].resolve_date( year );
            if( date <= earliest_date )
            {
                earliest_date = date;
                result       = i;
            }
        }
        return result < active_count // did we find an entry?
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
        bool in_historical;

        /// Holds the current year when we're iterating through active entries.
        /// Initialized to the first active year.
        i32 year;

        /// Historical entries buffer
        RuleTrans const* hist_;
        size_t           hist_size_;

        /// Rules which are active going forward into perpetuity
        RuleEntry const* active_;
        size_t           active_size_;

        /// Saves the first active year (for record keeping purposes, eg when
        /// it's necessary to retrieve the initial letter)
        i32 first_active_year_;

        /// Evaluated transitions from active entries buffer
        std::unique_ptr<RuleTrans[]> active_buff;

        RuleTransIter( RuleTrans const* hist,
            size_t                      hist_size,
            RuleEntry const*            active,
            size_t                      active_size,
            int                         first_active_year ) noexcept
        : i( 0 )
        , in_historical( true )
        , year( first_active_year )
        , hist_( hist )
        , hist_size_( hist_size )
        , active_( active )
        , active_size_( active_size )
        , first_active_year_( first_active_year ) {}

        RuleTransIter( RuleEvalResult const& r ) noexcept
        : RuleTransIter{
            r.historical.data(),
            r.historical.size(),
            r.active.data(),
            r.active.size(),
            r.year_end,
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
        RuleLetter initial_letter() const noexcept {
            for( size_t i = 0; i < hist_size_; ++i )
            {
                if( hist_[i].save.save == 0 ) { return hist_[i].letter; }
            }
            /// NOTE: we are living in a state of sin by doing this... active
            /// transitions are not necessarily sorted by the one we enter
            /// earliest
            if( auto* result = earliest_active_with_save_zero(
                    active_, active_size_, first_active_year_ ) )
                return result->letter;

            return RuleLetter( "-" );
        }


        std::optional<RuleTrans> next() {
            if( in_historical )
            {
                // If we have historical rules, then we can just pull from that
                if( i < hist_size_ ) return hist_[i++];
                i             = active_size_;
                in_historical = false;
                active_buff.reset( new RuleTrans[active_size_] );
            }
            // If index == active_size_, we need to refill the 'active' buffer
            // so that we have more values to pull from
            if( i == active_size_ )
            {
                if( active_size_ == 0 )
                {
                    // Because there are no active entries, there are no more
                    // transitions in this rule.
                    return std::nullopt;
                }

                dump_active( active_, active_size_, year++, active_buff.get() );
                i = 0;
            }
            return active_buff[i++];
        }
    };


    class ZoneTransIter {
        RuleTransIter rule_iter_;
        RuleTrans     next_;
        RuleLetter    current_letter_;
        RuleSave      current_save_;
        bool          is_done;

      public:

        ZoneTransIter( RuleEvalResult const& r )
        : rule_iter_( r )
        , current_letter_( rule_iter_.initial_letter() )
        , current_save_( 0 )
        , is_done( false ) {
            if( auto next = rule_iter_.next() )
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
        [[nodiscard]] ZoneState advance_to( sysseconds_t when,
            FromUTC                                      stdoff,
            ZoneFormat const&                            format,
            ZoneTime                                     old_time ) {
            // We are going to advance until the next state is strictly after
            // 'when'
            for( ;; )
            {
                // The next transition time is based on what the zone time was
                // previously

                auto next_trans_time = next_.resolve( old_time );

                // If the next transition has not yet occurred, we've found the
                // current state
                if( next_trans_time > when ) break;

                // Update the current state
                current_save_   = next_.save;
                current_letter_ = next_.letter;

                // Try to advance
                if( auto x = rule_iter_.next() )
                {
                    // We have a value; we can update 'next_'
                    next_ = *x;
                }
                else
                {
                    // We're done - no more values
                    is_done = true;
                    break;
                }
            }
            // Return the current state
            return ZoneState( stdoff, current_save_, format, current_letter_ );
        }

        RuleSave   current_save() const noexcept { return current_save_; }
        RuleLetter current_letter() const noexcept { return current_letter_; }

        /// Return the next ZoneTransition, if one exists prior to the
        /// until_time
        std::optional<ZoneTransition> next( sysseconds_t until_time,
            FromUTC                                      stdoff,
            ZoneFormat const&                            format ) {
            // Record the current walloff. This is needed because the time
            // when the next transition will occur will be in terms of the
            // current walloff.
            FromUTC walloff = stdoff.save( current_save_ );

            // If the next time would be after the until_time, we don't have a
            // value. Do not update the iterator
            if( next_.resolve( { stdoff, walloff } ) >= until_time )
                return std::nullopt;

            // Update the current save and letter
            current_save_   = next_.save;
            current_letter_ = next_.letter;

            // We will be returning 'next' as the result, and then updating
            // 'next_'
            auto tmp = next_;

            if( auto x = rule_iter_.next() )
            {
                // We have a value, so: we can update next_ to be that value
                next_ = *x;
            }
            else
            {
                // Mark ourselves as done
                if( is_done ) { return std::nullopt; }
                is_done = true;
            }
            return tmp.resolve_trans( { stdoff, walloff }, format );
        }
    };


    RuleEvalResult evaluate_rules( vector<RuleEntry> const& entries );

    struct AbbrBlock {
        u32 data_;

        constexpr size_t index() const noexcept { return data_ >> 4; }
        constexpr size_t size() const noexcept { return data_ & 0xf; }

        static AbbrBlock make( size_t i, size_t s ) noexcept {
            return { u32( i ) << 4 | u32( s & 0xf ) };
        }

        constexpr explicit operator u32() const noexcept { return data_; }

        constexpr bool operator==( AbbrBlock const& rhs ) const noexcept {
            return data_ == rhs.data_;
        }
    };
    std::string format_as( AbbrBlock b );

    struct ZoneStates {
        /// Return the _last_ value less than or equal to 'when'.
        /// Return -1 if no such value exists
        static ptrdiff_t _find( span<i64 const> s, i64 when ) {
            auto it = std::upper_bound( s.begin(), s.end(), when );
            return ( it - s.begin() ) - 1;
        }

        sysseconds_t     safe_cycle_time;
        vector<ZoneAbbr> abbr_table_;

        AbbrBlock abbr_initial_;
        i32       walloff_initial_;
        i32       stdoff_initial_;

        vector<sysseconds_t> abbr_trans_;
        vector<AbbrBlock>    abbr_;

        vector<sysseconds_t> walloff_trans_;
        vector<i32>          walloff_;

        vector<sysseconds_t> stdoff_trans_;
        vector<i32>          stdoff_;

        vector<sysseconds_t> tt_;

        vector<ZoneTransition> get_transitions() const;

        ZoneState initial() const noexcept {
            return {
                FromUTC( stdoff_initial_ ),
                FromUTC( walloff_initial_ ),
                abbr_table_[abbr_initial_.index()],
            };
        }

        i32 const& stdoff( sysseconds_t t ) const noexcept {
            auto i = _find( stdoff_trans_, t );
            return i >= 0 ? stdoff_[i] : stdoff_initial_;
        }

        i32 const& walloff( sysseconds_t t ) const noexcept {
            auto i = _find( walloff_trans_, t );
            return i >= 0 ? walloff_[i] : walloff_initial_;
        }

        AbbrBlock const& abbr( sysseconds_t t ) const noexcept {
            auto i = _find( abbr_trans_, t );
            return i >= 0 ? abbr_[i] : abbr_initial_;
        }


        string_view abbr_to_sv( AbbrBlock block ) {
            return string_view(
                abbr_table_[block.index()].buff_, block.size() );
        }


        ZoneState get_state( sysseconds_t t ) const {
            return {
                FromUTC( stdoff( t ) ),
                FromUTC( walloff( t ) ),
                abbr_table_[abbr( t ).index()],
            };
        }

        string format_local( sysseconds_t time ) const {
            auto const& state = get_state( time );
            return local_to_string( time, state.walloff, state.abbr );
        }

        static ZoneStates make_static( ZoneState state ) {
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

        RuleEvalResult evaluate_rules( string_view rule ) const;
        ZoneStates     get_zone_states( string_view name, i32 end_year ) const;

        bool operator==( TZDataFile const& rhs ) const noexcept {
            return rules == rhs.rules    //
                   && zones == rhs.zones //
                   && links == rhs.links;
        }

        /// List all zones by name
        vector<string> list_zones() const {
            vector<string> result( zones.size() );
            size_t         i = 0;
            for( auto const& [name, _] : zones ) result[i++] = string( name );
            return result;
        }
    };

    struct TZData : TZDataFile {
        vector<std::pair<string, file_bytes>> data;

        std::string version;

        /// Get a list of the source files that make up the timezone database
        vector<string> source_files() const {
            vector<string> result( data.size() );
            for( size_t i = 0; i < data.size(); ++i ) result[i] = data[i].first;
            return result;
        }
    };

    rule_year_t parse_year( OptTok tok );
    rule_year_t parse_year_to( OptTok tok );
    Mon         parse_month( OptTok tok );
    u8          parse_day_of_month( OptTok tok );
    RuleOn      parse_rule_on( OptTok tok );

    /// Parse a Zone Rule entry
    ///
    /// This can be either:
    /// - A '-', indicating that there is no associated rule
    /// - A named rule, such as 'US' or 'Indianapolis'
    /// - A offset, such as 1:00
    ZoneRule parse_zone_rule( OptTok tok );

    /// Parse a zone format
    ZoneFormat parse_zone_format( OptTok tok );

    /// Parse a zone entry
    ZoneEntry parse_zone_entry( TokenIter tok_iter );

    /// Parses either a rule, zone, or link
    void parse_entry( TZDataFile& file, string_view line, LineIter& lines );

    // Estimated number of buckets for rules
    // There are 130 rules:
    // `rg -o_ni '^R [^ ]+' build/data/tzdata/tzdata.zi | sort -u | wc -l`
    constexpr size_t RULE_BUCKETS = 130 * 2;
    // Estimated number of buckets for timezones.
    // There are 341 zones in tzdata.zi:
    // `rg -o_ni '^Z [^ ]+' build/data/tzdata/tzdata.zi | sort -u | wc -l`
    constexpr size_t ZONE_BUCKETS = 341 * 2;
    // Estimated number of buckets for links.
    // There are 111 links:
    // `rg -o_ni '^L [^ ]+' build/data/tzdata/tzdata.zi | sort -u | wc -l`
    constexpr size_t LINK_BUCKETS = 111 * 2;

    TZData load_zone_info_from_file( string file );

    /// Load zone info from a directory, eg build/data/tzdata
    TZData load_zone_info_from_dir( string fp );

    TZDataFile parse_tzdata(
        string_view input, string_view filename = "(none)" );

    /// Append timezone data from the given input
    void append_tzdata(
        TZDataFile& file, string_view input, string_view filename = "(none)" );

} // namespace vtz
