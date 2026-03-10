#pragma once

#include <cstddef>
#include <cstdint>

namespace vtz {
    using u8  = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    using i8  = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;
    using i64 = int64_t;

    using sec_t         = i64;
    using sys_seconds_t = i64;
    using nanos_t       = i64;

    using sys_days_t = i32;
} // namespace vtz
