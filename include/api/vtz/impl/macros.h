#pragma once


#if _MSC_VER
    #include <cstring>
    #define _vtz_memcpy memcpy
#else
    #if __has_builtin( __builtin_memcpy )
        #define _vtz_memcpy __builtin_memcpy
    #else
        #include <cstring>
        #define _vtz_memcpy memcpy
    #endif
#endif

#if _MSC_VER
    #define VTZ_INLINE            __forceinline
    #define VTZ_IS_LIKELY( expr ) bool( expr )
    #define VTZ_UNREACHABLE()     __assume( 0 )
    #if _MSVC_LANG >= 202002L
        #define VTZ_LIKELY [[likely]]
    #else
        #define VTZ_LIKELY
    #endif
#else
    #define VTZ_INLINE            [[gnu::always_inline]] inline
    #define VTZ_IS_LIKELY( expr ) __builtin_expect( bool( expr ), 1 )
    #define VTZ_UNREACHABLE()     __builtin_unreachable()
    #define VTZ_LIKELY            [[likely]]
#endif
