#pragma once

#include <chrono>
#include <cstdint>
#include <date/date.h>
#include <vtz/format.h>
#include <vtz/impl/chrono_types.h>


inline namespace vtz_test {
    using namespace vtz;

    constexpr vtz::sys_days _get_days(
        int year, unsigned month, unsigned day ) {
        return date::sys_days( date::year_month_day{
            date::year{ year },
            date::month{ month },
            date::day{ day },
        } );
    }

    // Convert a datetime (in terms of year, month, day, hour, minute, and sec)
    // to a sys_seconds object, treating everything as UTC time.
    constexpr vtz::sys_seconds _get_time( int year,
        unsigned                              month,
        unsigned                              day,
        int                                   hour   = 0,
        int                                   minute = 0,
        int                                   second = 0 ) {
        auto T = vtz::sys_seconds( _get_days( year, month, day ) );

        return T + std::chrono::hours{ hour } + std::chrono::minutes{ minute }
               + std::chrono::seconds{ second };
    }

    constexpr vtz::sys_seconds _to_sys( sys_seconds_t s ) {
        return sys_seconds{ std::chrono::seconds{ s } };
    }

    constexpr int64_t _get_time_t( int year,
        unsigned                       month,
        unsigned                       day,
        int                            hour   = 0,
        int                            minute = 0,
        int                            second = 0 ) {
        return _get_time( year, month, day, hour, minute, second )
            .time_since_epoch()
            .count();
    }

    inline vtz::local_seconds _to_local(
        vtz::sys_seconds T, vtz::seconds offset ) {
        return vtz::local_seconds( T.time_since_epoch() + offset );
    }
} // namespace vtz_test


template<class Dur>
std::string _fmt( vtz::sys_time<Dur> T ) {
    return vtz::format( "%F %T time_t=%s", T );
}

#include <gtest/gtest-printers.h>
#include <gtest/gtest.h>
// Add Print support for printing sys_time objects
namespace testing::internal {
    template<class Dur>
    class UniversalPrinter<vtz::sys_time<Dur>> {
      public:

        template<class Sink>
        static void Print( vtz::sys_time<Dur> const& T, Sink* os ) {
            *os << vtz::format( "%F %T (time since epoch=%ss)", T );
        }
    };
    template<class Dur>
    class UniversalPrinter<vtz::local_time<Dur>> {
      public:

        template<class Sink>
        static void Print( vtz::local_time<Dur> const& T, Sink* os ) {
            *os << vtz::format(
                "%F %T (time since epoch=%ss) [local time]", T );
        }
    };
} // namespace testing::internal
