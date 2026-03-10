#pragma once


#include <string_view>
#include <vtz/impl/chrono_types.h>
#include <vtz/impl/export.h>
#include <vtz/types.h>


namespace vtz::_unzoned {
    using std::string_view;

    VTZ_EXPORT std::string format_d( string_view fmt, sys_days_t days );


    VTZ_EXPORT size_t format_to_d(
        string_view fmt, sys_days_t days, char* buff, size_t count );


    VTZ_EXPORT std::string format_s( string_view fmt, sys_seconds_t t );


    VTZ_EXPORT size_t format_to_s(
        string_view fmt, sys_seconds_t t, char* buff, size_t count );


    VTZ_EXPORT std::string format_precise_s(
        string_view fmt, sys_seconds_t t, u32 nanos, int precision );

    VTZ_EXPORT size_t format_precise_to_s( string_view fmt,
        sys_seconds_t                                  t,
        u32                                            nanos,
        int                                            precision,
        char*                                          buff,
        size_t                                         count );
} // namespace vtz::_unzoned
