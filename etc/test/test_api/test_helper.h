#pragma once

#include <vtz/impl/chrono_types.h>
#include <vtz/types.h>
#include <vtz/format.h>

#include <date/date.h>

#include <gtest/gtest.h>

inline namespace vtz_test {
    using namespace vtz;

    inline vtz::sys_days _get_days(int year, unsigned month, unsigned day) {
        return date::sys_days( date::year_month_day{
            date::year{ year },
            date::month{ month },
            date::day{ day },
        } );
    }

    // Convert a datetime (in terms of year, month, day, hour, minute, and sec)
    // to a sys_seconds object, treating everything as UTC time.
    inline vtz::sys_seconds _get_time( int year,
        unsigned                    month,
        unsigned                    day,
        int                         hour   = 0,
        int                         minute = 0,
        int                         second = 0 ) {
        auto T = vtz::sys_seconds( _get_days(year, month, day) );

        return T + std::chrono::hours{ hour }
               + std::chrono::minutes{ minute }
               + std::chrono::seconds{ second };
    }

    inline vtz::sys_seconds _to_sys( sysseconds_t s ) {
        return sys_seconds{ std::chrono::seconds{ s } };
    }
} // namespace vtz_test


// Add Print support for printing sys_time objects
namespace testing::internal {
template <class Dur>
class UniversalPrinter<vtz::sys_time<Dur>> {
  public:
    template <class Sink>
    static void Print( vtz::sys_time<Dur> const& T, Sink* os ) {
        *os << vtz::format("%F %T (time since epoch=%ss)", T);
    }
};
}
