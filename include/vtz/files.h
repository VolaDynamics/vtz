#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace vtz {
    using std::string_view;

#ifdef _WIN32
    constexpr char FILE_SEP = '\\';
#else
    constexpr char FILE_SEP = '/';
#endif

    std::runtime_error fileError( int errc, string_view fp, string_view verb );

    /// Read the full contents of a file as if it's a binary file.
    std::string readFile( char const* fp );

    std::string readFile( std::string const& filepath );
} // namespace vtz
