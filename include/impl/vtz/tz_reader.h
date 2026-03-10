#pragma once

#include "vtz/tz_reader/tz_string.h"
#include <algorithm>
#include <utility>
#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/files.h>
#include <vtz/impl/bit.h>
#include <vtz/inplace_optional.h>
#include <vtz/span.h>
#include <vtz/strings.h>

#include <vtz/tz_reader/from_utc.h>
#include <vtz/tz_reader/link.h>
#include <vtz/tz_reader/rule_at.h>
#include <vtz/tz_reader/rule_letter.h>
#include <vtz/tz_reader/rule_on.h>
#include <vtz/tz_reader/zone_format.h>
#include <vtz/tz_reader/zone_rule.h>
#include <vtz/tz_reader/zone_save.h>
#include <vtz/tz_reader/zone_state.h>
#include <vtz/tz_reader/zone_until.h>

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

    struct parse_error {
        char const* expected; ///< What was expected
        char const* but;      ///< Reason why input was bad
        opt_token   token;    ///< token where failure occurred

        std::string get_error_message(
            string_view input, string_view filename ) const;
    };


    struct rule_trans {
        sys_days_t  date;
        rule_at     at;
        zone_save   save;
        rule_letter letter;

        /// Resolve the time when the rule would take effect, based on the
        /// current state of the zone
        constexpr sys_seconds_t resolve( zone_time time ) const noexcept {
            return at.resolve_at( date, time );
        }

        /// Resolve the zone_transition when the rule would take effect, based
        /// on the current state of the zone.
        ///
        /// The rule will take effect based on the zone's _CURRENT_ stdoff or
        /// walloff, so we need to pass in the full zone_time object.
        constexpr zone_transition resolve_trans(
            zone_time time, zone_format format ) const noexcept {
            return zone_transition{
                resolve( time ),
                zone_state{
                    time.stdoff,
                    time.stdoff.save( save ),
                    format,
                    letter,
                },
            };
        }

        static rule_trans from_civil( int year,
            u32                           mon,
            u32                           day,
            rule_at                       at,
            zone_save                     save,
            rule_letter                   letter ) {
            return rule_trans{
                resolve_civil( year, mon, day ), at, save, letter
            };
        }

        constexpr bool operator==( rule_trans const& rhs ) const noexcept {
            return date == rhs.date    //
                   && at == rhs.at     //
                   && save == rhs.save //
                   && letter == rhs.letter;
        }


        constexpr static auto compare_date() noexcept {
            return []( rule_trans const& lhs, rule_trans const& rhs ) {
                return lhs.date < rhs.date;
            };
        }
    };


    /// Example rules:
    ///
    /// #Rule NAME    FROM TO    -   IN  ON      AT   SAVE LETTER
    /// Rule  Chicago 1920 only  -   Jun 13      2:00 1:00 D
    /// Rule  Chicago 1920 1921  -   Oct lastSun 2:00 0    S
    /// Rule  Chicago 1921 only  -   Mar lastSun 2:00 1:00 D
    /// Rule  Chicago 1922 1966  -   Apr lastSun 2:00 1:00 D
    /// Rule  Chicago 1922 1954  -   Sep lastSun 2:00 0    S
    /// Rule  Chicago 1955 1966  -   Oct lastSun 2:00 0    S

    struct rule_entry {
        rule_year_t from;
        rule_year_t to;
        month_t     in;
        rule_on     on;
        rule_at     at;
        zone_save   save;
        rule_letter letter;

        /// Resolve the DateTime (in sys_seconds_t)
        constexpr sys_seconds_t resolve_at(
            i32 year, from_utc stdoff, from_utc walloff ) const noexcept {
            return at.resolve_at(
                on.resolve_date( year, in ), stdoff, walloff );
        }

        constexpr sys_days_t resolve_date( i32 year ) const noexcept {
            return on.resolve_date( year, in );
        }

        // Resolve a transition for the given year
        constexpr rule_trans resolve_trans( i32 year ) const noexcept {
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

        bool operator==( rule_entry const& rhs ) const noexcept {
            return from == rhs.from    //
                   && to == rhs.to     //
                   && in == rhs.in     //
                   && on == rhs.on     //
                   && at == rhs.at     //
                   && save == rhs.save //
                   && letter == rhs.letter;
        }

        constexpr static auto is_expired( i64 year ) noexcept {
            return [year]( rule_entry const& rule ) {
                return rule.expires_before( year );
            };
        }


        constexpr static auto compare_from() noexcept {
            return []( rule_entry const& lhs, rule_entry const& rhs ) {
                return lhs.from < rhs.from;
            };
        }

        constexpr static auto has_expiry() noexcept {
            return []( rule_entry const& rule ) { return rule.to != Y_MAX; };
        }
    };


    // # Zone	NAME		STDOFF	RULES	FORMAT	[UNTIL]
    // Zone America/Los_Angeles -7:52:58 -	LMT	1883 Nov 18 20:00u
    //             -8:00	US	P%sT	1946
    //             -8:00	CA	P%sT	1967
    //             -8:00	US	P%sT

    struct zone_entry {
        from_utc    stdoff;
        zone_until  until;
        zone_rule   rules;
        zone_format format;

        zone_entry() = default;

        constexpr zone_entry( from_utc stdoff,
            zone_rule                  rules,
            zone_format                format,
            zone_until                 until ) noexcept
        : stdoff( stdoff )
        , until( until )
        , rules( rules )
        , format( format ) {}

        zone_entry( string_view stdoff,
            string_view         rules,
            string_view         format,
            string_view         until = {} );

        bool operator==( zone_entry const& rhs ) const noexcept {
            return stdoff == rhs.stdoff    //
                   && rules == rhs.rules   //
                   && format == rhs.format //
                   && until == rhs.until;
        }
    };

    struct zone {
        string_view        name;
        vector<zone_entry> ents;

        bool operator==( zone const& rhs ) const noexcept {
            return name == rhs.name && ents == rhs.ents;
        }
    };

    using rule_map = map<string_view, vector<rule_entry>>;
    using zone_map = map<string_view, vector<zone_entry>>;
    using link_map = map<string_view, string_view>;


    /// Get's the initial state, before any transition has happened.
    ///
    /// From 'How to Read the tz Database Source Files':
    /// > If switching to a named rule before any transition has happened,
    /// > assume standard time (SAVE zero), and use the LETTER data from
    /// > the earliest transition with a SAVE of zero.
    [[nodiscard]] inline std::optional<rule_letter> get_initial_letter(
        rule_trans const* trans, size_t trans_size ) {
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

    struct rule_eval_result {
        /// Known transitions: contains all transitions up until the current
        /// active rule set
        vector<rule_trans> historical;
        // Rule Entries that are still active after evaluation (these are
        // entries which have 'max' as the TO fields)
        vector<rule_entry> active;
        // First year where there's a rule
        i32 year_start;
        // Year where the historical rule transitions no longer apply
        // (and we only care about active rules)
        i32 year_end;
    };


    size_t dump_active( rule_entry const* active,
        size_t                            active_count,
        size_t                            year,
        rule_trans*                       p );

    /// Return the earliest active rule with a save of 0. Returns nullptr
    /// if no active rule with a save of 0 is found
    inline rule_entry const* earliest_active_with_save_zero(
        rule_entry const* active, size_t active_count, i32 year ) noexcept {
        size_t     result        = size_t( -1 );
        sys_days_t earliest_date = MAX_DAYS;
        for( size_t i = 0; i < active_count; ++i )
        {
            if( active[i].save.save != 0 ) continue;
            auto date = active[i].resolve_date( year );
            if( date <= earliest_date )
            {
                earliest_date = date;
                result        = i;
            }
        }
        return result < active_count // did we find an entry?
                   ? active + result // yes: return pointer to entry
                   : nullptr         // no: return nullptr
            ;
    }

    /// Implements a State Machine such that we can pull rule transitions
    struct rule_trans_iter {
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
        rule_trans const* hist_;
        size_t            hist_size_;

        /// Rules which are active going forward into perpetuity
        rule_entry const* active_;
        size_t            active_size_;

        /// Saves the first active year (for record keeping purposes, eg when
        /// it's necessary to retrieve the initial letter)
        i32 first_active_year_;

        /// Evaluated transitions from active entries buffer
        std::unique_ptr<rule_trans[]> active_buff;

        rule_trans_iter( rule_trans const* hist,
            size_t                         hist_size,
            rule_entry const*              active,
            size_t                         active_size,
            int                            first_active_year ) noexcept
        : i( 0 )
        , in_historical( true )
        , year( first_active_year )
        , hist_( hist )
        , hist_size_( hist_size )
        , active_( active )
        , active_size_( active_size )
        , first_active_year_( first_active_year ) {}

        rule_trans_iter( rule_eval_result const& r ) noexcept
        : rule_trans_iter{
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
        /// Returns `rule_letter("-")` if there are no entries with a save of 0
        rule_letter initial_letter() const noexcept {
            for( size_t j = 0; j < hist_size_; ++j )
            {
                if( hist_[j].save.save == 0 ) { return hist_[j].letter; }
            }
            /// NOTE: we are living in a state of sin by doing this... active
            /// transitions are not necessarily sorted by the one we enter
            /// earliest
            if( auto* result = earliest_active_with_save_zero(
                    active_, active_size_, first_active_year_ ) )
                return result->letter;

            return rule_letter( "-" );
        }


        std::optional<rule_trans> next() {
            if( in_historical )
            {
                // If we have historical rules, then we can just pull from that
                if( i < hist_size_ ) return hist_[i++];
                i             = active_size_;
                in_historical = false;
                active_buff.reset( new rule_trans[active_size_] );
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


    class zone_trans_iter {
        rule_trans_iter rule_iter_;
        rule_trans      next_;
        rule_letter     current_letter_;
        zone_save       current_save_;
        bool            is_done;

      public:

        zone_trans_iter( rule_eval_result const& r )
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
        [[nodiscard]] zone_state advance_to( sys_seconds_t when,
            from_utc                                       stdoff,
            zone_format const&                             format,
            zone_time                                      old_time ) {
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
            return zone_state( stdoff, current_save_, format, current_letter_ );
        }

        zone_save   current_save() const noexcept { return current_save_; }
        rule_letter current_letter() const noexcept { return current_letter_; }

        /// Return the next zone_transition, if one exists prior to the
        /// until_time
        std::optional<zone_transition> next( sys_seconds_t until_time,
            from_utc                                       stdoff,
            zone_format const&                             format ) {
            // Record the current walloff. This is needed because the time
            // when the next transition will occur will be in terms of the
            // current walloff.
            from_utc walloff = stdoff.save( current_save_ );

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


    rule_eval_result evaluate_rules( vector<rule_entry> const& entries );

    struct abbr_block {
        u32 data_;

        constexpr size_t index() const noexcept { return data_ >> 4; }
        constexpr size_t size() const noexcept { return data_ & 0xf; }

        static abbr_block make( size_t i, size_t s ) noexcept {
            return { u32( i ) << 4 | u32( s & 0xf ) };
        }

        constexpr explicit operator u32() const noexcept { return data_; }

        constexpr bool operator==( abbr_block const& rhs ) const noexcept {
            return data_ == rhs.data_;
        }
    };

    struct zone_states {
        /// Return the _last_ value less than or equal to 'when'.
        /// Return -1 if no such value exists
        static ptrdiff_t _find( span<i64 const> s, i64 when ) {
            auto it = std::upper_bound( s.begin(), s.end(), when );
            return ( it - s.begin() ) - 1;
        }

        sys_seconds_t     safe_cycle_time;
        vector<zone_abbr> abbr_table_;

        abbr_block abbr_initial_;
        i32        walloff_initial_;
        i32        stdoff_initial_;

        vector<sys_seconds_t> abbr_trans_;
        vector<abbr_block>    abbr_;

        vector<sys_seconds_t> walloff_trans_;
        vector<i32>           walloff_;

        vector<sys_seconds_t> stdoff_trans_;
        vector<i32>           stdoff_;

        vector<sys_seconds_t> tt_;

        vector<zone_transition> get_transitions() const;

        zone_state initial() const noexcept {
            return {
                from_utc( stdoff_initial_ ),
                from_utc( walloff_initial_ ),
                abbr_table_[abbr_initial_.index()],
            };
        }

        i32 const& stdoff( sys_seconds_t t ) const noexcept {
            auto i = _find( stdoff_trans_, t );
            return i >= 0 ? stdoff_[size_t( i )] : stdoff_initial_;
        }

        i32 const& walloff( sys_seconds_t t ) const noexcept {
            auto i = _find( walloff_trans_, t );
            return i >= 0 ? walloff_[size_t( i )] : walloff_initial_;
        }

        abbr_block const& abbr( sys_seconds_t t ) const noexcept {
            auto i = _find( abbr_trans_, t );
            return i >= 0 ? abbr_[size_t( i )] : abbr_initial_;
        }


        string_view abbr_to_sv( abbr_block block ) {
            return string_view(
                abbr_table_[block.index()].buff_, block.size() );
        }


        zone_state get_state( sys_seconds_t t ) const {
            return {
                from_utc( stdoff( t ) ),
                from_utc( walloff( t ) ),
                abbr_table_[abbr( t ).index()],
            };
        }

        string format_local( sys_seconds_t time ) const {
            auto const& state = get_state( time );
            return local_to_string( time, state.walloff, state.abbr );
        }

        static zone_states make_static( zone_state state ) {
            return {
                0,
                { state.abbr },
                abbr_block::make( 0, state.abbr.size() ),
                state.walloff.off,
                state.stdoff.off,
            };
        }
    };

    struct tz_data_file {
        rule_map rules;
        zone_map zones;
        link_map links;

        rule_eval_result evaluate_rules( string_view rule ) const;
        zone_states get_zone_states( string_view name, i32 end_year ) const;

        bool operator==( tz_data_file const& rhs ) const noexcept {
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

    struct tz_data : tz_data_file {
        vector<std::pair<string, file_bytes>> data;

        std::string version;

        /// Get a list of the source files that make up the timezone database
        vector<string> source_files() const {
            vector<string> result( data.size() );
            for( size_t i = 0; i < data.size(); ++i ) result[i] = data[i].first;
            return result;
        }
    };

    rule_year_t parse_year( opt_token tok );
    rule_year_t parse_year_to( opt_token tok );
    month_t     parse_month( opt_token tok );
    u8          parse_day_of_month( opt_token tok );
    rule_on     parse_rule_on( opt_token tok );

    /// Parse a POSIX TZ String that may appear at the end of a tzfile
    tz_string parse_tz_string( string_view sv );

    /// Parse a Zone Rule entry
    ///
    /// This can be either:
    /// - A '-', indicating that there is no associated rule
    /// - A named rule, such as 'US' or 'Indianapolis'
    /// - A offset, such as 1:00
    zone_rule parse_zone_rule( opt_token tok );

    /// Parse a zone format
    zone_format parse_zone_format( opt_token tok );

    /// Parse a zone entry
    zone_entry parse_zone_entry( token_iter tok_iter );

    /// Parses either a rule, zone, or link
    void parse_entry( tz_data_file& file, string_view line, line_iter& lines );

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

    tz_data load_zone_info_from_file( string file );

    /// Load zone info from a directory, eg build/data/tzdata
    tz_data load_zone_info_from_dir( string fp );

    tz_data_file parse_tzdata(
        string_view input, string_view filename = "(none)" );

    /// Append timezone data from the given input
    void append_tzdata( tz_data_file& file,
        string_view                   input,
        string_view                   filename = "(none)" );


    zone_states load_zone_states( span<zone_entry const> entries,
        map<string_view, zone_trans_iter>                cache,
        i64                                              safe_cycle_time,
        i32                                              end_year = -1 );

    zone_states load_zone_states_tzfile( std::string const& fp );
} // namespace vtz
