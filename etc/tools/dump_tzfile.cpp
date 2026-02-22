#include <cstdint>
#include <fmt/color.h>
#include <fmt/format.h>
#include <vtz/files.h>
#include <vtz/format.h>
#include <vtz/strings.h>
#include <vtz/tz.h>
#include <vtz/tz_reader/from_utc.h>
#include <vtz/tz_reader/leap_table.h>
#include <vtz/tzfile.h>

#include <vtz/libfmt_compat.h>

using color = fmt::terminal_color;
using fmt::fg;
using fmt::print;
using fmt::println;
using fmt::styled;
using em = fmt::emphasis;

using std::string_view;

constexpr auto bold_blue = fg( color::blue ) | em::bold;
// constexpr auto bold_cyan    = fg( color::cyan ) | em::bold;
constexpr auto bold_magenta = fg( color::magenta ) | em::bold;
constexpr auto bold_green   = fg( color::green ) | em::bold;
constexpr auto bold_white   = fg( color::white ) | em::bold;
constexpr auto bold_red     = fg( color::red ) | em::bold;


template<class T>
static void print_header_entry(
    string_view name, T const& value, string_view what ) {
    println( "  {:<16}  {:6}  {}",
        styled( name, fg( color::blue ) ),
        styled( value, fg( color::white ) | em::bold ),
        styled( what, em::faint ) );
}

static void print_tzif_header( vtz::tzfile_header const& header,
    string_view                                          title = "TZif Header",
    bool                                                 show_version = true ) {
    // Print header title
    println( "{}",
        styled( fmt::format( "=== {} ===", title ),
            fg( color::cyan ) | em::bold ) );

    if( show_version )
    {
        println( "{}      TZif", styled( "Magic:", fg( color::green ) ) );

        print( "{}    ", styled( "Version:", fg( color::green ) ) );
        if( header.version == '\0' )
        {
            // version is null byte (can't be displayed)
            println( "{}", styled( "1 (NUL byte)", fg( color::yellow ) ) );
        }
        else
        {
            // version is valid
            println( "{}", styled( header.version, fg( color::yellow ) ) );
        }

        println( "" );
    }

    // Print the six integer values
    println(
        "{}", styled( "Header Counts:", fg( color::magenta ) | em::bold ) );

    print_header_entry(
        "tzh_ttisutcnt:", header.tzh_ttisutcnt, "(UT/local indicators)" );
    print_header_entry( "tzh_ttisstdcnt:",
        header.tzh_ttisstdcnt,
        "(standard/wall indicators)" );
    print_header_entry(
        "tzh_leapcnt:", header.tzh_leapcnt, "(leap second entries)" );
    print_header_entry(
        "tzh_timecnt:", header.tzh_timecnt, "(transition times)" );
    print_header_entry(
        "tzh_typecnt:", header.tzh_typecnt, "(local time types)" );
    print_header_entry(
        "tzh_charcnt:", header.tzh_charcnt, "(timezone abbreviation bytes)" );
}

static std::string describe(
    vtz::ttinfo_bytes type, string_view designations ) {
    return fmt::format( "isdst={}  abbr={:>4}  utoff={:>9}",
        type.tt_isdst() ? 1 : 0,
        designations.data() + type.tt_desigidx(),
        vtz::FromUTC( type.tt_utoff() )
        //
    );
}

template<vtz::tzfile_kind kind>
void print_file(
    vtz::basic_tzfile<kind> const& ff, string_view title = "TZif Header" ) {
    print_tzif_header( ff.header(), title );

    using vtz::leap_bytes;
    using vtz::endian::span_be;
    using vtz::endian::span_bytes;

    size_t timecnt      = ff.tzh_timecnt;
    auto   TT           = ff.transition_times();
    auto   type_indices = ff.type_indices();
    auto   ttinfo       = ff.ttinfo();
    auto   designations = ff.abbrev_buff();

    vtz::LeapTable leaps = ff.leap();

    auto is_std = ff.isstd();
    auto is_utc = ff.isutc();

    for( size_t i = 0; i < timecnt; ++i )
    {
        // Print transition times
        int64_t Traw     = TT[i];
        int64_t T        = leaps.remove_leap( Traw );
        auto    ti       = type_indices[i];
        auto    info     = ttinfo[ti];
        auto    utoff    = info.tt_utoff();
        auto    isdst    = info.tt_isdst();
        auto    desigidx = info.tt_desigidx();

        char const* abbr = designations.data() + desigidx;

        if( !leaps.empty() )
        {
            println( "raw={} utc={} local={} off={} dst={} abbr={} ti={}",
                styled( vtz::format_time_s( "%F %T", Traw ), bold_magenta ),
                styled( vtz::format_time_s( "%F %T", T ), bold_magenta ),
                styled( vtz::format_time_s( "%F %T", T + utoff ), bold_blue ),
                styled( vtz::FromUTC( utoff ), bold_green ),
                styled( int( isdst ), bold_white ),
                styled( abbr, bold_red ),
                styled( ti, bold_white ) );
        }
        else
        {
            println( "utc={} local={} off={} dst={} abbr={} ti={}",
                styled( vtz::format_time_s( "%F %T", T ), bold_magenta ),
                styled( vtz::format_time_s( "%F %T", T + utoff ), bold_blue ),
                styled( vtz::FromUTC( utoff ), bold_green ),
                styled( int( isdst ), bold_white ),
                styled( abbr, bold_red ),
                styled( ti, bold_white ) );
        }
    }

    if( ff.tzh_leapcnt )
    {
        fmt::println( "\nLeap entries:" );
        for( size_t i = 0; i < leaps.size(); ++i )
        {
            fmt::println( "  [{:>2}] count={:>2} when={}",
                i,
                leaps.counts( i ),
                vtz::format_time_s( "%F %T", leaps.when( i ) ) );
        }
    }

    size_t ttcnt
        = std::max( { ff.tzh_ttisutcnt, ff.tzh_ttisstdcnt, ff.tzh_typecnt } );

    fmt::println( "\nType entries:" );
    for( size_t i = 0; i < ttcnt; ++i )
    {
        bool has_ty  = i < ff.tzh_typecnt;
        bool has_std = i < ff.tzh_ttisstdcnt;
        bool has_utc = i < ff.tzh_ttisutcnt;

        std::string _ttinfo = has_ty ? describe( ttinfo[i], designations ) : "";
        std::string _std = has_std ? fmt::format( "isstd={}", is_std[i] ) : "";
        std::string _utc = has_utc ? fmt::format( "isutc={}", is_utc[i] ) : "";

        fmt::println( "  [{}] {:>36} {:>10} {:>10}", i, _ttinfo, _std, _utc );
    }


    if constexpr( kind == vtz::tzfile_kind::modern )
    {
        /// Print the tz string at the end of the file
        println( "\nTZ={}", vtz::escape_string( ff.tz_string() ) );
    }
}

int main( int argc, char const* argv[] ) {
    if( argc <= 1 || argv[1] == nullptr )
    {
        printf( "Usage: dump_tzfile <filename>" );
        return 1;
    }
    char const* path = argv[1];

    auto contents = vtz::read_file_bytes( path );
    auto bytes    = vtz::span<char const>( contents );

    auto file32 = vtz::tzfile32( bytes );

    print_file( file32 );

    if( file32.version_is_modern() )
    {
        auto file64 = file32.load_modern();
        print_file( file64 );
    }
    return 0;
}
