#include <string>
#include <test_vtz/paths.h>

path_settings paths = path_settings();

using std::string_view;

namespace {
    constexpr bool is_file_sep( char ch ) {
#if _WIN32
        return ch == '\\' || ch == '/';
#else
        return ch == '/';
#endif
    }

#ifdef _WIN32
    constexpr char FILE_SEP = '\\';
#else
    constexpr char FILE_SEP = '/';
#endif


    /// Returns true if the given path is absolute
    ///
    /// - '' -> false
    /// - 'C:/...' -> true (on windows) (returns true for other drive
    /// letters)
    /// - /... -> true (any path starting with a path separator is true)
    bool is_absolute( string_view file ) noexcept {
        if( file.empty() ) return false;

#if _WIN32
        if( file.size() >= 3 && file[1] == ':' && is_file_sep( file[2] ) )
            return true;
#endif

        return is_file_sep( file[0] );
    }
} // namespace


std::string _join_fp( string_view prefix, string_view file ) {
    // If the prefix is empty or the file is absolute, just return the file
    if( prefix.empty() || is_absolute( file ) ) return std::string( file );

    // If it already ends in a file separator, just add the file
    if( is_file_sep( prefix.back() ) )
    {
        std::string result;
        result.reserve( prefix.size() + file.size() );
        result.append( prefix );
        result.append( file );
        return result;
    }

    std::string result;
    result.reserve( prefix.size() + file.size() + 1 );
    result.append( prefix );
    result.push_back( FILE_SEP );
    result.append( file );
    return result;
}
