#pragma once

#include <vtz/impl/chrono_types.h>
#include <vtz/types.h>

#include <date/date.h>

inline namespace vtz_test {
    using namespace vtz;

    // Convert a datetime (in terms of year, month, day, hour, minute, and sec)
    // to a sys_seconds object, treating everything as UTC time.
    inline vtz::sys_seconds _get_time( int year,
        unsigned                    month,
        unsigned                    day,
        int                         hour   = 0,
        int                         minute = 0,
        int                         second = 0 ) {
        auto days = date::sys_days( date::year_month_day{
            date::year{ year }, date::month{ month }, date::day{ day } } );

        return days + std::chrono::hours{ hour }
               + std::chrono::minutes{ minute }
               + std::chrono::seconds{ second };
    }

    inline vtz::sys_seconds _to_sys( sysseconds_t s ) {
        return sys_seconds{ std::chrono::seconds{ s } };
    }
} // namespace vtz_test
