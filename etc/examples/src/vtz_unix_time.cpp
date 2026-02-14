#include <ctime>
#include <fmt/format.h>
#include <vtz/tz.h>



int main() {
    // vtz::locate_zone() returns a pointer to a timezone

    // This timezone is managed by vtz, and will remain valid for the whole
    // lifetime of the program.

    // See also: https://en.cppreference.com/w/cpp/chrono/locate_zone.html

    auto ny        = vtz::locate_zone( "America/New_York" );
    auto london    = vtz::locate_zone( "Europe/London" );
    auto amsterdam = vtz::locate_zone( "Europe/Amsterdam" );

    // vtz::current_zone() returns a pointer to the current timezone.
    // For any timezone, you gat get the name with .name()
    auto here = vtz::current_zone();

    /// Get the current time (in seconds)
    time_t now = std::time( nullptr );

    fmt::println( "Current Time:" );
    fmt::println( "New York:     {}", ny->format_s( "%F %T %Z", now ) );
    fmt::println( "London:       {}", london->format_s( "%F %T %Z", now ) );
    fmt::println( "Amsterdam:    {}", amsterdam->format_s( "%F %T %Z", now ) );


    // Time within the current zone:

    fmt::println( "Current Zone: {} (name={})",
        here->format_s( "%F %T %Z", now ),
        here->name() );
}
