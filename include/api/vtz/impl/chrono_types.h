#pragma once

#include <chrono>
#include <string>
#include <vtz/types.h>

namespace vtz {
    using std::chrono::floor;
    using std::chrono::hours;
    using std::chrono::minutes;
    using std::chrono::nanoseconds;
    using std::chrono::seconds;

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


    struct sys_info {
        sys_seconds begin;
        sys_seconds end;
        seconds     offset;
        minutes     save;
        std::string abbrev;

        bool operator==( sys_info const& rhs ) const noexcept {
            return begin == rhs.begin      //
                   && end == rhs.end       //
                   && offset == rhs.offset //
                   && save == rhs.save     //
                   && abbrev == rhs.abbrev;
        }
    };

    /// This class describes the result of converting a `local_time` to a
    /// `sys_time`.
    ///
    /// - If the result of the conversion is unique, then `result ==
    ///   local_info::unique`, `first` is filled out with the correct
    ///   `sys_info`, and second is zero-initialized.
    /// - If the `local_time` is nonexistent, then `result ==
    ///   local_info::nonexistent`, `first` is filled out with the `sys_info`
    ///   that ends just prior to the `local_time`, and second is filled out
    ///   with the `sys_info` that begins just after the `local_time`.
    /// - If the `local_time` is ambiguous, then `result ==
    ///   local_info::ambiguous`, `first` is filled out with the `sys_info` that
    ///   ends just after the `local_time`, and second is filled with the
    ///   `sys_info` that starts just before the `local_time`.
    ///
    /// This is a low-level data structure; typical conversions from local_time
    /// to sys_time will use it implicitly rather than explicitly.

    struct local_info {
        constexpr static int unique      = 0;
        constexpr static int nonexistent = 1;
        constexpr static int ambiguous   = 2;

        int      result;
        sys_info first;
        sys_info second;

        bool operator==( local_info const& rhs ) const noexcept {
            return result == rhs.result  //
                   && first == rhs.first //
                   && second == rhs.second;
        }
    };
} // namespace vtz
#ifdef VTZ_DATE_COMPAT
namespace date = vtz;
#endif
