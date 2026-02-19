#pragma once

#include "vtz/date_types.h"
#include <vtz/impl/bit.h>
#include <vtz/civil.h>
#include <vtz/strings.h>
#include <vtz/tz_reader/FromUTC.h>
#include <vtz/impl/zone_abbr.h>

namespace vtz {


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

        constexpr void set_fmt( Tag tag, size_t sz0, size_t sz1 ) noexcept {
            fmt_ = u16( ( sz0 << 2 ) | ( sz1 << 6 ) | tag );
        }

        [[nodiscard]] constexpr ZoneFormat with(
            Tag tag, size_t sz0 = 0, size_t sz1 = 0 ) const noexcept {
            auto result = *this;
            result.set_fmt( tag, sz0, sz1 );
            return result;
        }

        /// Format the abbreviation using the given letter (for PERCENT_S) or
        /// selecting standard/DST (for SLASH based on whether DST is active)
        /// For PERCENT_Z, pass the offset in seconds via the offset parameter
        template<size_t N>
        constexpr size_t write_n( char* dest,
            i32                         off,
            bool                        is_dst,
            char const*                 letter,
            size_t                      letter_size ) const noexcept {
            size_t sz0 = ( fmt_ >> 2 ) & 0xf;
            size_t sz1 = ( fmt_ >> 6 ) & 0xf;

            /// Buffer to place offset (if the format contained a '%z')
            char z_buff[16]{};

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
                    size_t s = is_dst ? sz1 : sz0;
                    auto*  p = buff + ( is_dst ? sz0 : 0 );
                    // Truncate if necessary
                    if( s > N ) s = N;
                    // Perform copy
                    for( size_t i = 0; i < s; ++i ) dest[i] = p[i];
                    return s;
                }
            case FMT_Z:
                // Ignore the letter, use shortest offset instead
                letter      = z_buff;
                letter_size = write_shortest_offset( off, z_buff );

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
                    if( letter_size > rem ) letter_size = rem;
                    for( size_t i = 0; i < letter_size; ++i )
                        dest[i] = letter[i];
                    dest += letter_size;
                    rem  -= letter_size;

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

            VTZ_UNREACHABLE();
        }

        template<size_t N>
        constexpr size_t write_n( char* dest,
            i32                         off,
            bool                        is_dst,
            string_view                 letter ) const noexcept {
            return write_n<N>(
                dest, off, is_dst, letter.data(), letter.size() );
        }

        template<size_t N>
        constexpr void format( fix_str<N>& dest,
            i32                           off,
            bool                          is_dst,
            string_view                   letter ) const noexcept {
            dest.size_ = u8( write_n<N>( dest.buff_, off, is_dst, letter ) );
        }

        constexpr zone_abbr format(
            i32 off, bool is_dst, string_view letter ) const noexcept {
            zone_abbr result{};
            format( result, off, is_dst, letter );
            return result;
        }

        constexpr zone_abbr format(
            FromUTC off, bool is_dst, string_view letter ) const noexcept {
            return format( off.off, is_dst, letter );
        }

        bool operator==( ZoneFormat const& rhs ) const noexcept {
            return _b16( *this ) == _b16( rhs );
        }
        bool operator!=( ZoneFormat const& rhs ) const noexcept {
            return _b16( *this ) != _b16( rhs );
        }
    };
} // namespace vtz
