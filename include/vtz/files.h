#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace vtz {
    using std::string_view;
    using file_bytes = std::vector<char>;

    std::string joinPath( string_view prefix, string_view file );

    std::runtime_error fileError( int errc, string_view fp, string_view verb );

    /// Read the full contents of a file as if it's a binary file.
    std::string readFile( char const* fp );

    std::string readFile( std::string const& filepath );

    /// Read any remaining content within a file, and close the file when
    /// complete.
    file_bytes readFileBytes( std::FILE* file, char const* fp );

    /// Read the full contents of a file as if it's a binary file.
    file_bytes readFileBytes( char const* fp );

    file_bytes readFileBytes( std::string const& filepath );
} // namespace vtz
