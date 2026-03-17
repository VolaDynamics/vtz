#include <date/tz.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <vtz/tz.h>

#include <test_vtz/paths.h>


#if __cpp_lib_filesystem
    #include <filesystem>
    #define HAS_STD_FILESYSTEM 1
#else
    #define HAS_STD_FILESYSTEM 0
#endif


void set_date_install( std::string path ) {
    try
    {
        // Try setting the install and locating a timezone
        date::set_install( path );
        (void)date::locate_zone( "America/New_York" );
    }
    catch( std::exception const& ex )
    {
#if HAS_STD_FILESYSTEM
        std::string cwd = std::filesystem::current_path().string();
#else
        std::string cwd = "(<missing std::filesystem>)"
#endif
        fmt::println( stderr,
            "Error when running tests @ cwd={}: date::locate_zone() "
            "fails with the following error:\n\n{}",
            cwd,
            ex.what() );
        std::exit( 1 );
    }
}

void set_vtz_install( std::string path ) {
    try
    {
        // Try setting the install and locating a timezone
        vtz::set_install( path );
        (void)vtz::locate_zone( "America/New_York" );
    }
    catch( std::exception const& ex )
    {
#if HAS_STD_FILESYSTEM
        std::string cwd = std::filesystem::current_path().string();
#else
        std::string cwd = "(<missing std::filesystem>)"
#endif
        fmt::println( stderr,
            "Error when running tests @ cwd={}: vtz::locate_zone() "
            "fails with the following error:\n\n{}",
            cwd,
            ex.what() );
        std::exit( 1 );
    }
}

namespace {
    std::string_view pop_arg(
        int& i, int argc, char** argv, char const* fail_msg ) {
        if( i < argc ) return std::string_view( argv[i++] );

        fmt::println( stderr, "{}", fail_msg );
        std::exit( 1 );
    }
} // namespace


int main( int argc, char** argv ) {
    // Initialize GoogleTest, parses command line arguments and removes all
    // recognized flags
    testing::InitGoogleTest( &argc, argv );

    // If we just listed tests, we don't need to do anything else
    if( ::testing::GTEST_FLAG( list_tests ) )
    {
        // Exit early - don't try loading the tzdb. Because the 'list_tests'
        // flag is set, this doesn't actually run all tests, it just lists
        // tests.
        return RUN_ALL_TESTS();
    }


    bool no_set_install = false;

    int i = 1;
    while( i < argc )
    {
        std::string_view arg = argv[i++];
        if( arg == "--build" )
        {
            auto build     = pop_arg( i,
                argc,
                argv,
                "Expected path to build dir following '--build' flag, eg "
                "./build" );
            paths.tzdata   = _join_fp( build, "data/tzdata" );
            paths.zoneinfo = _join_fp( build, "data/zoneinfo" );
            continue;
        }
        if( arg == "--testdata" )
        {
            auto testdata = pop_arg(
                i, argc, argv, "Expected path to testdata, eg etc/testdata" );
            paths.testdata = std::string( testdata );
            continue;
        }

        // If this argument is set, then `set_vtz_install` will be disabled
        // This is useful for testing against the system's os tzdb files,
        // or for testing the embedded tz database
        if( arg == "--no_set_install" )
        {
            no_set_install = true;
            continue;
        }

        fmt::println( "Unrecognized test arg: {}", arg );
        std::exit( 1 );
    }

    set_date_install( paths.tzdata );
    if( !no_set_install ) { set_vtz_install( paths.tzdata ); }

    // Run all tests
    return RUN_ALL_TESTS();
}
