#pragma once

#include <vtz/tz_reader/RuleSave.h>
#include <vtz/bit.h>

#include <string>
#include <string_view>

namespace vtz {
    i32 parseHHMMSSOffset( char const* p, size_t size, int sign = 1 ) noexcept;
    i32 parseSignedHHMMSSOffset( char const* p, size_t size ) noexcept;

    inline i32 parseHHMMSSOffset( string_view sv ) noexcept {
        return parseHHMMSSOffset( sv.data(), sv.size(), 1 );
    }
    inline i32 parseSignedHHMMSSOffset( string_view sv ) noexcept {
        return parseSignedHHMMSSOffset( sv.data(), sv.size() );
    }


    /// Consider an offset like `UTC-5:00`. This offset is _from_ UTC,
    /// so to get to Local time, you take the UTC time, and add `-5:00`
    /// (This is `utc + off`)
    ///
    /// Similarly, to get to UTC time, from local time, you _subtract_ `-5:00`
    /// (This is `utc - off`)

    struct FromUTC {
        i32 off{};
        FromUTC() = default;

        /// Add offset to get from utc time to local time
        constexpr i64 toLocal( i64 utc ) const noexcept { return utc + off; }

        /// Subtract offset to get from local time to UTC time
        constexpr i64 toUTC( i64 local ) const noexcept { return local - off; }

        /// Consider America/New_York. When we 'save' 1 hour, our offset from
        /// UTC goes from `-5:00` to `-4:00` - the hour is _added_ to the offset
        constexpr FromUTC save( RuleSave save ) const noexcept {
            return { off + save.save };
        }

        /// Consider America/New_York. When we 'save' 1 hour, our offset from
        /// UTC
        /// goes from `-5:00` to `-4:00` - the hour is _added_ to the offset
        constexpr FromUTC save( i32 saveSeconds ) const noexcept {
            return { off + saveSeconds };
        }

        constexpr FromUTC( i32 off ) noexcept
        : off( off ) {}

        explicit FromUTC( string_view sv )
        : off( parseSignedHHMMSSOffset( sv ) ) {}

        template<size_t N>
        FromUTC( char const ( &arr )[N] )
        : FromUTC( string_view( arr ) ) {}


        constexpr bool operator==( FromUTC const& rhs ) const noexcept {
            return off == rhs.off;
        }
    };
    string format_as( FromUTC off );
} // namespace vtz
