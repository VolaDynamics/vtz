#pragma once

#include <cstdint>
#include <string_view>

#include <vtz/vtzdef.h>

#include <vtz/inplace_optional.h>
namespace vtz {
    using std::string_view;


    // clang-format off
    /// Enum representing the Month. Values start counting up from 1.
    ///
    /// `Jan=1` was chosen because using '1' for the first month is convention,
    /// and converting to the numeric month representation is as simple
    /// as doing a cast.
    enum class Mon : u8 { Jan = 1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

    /// ENum representing the day of the week. Values start counting up from 0.
    ///
    /// `Sun=0` (and dates start from 0) because being able to compute DOW
    /// by taking the number of days
    enum class DOW : u8 { Sun, Mon, Tue, Wed, Thu, Fri, Sat };
    // clang-format on

    /// Weekday Difference. Return the number of days to get from the lhs to the
    /// rhs weekday.
    ///
    /// Eg: Mon - Sun == 1, Sun - Mon == 6
    constexpr usize operator-( DOW lhs, DOW rhs ) noexcept {
        usize l = usize( lhs );
        usize r = usize( rhs );
        return ( l - r ) + ( l < r ? 7 : 0 );
    }

    using OptMon = OptV<Mon, Mon{}>;
    using OptDOW = OptV<DOW, DOW{ 7 }>;

} // namespace vtz


namespace vtz::_impl {
    // clang-format off
    constexpr inline char MONTH_NAMES[12][4]{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    constexpr inline char DOW_NAMES[7][4]{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

    /// Load 1 byte from the input and return the result as a u32
    constexpr u32 _load1( char const* src ) noexcept { return u8( src[0] ); }
    /// Load 2 bytes from the input and return the result as a u32.
    constexpr u32 _load2( char const* src ) noexcept { return u8( src[0] ) | ( u8( src[1] ) << 8 ); }
    /// Load 3 bytes from the input and return the result as a u32.
    constexpr u32 _load3( char const* src ) noexcept { return u8( src[0] ) | ( u8( src[1] ) << 8 ) | ( u8( src[2] ) << 16 ); }
    /// Load 4 bytes from the input and return the result as a u32.
    constexpr u32 _load4( char const* src ) noexcept { return u8( src[0] ) | ( u8( src[1] ) << 8 ) | ( u8( src[2] ) << 16 ) | ( u8( src[3] ) << 24 ); }
    // clang-format on


    constexpr OptMon _parseMon( char const* src, size_t len ) noexcept {
        if( len == 3 )
        {
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
        }
        else if( len == 2 )
        {
            switch( _load2( src ) )
            {
            case _load2( "Ja" ): return { Mon::Jan };
            case _load2( "Fe" ): return { Mon::Feb };
            // case _load2( "Ma" ): return { Mon::Mar };
            case _load2( "Ap" ): return { Mon::Apr };
            // case _load2( "Ma" ): return { Mon::May };
            // case _load2( "Ju" ): return { Mon::Jun };
            // case _load2( "Ju" ): return { Mon::Jul };
            case _load2( "Au" ): return { Mon::Aug };
            case _load2( "Se" ): return { Mon::Sep };
            case _load2( "Oc" ): return { Mon::Oct };
            case _load2( "No" ): return { Mon::Nov };
            case _load2( "De" ): return { Mon::Dec };
            }
        }
        else if( len == 1 )
        {
            switch( src[0] )
            {
            // case 'J': return { Mon::Jan };
            case 'F': return { Mon::Feb };
            // case 'M': return { Mon::Mar };
            // case 'A': return { Mon::Apr };
            // case 'M': return { Mon::May };
            // case 'J': return { Mon::Jun };
            // case 'J': return { Mon::Jul };
            // case 'A': return { Mon::Aug };
            case 'S': return { Mon::Sep };
            case 'O': return { Mon::Oct };
            case 'N': return { Mon::Nov };
            case 'D': return { Mon::Dec };
            }
        }

        return none;
    }

    constexpr OptDOW _parseDOW( char const* src, size_t len ) noexcept {
        if( len == 3 )
        {
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
        }
        else if( len == 2 )
        {
            switch( _load2( src ) )
            {
            case _load2( "Su" ): return { DOW::Sun };
            case _load2( "Mo" ): return { DOW::Mon };
            case _load2( "Tu" ): return { DOW::Tue };
            case _load2( "We" ): return { DOW::Wed };
            case _load2( "Th" ): return { DOW::Thu };
            case _load2( "Fr" ): return { DOW::Fri };
            case _load2( "Sa" ): return { DOW::Sat };
            }
        }
        else if( len == 1 )
        {
            switch( _load1( src ) )
            {
            // case 'S': return { DOW::Sun };
            case 'M': return { DOW::Mon };
            // case 'T': return { DOW::Tue };
            case 'W': return { DOW::Wed };
            // case 'T': return { DOW::Thu };
            case 'F': return { DOW::Fri };
            // case 'S': return { DOW::Sat };
            }
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
