#include <algorithm>
#include <charconv>
#include <vtz/strings.h>

namespace vtz {
    namespace {
        size_t count_newlines( string_view text ) {
            return std::count( text.data(), text.data() + text.size(), '\n' );
        }
    } // namespace

    location location::where_ptr( string_view body, char const* pos ) noexcept {
        return location::where( body, size_t( pos - body.data() ) );
    }

    location location::where( string_view body, size_t pos ) noexcept {
        // Return empty location if the position is outside the body
        if( pos > body.size() ) return {};

        return location::where( string_view( body.data(), pos ) );
    }

    location location::where( string_view substr ) noexcept {
        // start counting from the character after the newline
        // if no newline is found, rfind() will return npos, which
        // is size_t(-1). This will result in line_start being 0,
        // which is correct since we're measuring from the beginning of the
        // string.
        size_t line_start = substr.rfind( '\n' ) + 1;

        // line index
        size_t li = count_newlines( substr );

        // column index
        size_t ci = substr.size() - line_start;

        return location{ li + 1, ci + 1 };
    }

    vector<string_view> tokenize( string_view line ) {
        size_t max_size = ( line.size() + 1 ) / 2;

        auto result = vector<string_view>( max_size );

        // Fill vector with tokens
        size_t     i = 0;
        token_iter tokens( line );
        while( auto tok = tokens.next() ) { result[i++] = tok; }

        result.resize( i );
        return result;
    }

    size_t count_lines( string_view input ) {
        if( input.empty() ) return 0;

        size_t count = count_newlines( input );

        // If the last line ends in a newline, the number of lines in identical
        // to the number of newlines
        if( input.back() == '\n' ) { return count; }
        // Otherwise, there's one extra line
        return count + 1;
    }


    vector<string_view> lines( string_view input ) {
        auto result = vector<string_view>( count_lines( input ) );

        line_iter r( input );
        for( auto& elem : result ) { elem = r.next(); }
        return result;
    }


    std::string location::str() const {
        char buffer[64];

        auto r1 = std::to_chars( buffer, buffer + sizeof( buffer ), line );
        *r1.ptr = ':';
        auto r2 = std::to_chars( r1.ptr + 1, buffer + sizeof( buffer ), col );
        return std::string( buffer, r2.ptr - buffer );
    }
} // namespace vtz
