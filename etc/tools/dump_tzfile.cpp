#include <vtz/strings.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <vtz/files.h>
#include <vtz/tz.h>
#include <vtz/tz_reader/FromUTC.h>
#include <vtz/tzfile.h>

using color = fmt::terminal_color;
using fmt::fg;
using fmt::print;
using fmt::println;
using fmt::styled;
using em = fmt::emphasis;

using std::string_view;

constexpr auto bold_blue    = fg( color::blue ) | em::bold;
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

template<vtz::tzfile_kind kind>
void print_file(
    vtz::basic_tzfile<kind> const& file32, string_view title = "TZif Header" ) {
    print_tzif_header( file32.header(), title );

    auto utc = vtz::TimeZone::utc();

    size_t timecnt      = file32.tzh_timecnt;
    auto   TT           = file32.transition_times();
    auto   types        = file32.type_indices();
    auto   ttinfo       = file32.ttinfo();
    auto   designations = file32.abbrev_buff();

    for( size_t i = 0; i < timecnt; ++i )
    {
        // Print transition times
        int64_t T        = TT[i];
        auto    type     = types[i];
        auto    info     = ttinfo[type];
        auto    utoff    = info.tt_utoff();
        auto    isdst    = info.tt_isdst();
        auto    desigidx = info.tt_desigidx();

        char const* abbr = designations.data() + desigidx;

        println( "utc={} local={} utoff={} isdst={} abbr={}",
            styled( utc.format_s( "%F %T (dow=%w)", T ), bold_magenta ),
            styled( utc.format_s( "%F %T", T + utoff ), bold_blue ),
            styled( vtz::FromUTC( utoff ), bold_green ),
            styled( int( isdst ), bold_white ),
            styled( abbr, bold_red ) );
    }

    if constexpr( kind == vtz::tzfile_kind::modern )
    {
        /// Print the tz string at the end of the file
        println( "TZ={}", vtz::escape_string( file32.tz_string() ) );
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
