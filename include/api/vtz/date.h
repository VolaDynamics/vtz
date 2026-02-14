#pragma once

#include <chrono>
#include <vtz/types.h>

namespace vtz {
    using std::chrono::hours;
    using std::chrono::minutes;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;
    using std::chrono::floor;

    using days = std::chrono::duration<i32, std::ratio<86400>>;
    using std::chrono::system_clock;
    using std::chrono::time_point;

    template<class Duration>
    using sys_time    = time_point<system_clock, Duration>;
    using sys_seconds = sys_time<seconds>;
    using sys_days    = sys_time<days>;

    struct local_t {};
    template<class Duration>
    using local_time = std::chrono::time_point<local_t, Duration>;

    using local_seconds = local_time<seconds>;
    using local_days    = local_time<days>;
} // namespace vtz
#ifdef VTZ_DATE_COMPAT
namespace date = vtz;
#endif
