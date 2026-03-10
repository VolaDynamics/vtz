#pragma once

#include <cstdint>
#include <string_view>

#include <vtz/impl/bit.h>

#include <vtz/impl/bit.h>
#include <vtz/inplace_optional.h>


namespace vtz {
    using std::string_view;


    // clang-format off
    /// Enum representing the Month. Values start counting up from 1.
    ///
    /// `Jan=1` was chosen because using '1' for the first month is convention,
    /// and converting to the numeric month representation is as simple
    /// as doing a cast.
    enum class month_t : u8 { Jan = 1, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec };

    /// ENum representing the day of the week. Values start counting up from 0.
    ///
    /// `Sun=0` (and dates start from 0) because being able to compute dow_t
    /// by taking the number of days
    enum class dow_t : u8 { Sun, Mon, Tue, Wed, Thu, Fri, Sat };
    // clang-format on

    /// Weekday Difference. Return the number of days to get from the lhs to the
    /// rhs weekday.
    ///
    /// Eg: Mon - Sun == 1, Sun - Mon == 6
    constexpr size_t operator-( dow_t lhs, dow_t rhs ) noexcept {
        size_t l = size_t( lhs );
        size_t r = size_t( rhs );
        return ( l - r ) + ( l < r ? 7 : 0 );
    }

    using opt_mon = opt_val<month_t, month_t{}>;
    using opt_dow = opt_val<dow_t, dow_t{ 7 }>;

} // namespace vtz


namespace vtz::_impl {
    // clang-format off
    constexpr inline char MONTH_NAMES[12][4]{ "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    constexpr inline char DOW_NAMES[7][4]{ "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };




    constexpr opt_mon _parse_mon( char const* src, size_t len ) noexcept {
        if( len == 3 )
        {
            switch( _load3( src ) )
            {
            case _load3( "Jan" ): return { month_t::Jan };
            case _load3( "Feb" ): return { month_t::Feb };
            case _load3( "Mar" ): return { month_t::Mar };
            case _load3( "Apr" ): return { month_t::Apr };
            case _load3( "May" ): return { month_t::May };
            case _load3( "Jun" ): return { month_t::Jun };
            case _load3( "Jul" ): return { month_t::Jul };
            case _load3( "Aug" ): return { month_t::Aug };
            case _load3( "Sep" ): return { month_t::Sep };
            case _load3( "Oct" ): return { month_t::Oct };
            case _load3( "Nov" ): return { month_t::Nov };
            case _load3( "Dec" ): return { month_t::Dec };
            }
        }
        else if( len == 2 )
        {
            switch( _load2( src ) )
            {
            case _load2( "Ja" ): return { month_t::Jan };
            case _load2( "Fe" ): return { month_t::Feb };
            // case _load2( "Ma" ): return { month_t::Mar };
            case _load2( "Ap" ): return { month_t::Apr };
            // case _load2( "Ma" ): return { month_t::May };
            // case _load2( "Ju" ): return { month_t::Jun };
            // case _load2( "Ju" ): return { month_t::Jul };
            case _load2( "Au" ): return { month_t::Aug };
            case _load2( "Se" ): return { month_t::Sep };
            case _load2( "Oc" ): return { month_t::Oct };
            case _load2( "No" ): return { month_t::Nov };
            case _load2( "De" ): return { month_t::Dec };
            }
        }
        else if( len == 1 )
        {
            switch( src[0] )
            {
            // case 'J': return { month_t::Jan };
            case 'F': return { month_t::Feb };
            // case 'M': return { month_t::Mar };
            // case 'A': return { month_t::Apr };
            // case 'M': return { month_t::May };
            // case 'J': return { month_t::Jun };
            // case 'J': return { month_t::Jul };
            // case 'A': return { month_t::Aug };
            case 'S': return { month_t::Sep };
            case 'O': return { month_t::Oct };
            case 'N': return { month_t::Nov };
            case 'D': return { month_t::Dec };
            }
        }

        return none;
    }

    constexpr opt_dow _parse_dow( char const* src, size_t len ) noexcept {
        if( len == 3 )
        {
            switch( _load3( src ) )
            {
            case _load3( "Sun" ): return { dow_t::Sun };
            case _load3( "Mon" ): return { dow_t::Mon };
            case _load3( "Tue" ): return { dow_t::Tue };
            case _load3( "Wed" ): return { dow_t::Wed };
            case _load3( "Thu" ): return { dow_t::Thu };
            case _load3( "Fri" ): return { dow_t::Fri };
            case _load3( "Sat" ): return { dow_t::Sat };
            }
        }
        else if( len == 2 )
        {
            switch( _load2( src ) )
            {
            case _load2( "Su" ): return { dow_t::Sun };
            case _load2( "Mo" ): return { dow_t::Mon };
            case _load2( "Tu" ): return { dow_t::Tue };
            case _load2( "We" ): return { dow_t::Wed };
            case _load2( "Th" ): return { dow_t::Thu };
            case _load2( "Fr" ): return { dow_t::Fri };
            case _load2( "Sa" ): return { dow_t::Sat };
            }
        }
        else if( len == 1 )
        {
            switch( _load1( src ) )
            {
            // case 'S': return { dow_t::Sun };
            case 'M': return { dow_t::Mon };
            // case 'T': return { dow_t::Tue };
            case 'W': return { dow_t::Wed };
            // case 'T': return { dow_t::Thu };
            case 'F': return { dow_t::Fri };
            // case 'S': return { dow_t::Sat };
            }
        }

        return none;
    }
} // namespace vtz::_impl


namespace vtz {
    constexpr string_view format_as( month_t m ) {
        size_t i = size_t( m ) - 1;
        return i < 12 ? string_view( _impl::MONTH_NAMES[i], 3 )
                      : string_view( "month_t(<unknown>)" );
    }


    constexpr string_view format_as( dow_t d ) {
        return u32( d ) < 7 ? string_view( _impl::DOW_NAMES[u32( d )], 3 )
                            : string_view( "dow_t(<unknown>)" );
    }
} // namespace vtz
