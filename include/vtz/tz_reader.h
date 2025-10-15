#pragma once

#include <ankerl/unordered_dense.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/strings.h>

#if __has_builtin( __builtin_memcpy )
    #define _vtz_memcpy __builtin_memcpy
#else
    #include <cstring>
    #define _vtz_memcpy memcpy
#endif

namespace vtz {
    using ankerl::unordered_dense::map;
    using std::string;

    using rule_year_t           = i16;
    constexpr rule_year_t Y_MAX = -1;
    static_assert( size_t( Y_MAX ) == ~size_t() );

    constexpr i32 OFFSET_NPOS = INT32_MIN;

    i32 parseHHMMSSOffset( char const* p, size_t size, int sign = 1 ) noexcept;
    i32 parseSignedHHMMSSOffset( char const* p, size_t size ) noexcept;

    inline i32 parseHHMMSSOffset( string_view sv ) noexcept {
        return parseHHMMSSOffset( sv.data(), sv.size(), 1 );
    }
    inline i32 parseSignedHHMMSSOffset( string_view sv ) noexcept {
        return parseSignedHHMMSSOffset( sv.data(), sv.size() );
    }

    struct RuleSave {
        i32 save = 0;

        constexpr RuleSave() = default;
        constexpr RuleSave( i32 save ) noexcept
        : save( save ) {}

        template<size_t N>
        RuleSave( char const ( &arr )[N] )
        : RuleSave( string_view( arr ) ) {}

        RuleSave( string_view text );

        static auto HHMM( int sign, i32 hour, i32 min ) noexcept -> RuleSave {
            return { sign * ( 3600 * hour + 60 * min ) };
        }
        static auto HHMMSS( int sign, i32 hour, i32 min, i32 sec ) noexcept
            -> RuleSave {
            return { sign * ( 3600 * hour + 60 * min + sec ) };
        }

        bool operator==( RuleSave rhs ) const noexcept {
            return save == rhs.save;
        }
    };
    string format_as( RuleSave );

    /// Consider an offset like `UTC-5:00`. This offset is _from_ UTC,
    /// so to get to Local time, you take the UTC time, and add `-5:00`
    /// (This is `utc + off`)
    ///
    /// Similarly, to get to UTC time, from local time, you _subtract_ `-5:00`
    /// (This is `utc - off`)

    struct FromUTC {
        i32 off{};
        FromUTC() = default;

        /// Add offset to get from utc time to local time
        constexpr i64 toLocal( i64 utc ) const noexcept { return utc + off; }

        /// Subtract offset to get from local time to UTC time
        constexpr i64 toUTC( i64 local ) const noexcept { return local - off; }

        /// Consider America/New_York. When we 'save' 1 hour, our offset from
        /// UTC goes from `-5:00` to `-4:00` - the hour is _added_ to the offset
        constexpr FromUTC save( RuleSave save ) const noexcept {
            return { off + save.save };
        }

        /// Consider America/New_York. When we 'save' 1 hour, our offset from
        /// UTC
        /// goes from `-5:00` to `-4:00` - the hour is _added_ to the offset
        constexpr FromUTC save( i32 saveSeconds ) const noexcept {
            return { off + saveSeconds };
        }

        constexpr FromUTC( i32 off ) noexcept
        : off( off ) {}

        explicit FromUTC( string_view sv )
        : off( parseSignedHHMMSSOffset( sv ) ) {}

        template<size_t N>
        FromUTC( char const ( &arr )[N] )
        : FromUTC( string_view( arr ) ) {}


        constexpr bool operator==( FromUTC const& rhs ) const noexcept {
            return off == rhs.off;
        }
    };
    string format_as( FromUTC off );


    struct alignas( 8 ) RuleLetter : FixStr<7> {
        using FixStr = FixStr<7>;

        using FixStr::FixStr;

