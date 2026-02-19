#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vtz {
    using std::string_view;
    using file_bytes = std::vector<char>;

    std::string join_path( string_view prefix, string_view file );

    std::runtime_error file_error( int errc, string_view fp, string_view verb );

    /// Read the full contents of a file as if it's a binary file.
    std::string read_file( char const* fp );

    std::string read_file( std::string const& filepath );

    /// Read any remaining content within a file, and close the file when
    /// complete.
    file_bytes read_file_bytes( std::FILE* file, char const* fp );

    /// Read the full contents of a file as if it's a binary file.
    file_bytes read_file_bytes( char const* fp );

    file_bytes read_file_bytes( std::string const& filepath );
} // namespace vtz
