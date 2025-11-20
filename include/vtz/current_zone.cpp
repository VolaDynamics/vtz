#include <cerrno>
#include <fmt/format.h>
#include <stdexcept>
#include <vtz/files.h>
#include <vtz/span.h>
#include <vtz/tz.h>

#include <optional>

namespace vtz::win32 {
    using std::string_view;
    constexpr size_t npos = string_view::npos;

    string_view resolveNative( string_view data, string_view name ) {
        std::string key;
        key.reserve( name.size() + 2 );
        key     += '"';
        key     += name;
        key     += '"';
        auto p0  = data.find( key );
        if( p0 == npos )
        {
            throw std::runtime_error( fmt::format(
                "Unable to find '{}' within windowsZones.xml", key ) );
        }
        p0      += key.size();
        auto p1  = data.find( "type=\"", p0 );
        if( p1 == npos )
        {
            throw std::runtime_error( fmt::format(
                "'{}' appeared in windowsZones.xml but the associated zone "
                "could not be determined (expected 'type=\"')",
                key ) );
        }
        p1 += 6;

        // Take the zone name
        string_view result = data.substr( p1 );
        return result.substr( 0, result.find_first_of( "\" " ) );
    }

    std::string loadWindowsZones() {
        return readFile( joinPath( get_install(), "windowsZones.xml" ) );
    }

    string_view windowsZoneToNative( string_view windowsZone ) {
        static std::string windowsZones = loadWindowsZones();
        return resolveNative( windowsZones, windowsZone );
    }
} // namespace vtz::win32


#ifdef _WIN32
    #include <windows.h>

namespace vtz::win32 {
    static std::string _get_current_timezone_name() {
        DYNAMIC_TIME_ZONE_INFORMATION tzi{};
        auto result = GetDynamicTimeZoneInformation( &tzi );
        if( result == TIME_ZONE_ID_INVALID )
            throw std::runtime_error( "Unable to determine current zone: "
                                      "GetDynamicTimeZoneInformation()"
                                      " reported TIME_ZONE_ID_INVALID." );

        auto wlen = wcslen( tzi.TimeZoneKeyName );

        char buf[1024];

        if( wlen < sizeof( buf ) )
        {
            size_t      count = wcstombs( buf, tzi.TimeZoneKeyName, wlen );
            string_view result( buf, count );
            if( result == "Coordinated Universal Time" ) return "UTC";
            return std::string( result );
        }

        throw std::runtime_error(
            "GetDynamicTimeZoneInformation() returned a timezone "
            "name that was too long." );
    }
} // namespace vtz::win32

#else

    #include <unistd.h>

    // This is madness
    #undef unix

namespace vtz::unix {
    /// Attempt to read a link into the given destination buffer. Throws if an
    /// error occurred.
    ///
    /// @return Returns the number of bytes read into the dest on success,
    /// nullopt if truncation may have occurred
    static std::optional<size_t> _try_readlink(
        char const* link, span<char> dest ) {
        ssize_t result = readlink( link, dest.data(), dest.size() );

        // If result < 0, this indicates an error
        if( result < 0 ) throw fileError( errno, link, "reading symlink" );

        // returns valid optional if there was no truncation
        if( result < dest.size() ) return result;

        return std::nullopt;
    }

    template<class F>
    static auto _readlink( char const* link, F func )
        -> decltype( func( string_view() ) ) {
        // Try reading the link into a stack-allocated buffer
        char buf[4096];

        if( auto sz = _try_readlink( link, span( buf ) ) )
        {
            // We read a valid link!!
            return func( string_view( buf, *sz ) );
        }

        // Attempt to read the link into progressively larger heap-allocated
        // buffers
        size_t bufSize = sizeof( buf ) * 2;

        // Stop after 10 doublings... the link must be insanely large
        for( int i = 0; i < 10; ++i )
        {
            auto p = std::unique_ptr<char[]>( new char[bufSize] );

            if( auto sz = _try_readlink( link, span( p.get(), bufSize ) ) )
            {
                // We read a valid link!
                return func( string_view( p.get(), *sz ) );
            }

            // Try reading with a bigger buffer
            bufSize *= 2;
        }

        throw std::runtime_error( fmt::format(
            "Error when attempting to read {}: readlink() "
            "returned an insanely long symlink. Link (truncated): {}",
            escapeString( link ),
            escapeString( string_view( buf, sizeof( buf ) ) ) ) );
    }

    static OptSV _get_by_zoneinfo( string_view path ) {
        string_view term = "zoneinfo/";
        auto        p    = path.rfind( term );
        if( p != string_view::npos )
        {
            // Return the name as the portion of the path name after 'zoneinfo/
            return path.substr( p + term.size() );
        }

        return std::nullopt;
    }

    static OptSV _get_tzname_fallback( string_view path ) {
        constexpr static string_view _prefixes[]{
            "America/",
            "Europe/",
            "Asia/",
            "Africa/",
            "Antarctica/",
            "Atlantic/",
            "Australia/",
            "Etc/",
            "Indian/",
            "Pacific/",
        };
        for( auto const& prefix : _prefixes )
        {
            auto p = path.rfind( prefix );
            if( p != string_view::npos ) return path.substr( p );
        }

        return std::nullopt;
    }

    // Resolve a timezone name from a link
    constexpr static auto _resolve_tzname
        = []( string_view path ) -> std::string {
        // Search for 'zoneinfo/'
        if( auto result = _get_by_zoneinfo( path ) )
            return std::string( result );

        // Search for a known timezone prefix
        if( auto result = _get_tzname_fallback( path ) )
            return std::string( result );

        throw std::runtime_error(
            fmt::format( "Unable to determine current timezone. "
                         "/etc/localtime resolved to {}, which does not "
                         "contain either 'zoneinfo/' or a valid zone name.",
                escapeString( path ) ) );
    };
} // namespace vtz::unix

#endif


namespace vtz {
    std::string _get_current_zone_name() {
#ifdef _WIN32
        return std::string(
            win32::windowsZoneToNative( win32::_get_current_timezone_name() ) );
#else
        return unix::_readlink( "/etc/localtime", unix::_resolve_tzname );
#endif
    }


    time_zone const* current_zone() {
        static time_zone const* _zone = locate_zone( _get_current_zone_name() );
        return _zone;
    }
} // namespace vtz
