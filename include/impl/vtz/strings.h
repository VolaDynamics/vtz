#pragma once

#include <cstring>
#include <vtz/impl/bit.h>
#include <vtz/impl/fix_str.h>

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vtz {
    using std::string;
    using std::string_view;
    using std::vector;

    [[nodiscard]] size_t count_lines( string_view input );

    /// Get a vector of all the lines in the input. Do not include line endings.
    vector<string_view> lines( string_view input );

    /// Tokenize a line. Return a vector of tokens
    vector<string_view> tokenize( string_view line );


    /// (linenumber, column) pair
    struct Location {
        size_t line{};
        size_t col{};

        Location() = default;

        constexpr Location( size_t line, size_t col ) noexcept
        : line( line )
        , col( col ) {}

        bool     has_value() const noexcept { return line != 0; }
        explicit operator bool() const noexcept { return line != 0; }

        bool operator==( Location rhs ) const noexcept {
            return line == rhs.line && col == rhs.col;
        }

        std::string str() const;

        /// Find the line and column number where a given cursor appears within
        /// some body of text. Return an empty Loc if the input is outside the
        /// body.
        static Location where_ptr(
            string_view body, char const* cursor ) noexcept;

        /// Find the line and column number where a given cursor appears within
        /// some body of text. Return an empty Loc if the input is outside the
        /// body.
        static Location where( string_view body, size_t cursor ) noexcept;

        /// Find the line and column number where the substring ends.
        /// Always returns a valid location.
        static Location where( string_view substr ) noexcept;
    };


    /// Optional-like wrapper around a string_view. This optional is empty if
    /// data() == nullptr
    ///
    /// `has_value()` implies `data() != nullptr`. This makes it appropriate for
    /// returning from a LineIterator, since non-empty lines are still valid.
    struct OptSV : string_view {
        using string_view::string_view;

        // Allow construction from base
        constexpr OptSV( string_view rhs ) noexcept
        : string_view( rhs ) {}

        constexpr OptSV( std::nullopt_t ) noexcept
        : string_view() {}

        constexpr string_view const& operator*() const noexcept {
            return *this;
        }
        constexpr string_view value() const noexcept { return *this; }

        constexpr bool has_value() const noexcept { return data() != nullptr; }

        constexpr string_view value_or( string_view rhs ) const noexcept {
            return has_value() ? string_view( *this ) : rhs;
        }

        constexpr explicit operator bool() const noexcept {
            return has_value();
        }
    };

    /// Optional-like interface representing a token. Unlike OptSV, has_value()
    /// returns true if the token is not empty.
    ///
    /// An empty token may still, however, have a valid data() pointer. This is
    /// used to simplify error handling:
    struct OptTok : string_view {
        OptTok() = default;

        constexpr OptTok( char const* cstr )
        : string_view( cstr ) {}
        constexpr OptTok( char const* p, size_t len ) noexcept
        : string_view( p, len ) {}
        constexpr OptTok( string_view rhs ) noexcept
        : string_view( rhs ) {}

        constexpr string_view value() const noexcept { return *this; }

        constexpr bool has_value() const noexcept { return size() > 0; }


        constexpr string_view value_or( string_view rhs ) const noexcept {
            return has_value() ? string_view( *this ) : rhs;
        }

        constexpr explicit operator bool() const noexcept {
            return has_value();
        }

        /// Used to indicate that we expected a token at the given position, but
        /// that one was missing.
        ///
        /// `data()` must still be valid, so that we can obtain a proper
        /// lineno/column when providing an error message to the user.
        constexpr static OptTok missing_at( char const* at ) noexcept {
            return OptTok( at, size_t( 0 ) );
        }
    };

    constexpr bool is_delim( char ch ) { return ch <= (unsigned char)( ' ' ); }

    struct TokenIter {
      private:

        char const* b_ = nullptr;
        char const* e_ = nullptr;

        constexpr static char const* find_delim(
            char const* p, char const* end ) noexcept {
            for( ;; )
            {
                if( p == end || is_delim( *p ) ) return p;
                ++p;
                if( p == end || is_delim( *p ) ) return p;
                ++p;
                if( p == end || is_delim( *p ) ) return p;
                ++p;
                if( p == end || is_delim( *p ) ) return p;
                ++p;
                if( p == end || is_delim( *p ) ) return p;
                ++p;
                if( p == end || is_delim( *p ) ) return p;
                ++p;
                if( p == end || is_delim( *p ) ) return p;
                ++p;
                if( p == end || is_delim( *p ) ) return p;
                ++p;
            }
        }

        constexpr static char const* find_non_delim(
            char const* p, char const* end ) noexcept {
            for( ;; )
            {
                if( p == end || !is_delim( *p ) ) return p;
                ++p;
                if( p == end || !is_delim( *p ) ) return p;
                ++p;
                if( p == end || !is_delim( *p ) ) return p;
                ++p;
                if( p == end || !is_delim( *p ) ) return p;
                ++p;
            }
        }

      public:

        constexpr TokenIter() = default;

        constexpr TokenIter( string_view line ) noexcept
        : b_( line.data() )
        , e_( line.data() + line.size() ) {}

        string_view rest() const noexcept {
            return string_view( b_, size_t( e_ - b_ ) );
        }

        void clear() noexcept { b_ = e_; }

        OptTok next() {
            auto start = find_non_delim( b_, e_ );

            if( start == e_ )
            {
                clear();
                return OptTok::missing_at( e_ );
            }

            auto end = find_delim( start + 1, e_ );

            auto result = string_view( start, size_t( end - start ) );

            // next time around, we will start searching from 1 past the last
            // character we checked
            if( end < e_ ) { b_ = end + 1; }
            else { clear(); }

            return result;
        }

        OptTok next_non_comment() {
            auto start = find_non_delim( b_, e_ );

            if( start == e_ )
            {
                clear();
                return OptTok::missing_at( e_ );
            }
            if( *start == '#' ) return OptTok::missing_at( start );

            auto end = find_delim( start + 1, e_ );

            auto result = string_view( start, size_t( end - start ) );

            // next time around, we will start searching from 1 past the last
            // character we checked
            if( end < e_ ) { b_ = end + 1; }
            else { clear(); }

            return result;
        }

        TokenIter& drop() {
            (void)next();
            return *this;
        }
    };

    inline string_view strip_comment( string_view line ) {
        auto pos = line.find( '#' );
        if( pos == string_view::npos ) { return line; }
        return string_view( line.data(), pos );
    }

    [[nodiscard]] inline string_view strip_trailing_delim(
        string_view s ) noexcept {
        if( s.empty() ) { return s; }
        size_t i = s.size();
        while( i > 0 && is_delim( s[i - 1] ) ) { --i; }
        return string_view( s.data(), i );
    }

    [[nodiscard]] inline string_view strip_leading_delim(
        string_view s ) noexcept {
        size_t i = 0;
        while( i < s.size() && is_delim( s[i] ) ) ++i;
        return string_view( s.data() + i, s.size() - i );
    }


    // If a line ends in a '\r' character, strip it from the end of the line
    [[nodiscard]] inline string_view strip_cr( string_view line ) {
        if( line.size() > 0 && line.back() == '\r' )
        { return string_view( line.data(), line.size() - 1 ); }
        return line;
    }


    struct LineIter {
      private:

        string_view input;

      public:

        LineIter() = default;

        constexpr LineIter( string_view input ) noexcept
        : input( input ) {}

        constexpr OptSV next() noexcept {
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
                input     = string_view( input.data() + input.size(), 0 );
                return line;
            }

            return string_view();
        }

        string_view rest() const noexcept { return input; }
    };

    inline char first_or_nil( string_view sv ) {
        if( sv.empty() ) return '\0';
        return sv[0];
    }

    std::string escape_string( std::string_view sv );

    /// std::string_view::starts_with doesn't appear until C++20
    inline bool starts_with( string_view s, string_view prefix ) {
        return s.size() >= prefix.size()
               && string_view( s.data(), prefix.size() ) == prefix;
    }
} // namespace vtz
