#include <fmt/format.h>
#include <vtz/libfmt_compat.h>


namespace vtz {
    static std::string to_hhmmss( int time_seconds ) {
        bool is_neg = time_seconds < 0;
        u32  save   = u32( std::abs( time_seconds ) );

        u32 hour  = save / 3600;
        save     %= 3600;
        u32 min   = save / 60;
        save     %= 60;
        u32 sec   = save;

        if( is_neg )
        {
            if( sec )
                return fmt::format( "-{:0>2}:{:0>2}:{:0>2}", hour, min, sec );
            else
                return fmt::format( "-{:0>2}:{:0>2}", hour, min );
        }
        else
        {
            if( sec )
                return fmt::format( "{:0>2}:{:0>2}:{:0>2}", hour, min, sec );
            else
                return fmt::format( "{:0>2}:{:0>2}", hour, min );
        }
    }


    std::string format_as( AbbrBlock b ) {
        return fmt::format( "(index: {}, size: {})", b.index(), b.size() );
    }


    std::string format_as( zone_save s ) {
        if( s.save == 0 ) { return "0"; }

        return to_hhmmss( s.save );
    }


    std::string format_as( from_utc off ) { return to_hhmmss( off.off ); }


    std::string format_as( RuleAt r ) {
        auto time = to_hhmmss( r.offset() );
        switch( r.kind() )
        {
        case RuleAt::LOCAL_WALL: return time;
        case RuleAt::LOCAL_STANDARD: return time + 's';
        case RuleAt::UTC: return time + 'u';
        default: return time + "<bad kind>";
        }
    }


    std::string format_as( ZoneUntil until ) {
        if( until.has_value() )
        {
            auto ymd = to_civil( until.date );
            return fmt::format(
                "{:>4} {} {:>2} {}", ymd.year, ymd.mon(), ymd.day, until.at );
        }

        return "(none)";
    }


    string format_as( ZoneRule const& rule ) {
        switch( rule.kind() )
        {
        case ZoneRule::HYPHEN: return "-";
        case ZoneRule::NAMED: return string( rule.name() );
        case ZoneRule::OFFSET: return to_hhmmss( rule.offset() );
        }

        throw std::runtime_error(
            "format_as(ZoneRule): kind() is invalid/corrupt" );
    }


    string format_as( ZoneFormat const& f ) {
        size_t      sz0 = ( f.fmt_ >> 2 ) & 0xf;
        size_t      sz1 = ( f.fmt_ >> 6 ) & 0xf;
        string_view h0( f.buff, sz0 );
        string_view h1( f.buff + sz0, sz1 );
        string_view mid;
        switch( f.tag() )
        {
        case ZoneFormat::LITERAL: mid = {}; break;
        case ZoneFormat::SLASH: mid = "/"; break;
        case ZoneFormat::FMT_S: mid = "%s"; break;
        case ZoneFormat::FMT_Z: mid = "%z"; break;
        }
        return fmt::format( "{}{}{}", h0, mid, h1 );
    }


    string format_as( RuleTrans const& r ) {
        return fmt::format( "{} @ {} SAVE={} LETTER='{}'",
            to_civil( r.date ),
            r.at,
            r.save,
            r.letter.sv() );
    }


    string format_as( RuleOn r ) {
        switch( r.kind() )
        {
        case RuleOn::DAY: return fmt::format( "{}", r.day() );
        case RuleOn::DOW_LE: return fmt::format( "{}<={}", r.dow(), r.day() );
        case RuleOn::DOW_GE: return fmt::format( "{}>={}", r.dow(), r.day() );
        case RuleOn::DOW_LAST: return fmt::format( "last{}", r.dow() );
        }

        throw std::runtime_error( "RuleOn::str(): bad kind()" );
    }
} // namespace vtz
