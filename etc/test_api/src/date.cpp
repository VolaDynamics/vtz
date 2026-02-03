
#include <chrono>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <vtz/tz.h>

[[noreturn]] void fail( std::string_view msg ) {
    std::fwrite( msg.data(), 1, msg.size(), stderr );
    std::exit( 1 );
}

std::vector<std::string_view> get_args( int argc, char const* argv[] ) {
    if( argc == 0 || argv == nullptr ) return std::vector<std::string_view>();
    return std::vector<std::string_view>( argv + 1, argv + argc );
}

class Args {
    std::vector<std::string_view> args;
    size_t                        pos = 0;

  public:

    Args( int argc, char const* argv[] )
    : args( get_args( argc, argv ) ) {}

    bool try_pop( std::string_view& sv ) noexcept {
        if( pos < args.size() )
        {
            sv = args[pos++];
            return true;
        }
        return false;
    }

    std::string_view pop( std::string_view error ) {
        std::string_view result;
        if( try_pop( result ) ) { return result; }
        fail( error );
    }
};

auto operator""_prefix( char const* s, size_t size ) {
    return [s, size]( std::string_view arg ) noexcept -> bool {
        return arg.size() >= size
               && std::string_view( arg.data(), size )
                      == std::string_view( s, size );
    };
}

int main( int argc, char const* argv[] ) {
    auto args = Args( argc, argv );

    std::string_view fmt = "%a %b %e %H:%M:%S %Z %Y";
    std::string_view zone;
    std::string_view arg;
    bool             use_nanos = false;

    while( args.try_pop( arg ) )
    {
        if( "+"_prefix( arg ) )
        {
            fmt = arg.substr( 1 );
            continue;
        }

        if( "-I"_prefix( arg ) )
        {
            std::string_view prec = arg.substr( 2 );
            if( prec == "" || prec == "date" ) { fmt = "%F"; }
            else if( prec == "hours" ) { fmt = "%FT%H%z"; }
            else if( prec == "minutes" ) { fmt = "%FT%H:%M%z"; }
            else if( prec == "seconds" ) { fmt = "%FT%H:%M:%S%z"; }
            else if( prec == "ns" )
            {
                fmt       = "%FT%H:%M:%S%z";
                use_nanos = true;
            }
            else
            {
                fail( "Bad argument to -I[FMT]. Valid FMT values are 'hours', "
                      "'minutes', 'seconds', or 'ns'" );
            }
            continue;
        }

        if( "-R" == arg )
        {
            fmt = "%a, %d %b %Y %T %z";
            continue;
        }

        if( "-u" == arg )
        {
            zone = "UTC";
            continue;
        }

        if( "-z" == arg )
        {
            zone = args.pop( "Expected timezone name following '-z' argument" );
            continue;
        }
    }


    auto now = std::chrono::system_clock::now();


    auto tz = zone.empty() ? vtz::current_zone() : vtz::locate_zone( zone );

    auto datetime = tz->format_precise( fmt, now, use_nanos ? 9 : 0 );
    puts( datetime.c_str() );
}
