#include <algorithm>
#include <charconv>
#include <vtz/strings.h>

namespace vtz {
    namespace {
        size_t countNewlines( string_view text ) {
            return std::count( text.data(), text.data() + text.size(), '\n' );
        }
    } // namespace

    Location Location::wherePtr( string_view body, char const* pos ) noexcept {
        return Location::where( body, size_t( pos - body.data() ) );
    }

    Location Location::where( string_view body, size_t pos ) noexcept {
        // Return empty location if the position is outside the body
        if( pos > body.size() ) return {};

        return Location::where( string_view( body.data(), pos ) );
    }

    Location Location::where( string_view substr ) noexcept {
        // start counting from the character after the newline
        // if no newline is found, rfind() will return npos, which
        // is size_t(-1). This will result in line_start being 0,
        // which is correct since we're measuring from the beginning of the
        // string.
        size_t line_start = substr.rfind( '\n' ) + 1;

        // line index
        size_t li = countNewlines( substr );

        // column index
        size_t ci = substr.size() - line_start;

        return Location{ li + 1, ci + 1 };
    }

    vector<string_view> tokenize( string_view line ) {
        size_t max_size = ( line.size() + 1 ) / 2;

        auto result = vector<string_view>( max_size );

        // Fill vector with tokens
        size_t    i = 0;
        TokenIter tokens( line );
        while( auto tok = tokens.next() ) { result[i++] = tok; }

        result.resize( i );
        return result;
    }

    size_t countLines( string_view input ) {
        if( input.empty() ) return 0;

        size_t count = countNewlines( input );

        // If the last line ends in a newline, the number of lines in identical
        // to the number of newlines
        if( input.back() == '\n' ) { return count; }
        // Otherwise, there's one extra line
        return count + 1;
    }


    vector<string_view> lines( string_view input ) {
        auto result = vector<string_view>( countLines( input ) );

        LineIter r( input );
        for( auto& elem : result ) { elem = r.next(); }
        return result;
    }


    std::string Location::str() const {
        char buffer[64];

        auto r1 = std::to_chars( buffer, buffer + sizeof( buffer ), line );
        *r1.ptr = ':';
        auto r2 = std::to_chars( r1.ptr + 1, buffer + sizeof( buffer ), col );
        return std::string( buffer, r2.ptr - buffer );
    }


    std::string escapeString( std::string_view sv ) {
        std::string s;
        s += '"';
        for( char ch : sv )
        {
            switch( ch )
            {
            case '\0': s += "\\0"; break;
            case '\\': s += "\\\\"; break;
            case '\a': s += "\\a"; break;
            case '\b': s += "\\b"; break;
            case '\f': s += "\\f"; break;
            case '\n': s += "\\n"; break;
            case '\r': s += "\\r"; break;
            case '\t': s += "\\t"; break;
            case '\v': s += "\\v"; break;
            default:
                {
                    uint8_t value( ch );
                    if( value < 32 || value >= 127 )
                    {
                        int  d1 = value >> 4;
                        int  d0 = value & 0xf;
                        char h1
                            = d1 >= 10 ? ( d1 + ( 'A' - 10 ) ) : ( d1 + '0' );
                        char h0
                            = d0 >= 10 ? ( d0 + ( 'A' - 10 ) ) : ( d0 + '0' );

                        char buff[4]{ '\\', 'x', h1, h0 };
                        s.append( buff, 4 );
                    }
                    else { s += ch; }
                }
            }
        }
        s += '"';
        return s;
    }
} // namespace vtz
