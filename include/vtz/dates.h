#pragma once

#include <cstdint>
#include <string_view>
namespace vtz {
    using std::string_view;
    using u8  = uint8_t;
    using u32 = uint32_t;

    // clang-format off
    enum class Mon : u32 { Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };
    enum class DOW : u32 { Sun, Mon, Tue, Wed, Thu, Fri, Sat };
    // clang-format on
} // namespace vtz


namespace vtz::_impl {
    // clang-format off
    constexpr inline char MONTH_NAMES[12][4]{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    constexpr inline char DOW_NAMES[7][4]{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

    /// Load 3 bytes from the input and return the result as a u32.
    constexpr u32 _load3( char const* src ) noexcept { return u8( src[0] ) | ( u8( src[1] ) << 8 ) | ( u8( src[2] ) << 16 ); }
    // clang-format on

    /// Load 3 bytes into a u32
    constexpr Mon _parseMon( char const* src ) noexcept {
        enum : u32 {
            Jan = _load3( "Jan" ),
            Feb = _load3( "Feb" ),
            Mar = _load3( "Mar" ),
            Apr = _load3( "Apr" ),
            May = _load3( "May" ),
            Jun = _load3( "Jun" ),
            Jul = _load3( "Jul" ),
            Aug = _load3( "Aug" ),
            Sep = _load3( "Sep" ),
            Oct = _load3( "Oct" ),
            Nov = _load3( "Nov" ),
            Dec = _load3( "Dec" ),
        };

        switch( _load3( src ) )
        {
        case Jan: return Mon::Jan;
        case Feb: return Mon::Feb;
        case Mar: return Mon::Mar;
        case Apr: return Mon::Apr;
        case May: return Mon::May;
        case Jun: return Mon::Jun;
        case Jul: return Mon::Jul;
        case Aug: return Mon::Aug;
        case Sep: return Mon::Sep;
        case Oct: return Mon::Oct;
        case Nov: return Mon::Nov;
        case Dec: return Mon::Dec;
        }

        return Mon( ~0u );
    }

    constexpr DOW _parseDOW( char const* src ) noexcept {
        enum : u32 {
            Sun = _load3( "Sun" ),
            Mon = _load3( "Mon" ),
            Tue = _load3( "Tue" ),
            Wed = _load3( "Wed" ),
            Thu = _load3( "Thu" ),
            Fri = _load3( "Fri" ),
            Sat = _load3( "Sat" ),
        };

        switch( _load3( src ) )
        {
        case Sun: return DOW::Sun;
        case Mon: return DOW::Mon;
        case Tue: return DOW::Tue;
        case Wed: return DOW::Wed;
        case Thu: return DOW::Thu;
        case Fri: return DOW::Fri;
        case Sat: return DOW::Sat;
        }

        return DOW( ~0u );
    }
} // namespace vtz::_impl


namespace vtz {
    constexpr string_view format_as( Mon m ) {
        return u32( m ) < 12 ? string_view( _impl::MONTH_NAMES[u32( m )], 3 )
                             : string_view( "Mon(<unknown>)" );
    }


    constexpr string_view format_as( DOW d ) {
        return u32( d ) < 7 ? string_view( _impl::DOW_NAMES[u32( d )], 3 )
                            : string_view( "DOW(<unknown>)" );
    }
} // namespace vtz
