#include <chrono>
#include <fmt/format.h>
#include <vtz/tz.h>


int main() {
    auto ny        = vtz::locate_zone( "America/New_York" );
    auto london    = vtz::locate_zone( "Europe/London" );
    auto amsterdam = vtz::locate_zone( "Europe/Amsterdam" );

    auto current_zone = vtz::current_zone();

    auto now = std::chrono::system_clock::now();

    fmt::println( "Current Time:" );
    fmt::println( "New York:     {}", ny->format( "%F %T %Z", now ) );
    fmt::println( "London:       {}", london->format( "%F %T %Z", now ) );
    fmt::println( "Amsterdam:    {}", amsterdam->format( "%F %T %Z", now ) );
    fmt::println( "Current Zone: {} (name={})",
        current_zone->format( "%F %T %Z", now ),
        current_zone->name() );
}
