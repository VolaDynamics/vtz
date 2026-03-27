#include <cstdio>
#include <vtz/tz.h>


[[noreturn]] void fail( char const* msg ) {
    puts( msg );
    std::fflush( stdout );
    std::exit( 1 );
}

void do_test( int argc, char const* argv[] ) {
    if( vtz::is_tzdb_loaded() == true )
        fail( "Timezone database should not have been loaded yet" );

    /// Set the install path, if it's been provided as a cli argument
    if( argc > 1 ) vtz::set_install( argv[1] );

    if( vtz::is_tzdb_loaded() == true )
        fail( "Setting the install path should not load the timezone "
              "database" );

    auto tz = vtz::locate_zone( "America/New_York" );

    if( tz == nullptr )
        fail( "Unable to locate America/New_York - zone incorrectly returned "
              "null" );

    if( vtz::is_tzdb_loaded() == false )
        fail( "is_tzdb_loaded returns false, even though it _should_ return "
              "true, because we were able to locate a zone." );
}

int main( int argc, char const* argv[] ) {
    try
    {
        // run test
        do_test( argc, argv );
    }
    catch( std::exception const& ex )
    {
        // Fail with error message
        fail( ex.what() );
    }
}