        RuleLetter() = default;
        constexpr RuleLetter( FixStr const& rhs ) noexcept
        : FixStr( rhs ) {}
        constexpr RuleLetter( char const ( &arr )[2] ) noexcept
        : FixStr{ 1, { arr[0] } } {}
        constexpr RuleLetter( char const ( &arr )[3] ) noexcept
        : FixStr{ 2, { arr[0], arr[1] } } {}
        constexpr RuleLetter( char const ( &arr )[4] ) noexcept
        : FixStr{ 3, { arr[0], arr[1], arr[2] } } {}


        constexpr bool operator==( RuleLetter const& rhs ) const noexcept {
            return sv() == sv();
        }
    };


    class RuleAt {
        i32 repr_{};

        constexpr explicit RuleAt( i32 repr ) noexcept
        : repr_( repr ) {}

      public:

        enum Kind : u8 {
            /// 'AT' field corresponds to local WALL time (this is the default)
            /// suffix _may_ be 'w', but also may not be present
            LOCAL_WALL,
            /// 'AT' field corresponds to local standard time ('s' suffix)
            LOCAL_STANDARD,
            /// 'AT' field corresponds to standard time at the prime meridian
            /// suffix can be g, u, or z
            UTC,
        };


        constexpr i32  offset() const noexcept { return repr_ >> 2; }
        constexpr Kind kind() const noexcept { return Kind( repr_ & 0x3 ); }

        RuleAt() = default;

        constexpr RuleAt( i32 time_, Kind kind ) noexcept
        : repr_( time_ << 2 | i32( kind ) ) {}

        template<size_t N>
        RuleAt( char const ( &arr )[N] )
        : RuleAt( string_view( arr ) ) {}

        RuleAt( string_view text );

        constexpr bool operator==( RuleAt const& rhs ) const noexcept {
            return repr_ == rhs.repr_;
        }
        constexpr bool operator!=( RuleAt const& rhs ) const noexcept {
            return repr_ != rhs.repr_;
        }

        /// Returns the timestamp (in seconds from UTC) when the 'AT' time is
        /// referring to, on the given date
        constexpr sysseconds_t resolveAt(
            sysdays_t date, FromUTC stdoff, FromUTC walloff ) const noexcept {
            // Time when the date starts (midnight on that date)
            i64 T = daysToSeconds( date );

            // time of day -
            i32 timeOfDay = offset();
            switch( kind() )
            {
            /// Time of day represents an offset from the current local time in
            /// the zone
            case LOCAL_WALL: return T + walloff.toUTC( timeOfDay );
            /// Time of day represents an offset from standard time within the
            /// zone
            case LOCAL_STANDARD: return T + stdoff.toUTC( timeOfDay );
            /// Time of day represents an offset from UTC time within the zone
            case UTC: return T + timeOfDay;
            }
        }

        constexpr static RuleAt fromRepr( i32 repr ) noexcept {
            return RuleAt( repr );
        }
    };

    template<>
    struct OptTraits<RuleAt> {
        constexpr static RuleAt NULL_VALUE = RuleAt::fromRepr( INT32_MIN );
    };

    using OptRuleAt = OptClass<RuleAt>;

    string format_as( RuleAt r );


    class RuleOn {
        u16 repr_;

        constexpr explicit RuleOn( u16 repr ) noexcept
        : repr_( repr ) {}

      public:

        enum Kind {
            DAY,        ///< 'on' is a specific day of the month, eg '13'
            DOW_LAST,   ///< 'on' is, eg 'lastSun' or 'lastMon'
            DOW_AFTER,  ///< 'on' is 'Sun>=13', for example
            DOW_BEFORE, ///< 'on' is 'Fri<=1', for example
        };

        RuleOn() = default;

        constexpr RuleOn( Kind kind, u8 day, DOW dow ) noexcept
        : repr_( u32( kind ) | ( u32( dow ) << 2 ) | ( u32( day ) << 5 ) ) {}
        constexpr Kind kind() const noexcept { return Kind( repr_ & 0x3 ); }
        constexpr u32  day() const noexcept { return repr_ >> 5; }
        constexpr DOW  dow() const noexcept {
            return DOW( ( repr_ >> 2 ) & 0x7 );
        }


