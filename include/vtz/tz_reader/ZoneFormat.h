#pragma once

#include <vtz/bit.h>
#include <vtz/civil.h>
#include <vtz/strings.h>
#include <vtz/tz_reader/FromUTC.h>

namespace vtz {
    struct ZoneAbbr : FixStr<15> {
        bool operator==( ZoneAbbr const& rhs ) const noexcept {
            return B16( *this ) == B16( rhs );
        }
        bool operator!=( ZoneAbbr const& rhs ) const noexcept {
            return B16( *this ) != B16( rhs );
        }
    };

    /// ZoneFormat holds timezone format strings with efficient representation
    /// Supports four format types:
    /// - LITERAL: Simple string (e.g., "GMT", "PST", "-00")
    /// - PERCENT_S: Contains %s substitution (e.g., "E%sT" -> "EST" or "EDT")
    /// - SLASH: Two alternatives separated by / (e.g., "GMT/BST")
    /// - PERCENT_Z: Use numeric offset (e.g., "%z" -> "-03" or "-04")
    struct alignas( 8 ) ZoneFormat {
        enum Tag {
            LITERAL = 0, // Interpret string literally
            SLASH   = 1, // Slash-separated alternatives
            FMT_S   = 2, // Use %s substitution
            FMT_Z   = 3, // Use numeric offset format
        };

        // Buffer for format string (max 11 chars + room for growth)
        char buff[14];
        /// bits 0-2: Tag
        /// bits 2-6: size before split (or size if literal)
        /// bits 6-10: size after split (or 0 if literal)
        u16 fmt_;

        ZoneFormat() = default;

        constexpr Tag tag() const noexcept { return Tag( fmt_ & 0x3 ); }

        constexpr void setFmt( Tag tag, size_t sz0, size_t sz1 ) noexcept {
            fmt_ = u16( ( sz0 << 2 ) | ( sz1 << 6 ) | tag );
        }

        [[nodiscard]] constexpr ZoneFormat with(
            Tag tag, size_t sz0 = 0, size_t sz1 = 0 ) const noexcept {
            auto result = *this;
            result.setFmt( tag, sz0, sz1 );
            return result;
        }

        /// Format the abbreviation using the given letter (for PERCENT_S) or
        /// selecting standard/DST (for SLASH based on whether DST is active)
        /// For PERCENT_Z, pass the offset in seconds via the offset parameter
        template<size_t N>
        constexpr size_t writeN( char* dest,
            i32                        off,
            bool                       isDST,
            char const*                letter,
            size_t                     letterSize ) const noexcept {
            size_t sz0 = ( fmt_ >> 2 ) & 0xf;
            size_t sz1 = ( fmt_ >> 6 ) & 0xf;

            /// Buffer to place offset (if the format contained a '%z')
            char zBuff[16]{};

            switch( tag() )
            {
            case LITERAL:
                {
                    // Truncate if necessary
                    if( sz0 > N ) sz0 = N;
                    // Perform copy
                    for( size_t i = 0; i < sz0; ++i ) dest[i] = buff[i];
                    return sz0;
                }
            case SLASH:
                {
                    // Select which half to copy
                    size_t s = isDST ? sz1 : sz0;
                    auto*  p = buff + ( isDST ? sz0 : 0 );
                    // Truncate if necessary
                    if( s > N ) s = N;
                    // Perform copy
                    for( size_t i = 0; i < s; ++i ) dest[i] = p[i];
                    return s;
                }
            case FMT_Z:
                // Ignore the letter, use shortest offset instead
                letter     = zBuff;
                letterSize = writeShortestOffset( off, zBuff );

                [[fallthrough]];
            case FMT_S:
                {
                    // Get parts
                    char const* src0 = buff;
                    char const* src1 = buff + sz0;
                    size_t      rem  = N; // Remaining space in dest buffer

                    // Truncate if necessary
                    if( sz0 > rem ) sz0 = rem;
                    // Perform copy of first part
                    for( size_t i = 0; i < sz0; ++i ) dest[i] = src0[i];
                    dest += sz0;
                    rem  -= sz0;

                    // Truncate if necessary
                    if( letterSize > rem ) letterSize = rem;
                    for( size_t i = 0; i < letterSize; ++i )
                        dest[i] = letter[i];
                    dest += letterSize;
                    rem  -= letterSize;

                    // Truncate if necessary
                    if( sz1 > rem ) sz1 = rem;
                    for( size_t i = 0; i < sz1; ++i ) dest[i] = src1[i];
                    dest += sz1;
                    rem  -= sz1;

                    // Number of bytes written is N - (remaining bytes left to
                    // write)
                    return N - rem;
                }
            }
        }

        template<size_t N>
        constexpr size_t writeN( char* dest,
            i32                        off,
            bool                       isDST,
            string_view                letter ) const noexcept {
            return writeN<N>( dest, off, isDST, letter.data(), letter.size() );
        }

        template<size_t N>
        constexpr void format( FixStr<N>& dest,
            i32                           off,
            bool                          isDST,
            string_view                   letter ) const noexcept {
            dest.size_ = writeN<N>( dest.buff_, off, isDST, letter );
        }

        constexpr ZoneAbbr format(
            i32 off, bool isDST, string_view letter ) const noexcept {
            ZoneAbbr result{};
            format( result, off, isDST, letter );
            return result;
        }

        constexpr ZoneAbbr format(
            FromUTC off, bool isDST, string_view letter ) const noexcept {
            return format( off.off, isDST, letter );
        }

        bool operator==( ZoneFormat const& rhs ) const noexcept {
            return B16( *this ) == B16( rhs );
        }
        bool operator!=( ZoneFormat const& rhs ) const noexcept {
            return B16( *this ) != B16( rhs );
        }

        string str() const;
    };
    inline string format_as( ZoneFormat const& f ) { return f.str(); }
} // namespace vtz
