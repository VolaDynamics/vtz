#pragma once

#include <string>

namespace vtz {
    /// Read the full contents of a file as if it's a binary file.
    std::string readFile( char const* fp );

    std::string readFile( std::string const& filepath );
} // namespace vtz
