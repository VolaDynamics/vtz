#include <benchmark/benchmark.h>
#include <date/tz.h>
#include <fmt/core.h>
#include <vtz/tz.h>

int main( int argc, char** argv ) {
    try
    {
        benchmark::Initialize( &argc, argv );
        if( benchmark::ReportUnrecognizedArguments( argc, argv ) ) return 1;

        vtz::set_install( "build/data/tzdata" );
        date::set_install( "build/data/tzdata" );
        benchmark::RunSpecifiedBenchmarks();
        benchmark::Shutdown();
        return 0;
    }
    catch( std::exception const& ex )
    {
        fmt::println( "Error: {}", ex.what() );
        return 1;
    }
    catch( ... )
    {
        fmt::println( "Failed with unknown exception." );
        return 1;
    }
}
