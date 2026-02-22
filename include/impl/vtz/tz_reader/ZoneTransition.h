#pragma once

#include <vtz/tz_reader/ZoneState.h>

namespace vtz {

    struct ZoneTransition {
        sysseconds_t when; // UTC time of change
        ZoneState    state;

        ZoneTransition() = default;
        constexpr ZoneTransition( i64 when, ZoneState state ) noexcept
        : when( when )
        , state( state ) {}

        constexpr ZoneTransition( none_type ) noexcept
        : when( INT64_MAX )
        , state() {}

        bool     has_value() const noexcept { return when < INT64_MAX; }
        explicit operator bool() const noexcept { return when < INT64_MAX; }
    };
} // namespace vtz
