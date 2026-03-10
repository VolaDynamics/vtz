#pragma once

#include <vtz/tz_reader/zone_state.h>

namespace vtz {

    struct zone_transition {
        sys_seconds_t when; // UTC time of change
        zone_state   state;

        zone_transition() = default;
        constexpr zone_transition( i64 when, zone_state state ) noexcept
        : when( when )
        , state( state ) {}

        constexpr zone_transition( none_type ) noexcept
        : when( INT64_MAX )
        , state() {}

        bool     has_value() const noexcept { return when < INT64_MAX; }
        explicit operator bool() const noexcept { return when < INT64_MAX; }
    };
} // namespace vtz
