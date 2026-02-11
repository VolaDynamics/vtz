#ifndef VTZ_EXPORT_H
#define VTZ_EXPORT_H

#ifdef VTZ_STATIC_DEFINE
#  define VTZ_EXPORT
#  define VTZ_NO_EXPORT
#else
#  ifdef _WIN32
#    ifdef vtz_EXPORTS
#      define VTZ_EXPORT __declspec(dllexport)
#    else
#      define VTZ_EXPORT __declspec(dllimport)
#    endif
#    define VTZ_NO_EXPORT
#  else
#    define VTZ_EXPORT __attribute__((visibility("default")))
#    define VTZ_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef VTZ_DEPRECATED
#  ifdef _WIN32
#    define VTZ_DEPRECATED __declspec(deprecated)
#  else
#    define VTZ_DEPRECATED __attribute__((__deprecated__))
#  endif
#endif

#ifndef VTZ_DEPRECATED_EXPORT
#  define VTZ_DEPRECATED_EXPORT VTZ_DEPRECATED VTZ_EXPORT
#endif

#endif /* VTZ_EXPORT_H */
