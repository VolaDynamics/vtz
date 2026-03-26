#pragma once

// Not all compilers have __has_builtin. We use _vtz_has_builtin to
// check for builtins. On compilers that do have it, _vtz_has_builtin
// Will return __has_builtin, and on compilers that don't have it,
// by default we assume that the builtin is missing.
#ifdef __has_builtin
    #define _vtz_has_builtin( x ) __has_builtin( x )
#else
    #define _vtz_has_builtin( x ) 0
#endif

#if _MSC_VER
    #include <cstring>
    #define _vtz_memcpy memcpy
#else
    // Compilers conforming to __GNUC__ should have __builtin_memcpy
    #if defined( __GNUC__ ) || _vtz_has_builtin( __builtin_memcpy )
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
