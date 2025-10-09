#pragma once

#include <vtz/inplace_optional.h>
#include <cstdint>
#include <string_view>
namespace vtz {
    using std::string_view;
    using u8  = uint8_t;
    using u16 = uint16_t;
    using u32 = uint32_t;
    using u64 = uint64_t;

    using i8  = int8_t;
    using i16 = int16_t;
    using i32 = int32_t;

    // clang-format off
    /// Enum representing the Month. Values start counting up from 1.
    ///
    /// `Jan=1` was chosen because using '1' for the first month is convention,
    /// and converting to the numeric month representation is as simple
    /// as doing a cast.
    enum class Mon : u16 { Jan = 1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

    /// ENum representing the day of the week. Values start counting up from 0.
    ///
    /// `Sun=0` (and dates start from 0) because being able to compute DOW
    /// by taking the number of days
    enum class DOW : u8 { Sun, Mon, Tue, Wed, Thu, Fri, Sat };
    // clang-format on

    using OptMon = OptV<Mon, Mon{}>;
    using OptDOW = OptV<DOW, DOW{ 7 }>;

} // namespace vtz


namespace vtz::_impl {
    // clang-format off
    constexpr inline char MONTH_NAMES[12][4]{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    constexpr inline char DOW_NAMES[7][4]{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

    /// Load 2 bytes from the input and return the result as a u32.
    constexpr u32 _load2( char const* src ) noexcept { return u8( src[0] ) | ( u8( src[1] ) << 8 ); }
    /// Load 3 bytes from the input and return the result as a u32.
    constexpr u32 _load3( char const* src ) noexcept { return u8( src[0] ) | ( u8( src[1] ) << 8 ) | ( u8( src[2] ) << 16 ); }
    /// Load 4 bytes from the input and return the result as a u32.
    constexpr u32 _load4( char const* src ) noexcept { return u8( src[0] ) | ( u8( src[1] ) << 8 ) | ( u8( src[2] ) << 16 ) | ( u8( src[3] ) << 24 ); }
    // clang-format on

    /// Load 3 bytes into a u32
    constexpr OptMon _parseMon( char const* src ) noexcept {
        switch( _load3( src ) )
        {
        case _load3( "Jan" ): return { Mon::Jan };
        case _load3( "Feb" ): return { Mon::Feb };
        case _load3( "Mar" ): return { Mon::Mar };
        case _load3( "Apr" ): return { Mon::Apr };
        case _load3( "May" ): return { Mon::May };
        case _load3( "Jun" ): return { Mon::Jun };
        case _load3( "Jul" ): return { Mon::Jul };
        case _load3( "Aug" ): return { Mon::Aug };
        case _load3( "Sep" ): return { Mon::Sep };
        case _load3( "Oct" ): return { Mon::Oct };
        case _load3( "Nov" ): return { Mon::Nov };
        case _load3( "Dec" ): return { Mon::Dec };
        }

        return none;
    }

    constexpr OptDOW _parseDOW( char const* src ) noexcept {
        switch( _load3( src ) )
        {
        case _load3( "Sun" ): return { DOW::Sun };
        case _load3( "Mon" ): return { DOW::Mon };
        case _load3( "Tue" ): return { DOW::Tue };
        case _load3( "Wed" ): return { DOW::Wed };
        case _load3( "Thu" ): return { DOW::Thu };
        case _load3( "Fri" ): return { DOW::Fri };
        case _load3( "Sat" ): return { DOW::Sat };
        }

        return none;
    }
} // namespace vtz::_impl


namespace vtz {
    constexpr string_view format_as( Mon m ) {
        size_t i = size_t( m ) - 1;
        return i < 12 ? string_view( _impl::MONTH_NAMES[i], 3 )
                      : string_view( "Mon(<unknown>)" );
    }


    constexpr string_view format_as( DOW d ) {
        return u32( d ) < 7 ? string_view( _impl::DOW_NAMES[u32( d )], 3 )
                            : string_view( "DOW(<unknown>)" );
    }
} // namespace vtz
