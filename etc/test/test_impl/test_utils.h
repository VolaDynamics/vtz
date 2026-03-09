#pragma once

#include <vtz/civil.h>
#include <chrono>

using namespace vtz;
using std::chrono::duration;

constexpr sysseconds_t _ct( i32 y, u32 m, u32 d, int h, int min, int sec ) {
    return resolve_civil_time( y, m, d, h, min, sec );
}
