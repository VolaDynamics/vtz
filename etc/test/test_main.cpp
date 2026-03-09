#include <date/tz.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <gtest/gtest.h>
#include <vtz/tz.h>

#if __cpp_lib_filesystem
    #include <filesystem>
    #define HAS_STD_FILESYSTEM 1
#else
    #define HAS_STD_FILESYSTEM 0
#endif


void set_date_install( std::string path = "build/data/tzdata" ) {
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
            ex.what(),
            cwd );
        std::exit( 1 );
    }
}

void set_vtz_install( std::string path = "build/data/tzdata" ) {
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

    set_date_install();

    int i = 1;
    while( i < argc )
    {
        std::string_view arg = argv[i++];
        if( arg == "--vtz_set_install" )
        {
            auto arg = pop_arg( i,
                argc,
                argv,
                "Expected install path after --vtz_set_install" );
            set_vtz_install( std::string( arg ) );
            continue;
        }

        fmt::println( "Unrecognized test arg: {}", arg );
        std::exit( 1 );
    }


    // Run all tests
    return RUN_ALL_TESTS();
}
