#pragma once

#include <algorithm>
#include <optional>
#include <string_view>
#include <vector>

namespace vtz {
    using std::string_view;

    /// Optional-like wrapper around a string_view. This optional is empty if
    /// data() == nullptr
    struct opt_sv : string_view
    {
        using string_view::string_view;

        // Allow construction from base
        constexpr opt_sv( string_view rhs ) noexcept
        : string_view( rhs )
        {
        }

        constexpr opt_sv( std::nullopt_t ) noexcept
        : string_view()
        {
        }

        constexpr string_view value() const noexcept { return *this; }

        constexpr bool has_value() const noexcept { return data() != nullptr; }

        constexpr string_view value_or( string_view rhs ) const noexcept
        {
            if( data() == nullptr ) { return rhs; }
            else { return *this; }
        }

        constexpr explicit operator bool() const noexcept
        {
            return has_value();
        }
    };

    constexpr bool is_delim( char ch ) { return ch == ' ' || ch == '\t'; }

    struct token_iter
    {
      private:

        char const* b_ = nullptr;
        char const* e_ = nullptr;

        constexpr static char const* find_delim(
            char const* p, char const* end ) noexcept
        {
            while( p < end )
            {
                if( is_delim( *p ) ) return p;
                ++p;
            }
            return end;
        }
        constexpr static char const* find_non_delim(
            char const* p, char const* end ) noexcept
        {
            while( p < end )
            {
                if( !is_delim( *p ) ) return p;
                ++p;
            }
            return end;
        }

      public:

        constexpr token_iter() = default;

        constexpr token_iter( string_view line ) noexcept
        : b_( line.data() )
        , e_( line.data() + line.size() )
        {
        }

        void clear() noexcept { b_ = e_; }

        opt_sv next()
        {
            auto start = find_non_delim( b_, e_ );

            if( start == e_ )
            {
                clear();
                return std::nullopt;
            }

            auto end = find_delim( start + 1, e_ );

            auto result = string_view( start, size_t( end - start ) );

            // start searching from 1 past the last character we checked
            if( end < e_ ) { b_ = end + 1; }
            else { clear(); }

            return result;
        }
    };


    // Tokenize a line, ignoring comments. Write the resulting tokens to the
    // output buffer. Return the number of written tokens.
    inline size_t tokenize( char const* line, size_t count, string_view* out )
    {
        auto comment_start = string_view( line, count ).find( '#' );
        if( comment_start != string_view::npos ) { count = comment_start; }

        size_t i = 0;
        size_t n = 0; // Number of tokens we discovered
        for( ;; )
        {
            while( i < count && is_delim( line[i] ) ) ++i;
            if( i == count ) break;
            size_t tok_start = i;
            while( i < count && !is_delim( line[i] ) ) ++i;
            out[n++] = string_view( line + tok_start, i - tok_start );
            if( i == count ) break;
        }
        return n;
    }

    /// Tokenize a line. Return a vector of tokens
    inline std::vector<string_view> tokenize( string_view line )
    {
        size_t max_size = ( line.size() + 1 ) / 2;

        auto result = std::vector<string_view>( max_size );

        // Fill vector with tokens
        size_t     i = 0;
        token_iter tokens( line );
        while( auto tok = tokens.next() )
        {
            result[i++] = tok;
        }

        result.resize( i );
        return result;
    }


    inline string_view strip_comment( string_view line )
    {
        auto pos = line.find( '#' );
        if( pos == string_view::npos ) { return line; }
        return string_view( line.data(), pos );
    }


    // If a line ends in a '\r' character, strip it from the end of the line
    [[nodiscard]] inline string_view strip_cr( string_view line )
    {
        if( line.size() > 0 && line.back() == '\r' )
        {
            return string_view( line.data(), line.size() - 1 );
        }
        return line;
    }


    [[nodiscard]] inline size_t count_lines( string_view input )
    {
        if( input.empty() ) return 0;

        size_t count
            = std::count( input.data(), input.data() + input.size(), '\n' );

        // If the last line ends in a newline, the number of lines in identical
        // to the number of newlines
        if( input.back() == '\n' ) { return count; }
        // Otherwise, there's one extra line
        return count + 1;
    }


    struct line_reader
    {
        string_view input;

        line_reader() = default;

        constexpr line_reader( string_view input ) noexcept
        : input( input )
        {
        }

        constexpr opt_sv next() noexcept
        {
            auto p = input.find( '\n' );
            if( p != string_view::npos )
            {
                string_view line = strip_cr( string_view( input.data(), p ) );
                // Start of next line
                size_t next = p + 1;
                input = string_view( input.data() + next, input.size() - next );

                return line;
            }
            if( !input.empty() )
            {
                // If there's data, return the final line.
                // The carriage return is only stripped if the ending is '\r\n'.
                // So, if there's no '\n', no stripping occurs.
                auto line = input;
                input     = string_view();
                return line;
            }

            return string_view();
        }
    };

    /// Get a vector of all the lines in the input. Do not include line endings.
    inline std::vector<string_view> lines( string_view input )
    {
        std::vector<string_view> result( count_lines( input ) );
        line_reader              r( input );
        for( auto& elem : result ) { elem = r.next(); }
        return result;
    }

    // # Rule	NAME	FROM	TO	-	IN	ON	AT	SAVE
    // LETTER Rule	CA	1948	only	-	Mar	14	2:01
    // 1:00	D Rule	CA	1949	only	-	Jan	 1	2:00
    // 0	S Rule	CA	1950	1966	-	Apr	lastSun	1:00
    // 1:00	D
    struct RawRule
    {
        string_view name;
        string_view from;
        string_view to;
        string_view in;
        string_view on;
        string_view at;
        string_view save;
    };
} // namespace vtz
