#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vtz/dates.h>
#include <vtz/strings.h>

namespace vtz {
    using rule_year_t            = i16;
    constexpr rule_year_t Y_ONLY = -1;
    constexpr rule_year_t Y_MAX  = -2;


    constexpr i32 OFFSET_NPOS = INT32_MIN;

    i32 parseHHMMSSOffset( char const* p, size_t size, int sign = 1 ) noexcept;
    i32 parseSignedHHMMSSOffset( char const* p, size_t size ) noexcept;

    inline i32 parseHHMMSSOffset( string_view sv ) noexcept {
        return parseHHMMSSOffset( sv.data(), sv.size(), 1 );
    }
    inline i32 parseSignedHHMMSSOffset( string_view sv ) noexcept {
        return parseSignedHHMMSSOffset( sv.data(), sv.size() );
    }


    template<size_t N>
    struct FixStr {
        u8   size_{};
        char buff_[N]{};


        constexpr string_view sv() const noexcept { return { data(), size() }; }

        explicit operator string_view() const noexcept { return sv(); }

        constexpr size_t      size() const noexcept { return size_; }
        constexpr char const* data() const noexcept { return buff_; }

        constexpr bool operator==( FixStr const& rhs ) const noexcept {
            return sv() == rhs.sv();
        }

        constexpr char const* begin() const noexcept { return buff_; }
        constexpr char const* end() const noexcept { return buff_ + size_; }

        using value_type = char;
    };

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
    std::string format_as( RuleSave );

    class RuleAt {
        i32 repr_{};

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
    };

    std::string format_as( RuleAt r );


    class RuleOn {
        u16 repr_;

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
    };
    std::string format_as( RuleOn r );

    // # Rule	NAME	FROM	TO	-	IN	ON	AT	SAVE
    // LETTER Rule	CA	1948	only	-	Mar	14	2:01
    // 1:00	D Rule	CA	1949	only	-	Jan	 1	2:00
    // 0	S Rule	CA	1950	1966	-	Apr	lastSun	1:00
    // 1:00	D
    struct Rule {
        string_view name;
        rule_year_t from;
        rule_year_t to;
        Mon         in;
        RuleOn      on;
        RuleAt      at;
        RuleSave    save;
        RuleLetter  letter;

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

    struct ZoneOff {
        i32 offset{};
        ZoneOff() = default;

        constexpr ZoneOff( i32 offset ) noexcept
        : offset( offset ) {}

        explicit ZoneOff( string_view sv )
        : offset( parseSignedHHMMSSOffset( sv ) ) {}

        template<size_t N>
        ZoneOff( char const ( &arr )[N] )
        : ZoneOff( string_view( arr ) ) {}


        constexpr bool operator==( ZoneOff const& rhs ) const noexcept {
            return offset == rhs.offset;
        }
    };
    std::string format_as( ZoneOff off );

    template <class T, T NULL_VALUE>
    struct Opt {
        T data;
        bool has_value() const noexcept { return data == NULL_VALUE; }
        T& value() noexcept { return data; }
        T const& value() const noexcept { return data; }
    };

    // # Zone	NAME		STDOFF	RULES	FORMAT	[UNTIL]
    // Zone America/Los_Angeles -7:52:58 -	LMT	1883 Nov 18 20:00u
    //             -8:00	US	P%sT	1946
    //             -8:00	CA	P%sT	1967
    //             -8:00	US	P%sT

    struct ZoneEntry {
        ZoneOff     stdoff;
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

    TZDataFile parseTZData(
        string_view input, string_view filename = "(none)" );


} // namespace vtz
