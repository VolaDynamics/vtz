#pragma once

#include <string>

namespace vtz {
    /// Read the full contents of a file as if it's a binary file.
    std::string read_file( char const* fp );

    std::string read_file( std::string const& filepath );
} // namespace vtz
