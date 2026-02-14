#pragma once

#include <cstddef>
#include <cstdint>
#include <vtz/impl/macros.h>
#include <vtz/types.h>
#include <vtz/impl/math.h>

namespace vtz::detail {
    /// Compute the amount of precision necessary to represent some duration
    constexpr int get_necessary_precision(
        bool is_floating_point, intmax_t num, intmax_t denom ) noexcept {
        // if it's floating-point, use max precision
        if( is_floating_point ) return 9;

        // if the numerator is divisible by the denominator, we can use 0 for
        // the precision
        if( num % denom == 0 ) return 0;

        if( 10 % denom == 0 ) return 1;
        if( 100 % denom == 0 ) return 2;
        if( 1000 % denom == 0 ) return 3;
        if( 10000 % denom == 0 ) return 4;
        if( 100000 % denom == 0 ) return 5;
        if( 1000000 % denom == 0 ) return 6;
        if( 10000000 % denom == 0 ) return 7;
        if( 100000000 % denom == 0 ) return 8;

        return 9;
    }

    template<class Duration>
    constexpr int get_necessary_precision() {
        using rep    = typename Duration::rep;
        using period = typename Duration::period;
        // period::num is the numerator, period::den is the denominator
        return get_necessary_precision(
            std::is_floating_point<rep>(), period::num, period::den );
    }
} // namespace vtz::detail

namespace vtz {
    // clang-format off

    /// Get the number of seconds since the epoch
    VTZ_INLINE constexpr sec_t ns_to_s( nanos_t ns ) noexcept { return vtz::math::div_floor<1000000000>( ns ); }
    VTZ_INLINE constexpr nanos_t s_to_ns( sec_t s ) noexcept { return s * 1000000000; }

    // clang-format on
} // namespace vtz