        constexpr static RuleOn on( u8 day ) noexcept {
            return { DAY, day, {} };
        }
        constexpr static RuleOn last( DOW dow ) noexcept {
            return { DOW_LAST, {}, dow };
        }
        constexpr static RuleOn after( u8 day, DOW dow ) noexcept {
            return { DOW_AFTER, day, dow };
        }
        constexpr static RuleOn before( u8 day, DOW dow ) noexcept {
            return { DOW_BEFORE, day, dow };
        }

        constexpr bool operator==( RuleOn const& rhs ) const noexcept {
            return repr_ == rhs.repr_;
        }

        constexpr static RuleOn fromRepr( u16 repr ) noexcept {
            return RuleOn( repr );
        }

        constexpr sysdays_t resolveDate( i32 year, Mon mon ) const noexcept {
            return resolve( year, u32( mon ) );
        }

        constexpr sysdays_t resolve( i32 year, u32 mon ) const noexcept {
            switch( kind() )
            {
            case DAY: return resolveCivil( year, mon, day() );
            case DOW_LAST: return resolveLastDOW( year, mon, dow() );
            case DOW_AFTER: return resolveDOW_GE( year, mon, day(), dow() );
            case DOW_BEFORE: return resolveDOW_LE( year, mon, day(), dow() );
            }
        }


        /// Evaluate the rule for a given year/month in order to obtain an
        /// actual date
        constexpr YMD eval( i32 year, Mon mon ) const noexcept {
            switch( kind() )
            {
            case DAY: return YMD( year, mon, day() );
            case DOW_LAST:
                return YMD(
                    year, mon, getLastDOWInMonth( year, u32( mon ), dow() ) );
            case DOW_AFTER:
                return getYMD_DOW_GE( year, u32( mon ), day(), dow() );
            case DOW_BEFORE:
                return getYMD_DOW_LE( year, u32( mon ), day(), dow() );
            }
        }

        string string() const;
    };
    inline string format_as( RuleOn r ) { return r.string(); }


    template<>
    struct OptTraits<RuleOn> {
        /// Corresponds to kind() == DAY, dow() == Sun, day() == 0, which is not
        /// a valid day of the month
        constexpr static RuleOn NULL_VALUE = RuleOn::fromRepr( 0 );
    };

    using OptRuleOn = OptClass<RuleOn>;


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

        bool operator==( RuleEntry const& rhs ) const noexcept {
            return from == rhs.from    //
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


    struct ZoneUntil {
        OptV<u16, 0> year; ///< year
        OptMon       mon;  ///< Month
        OptV<u8, 0>  day;  ///< Day of the month (1-31)
        OptRuleAt    at;   ///< Time of day when it ends

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
        FromUTC     stdoff;
        ZoneUntil   until;
        string_view rules;
        string_view format;

        ZoneEntry() = default;

        constexpr ZoneEntry( FromUTC stdoff,
            string_view              rules,
            string_view              format,
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

    struct ZoneState {
        i32        offset; // offset from UTC
        FixStr<11> abbr;
    };

    struct ZoneTransition {
        i64       when; // UTC time of change
        ZoneState state;
    };

    struct ZoneStates {
        ZoneState              initial;
        vector<ZoneTransition> transitions;
    };

    struct TZDataFile {
        RuleMap rules;
        ZoneMap zones;
        LinkMap links;

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

    /// Parse a zone entry
    ZoneEntry parseZoneEntry( TokenIter tok_iter );

    /// Parses either a rule, zone, or link
    void parseEntry( TZDataFile& file, string_view line, LineIter& lines );

    TZDataFile parseTZData(
        string_view input, string_view filename = "(none)" );


} // namespace vtz
