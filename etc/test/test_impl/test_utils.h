#pragma once

#include <chrono>
#include <vtz/civil.h>
#include <vtz/impl/chrono_types.h>

using namespace vtz;
using std::chrono::duration;

constexpr sec_t _ct( i32 y, u32 m, u32 d, int h, int min, int sec ) {
    return resolve_civil_time( y, m, d, h, min, sec );
}


/// Convert raw time in seconds to sys_seconds
inline vtz::sys_seconds as_sys( sysseconds_t t ) { return vtz::sys_seconds( vtz::seconds( t ) ); }

/// Convert raw time in seconds to sys_seconds
inline vtz::local_seconds as_local( sec_t t ) { return vtz::local_seconds( vtz::seconds( t ) ); }

inline vtz::sys_seconds _cts( i32 y, u32 m, u32 d, int h, int min, int sec ) {
    return as_sys( resolve_civil_time( y, m, d, h, min, sec ) );
}

inline vtz::local_seconds _ctl( i32 y, u32 m, u32 d, int h, int min, int sec ) {
    return as_local( resolve_civil_time( y, m, d, h, min, sec ) );
}
