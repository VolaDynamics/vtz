#pragma once

#include <vtz/tz_reader/RuleSave.h>
#include <vtz/bit.h>
#include <vtz/strings.h>

namespace vtz {
    /// From "How to Read the tz Database Source Files":
    ///
    /// The RULES column tells us whether daylight saving time is being
    /// observed:
    /// - A hyphen, a kind of null value, means that we have not set our clocks
    ///   ahead of standard time.
    /// - An amount of time (usually but not necessarily “1:00” meaning one
    ///   hour) means that we have set our clocks ahead by that amount.
    /// - Some alphabetic string means that we might have set our clocks ahead;
    ///   and we need to check the rule the name of which is the given
    ///   alphabetic string.

    struct alignas( u64 ) ZoneRule {
        struct hyphen_t {};
        constexpr static hyphen_t hyphen;

        enum Kind {
            NAMED,  ///< Zone Rule is a named rule (eg, 'US')
            OFFSET, ///< We have set our clocks ahead of STDOFF by the given
                    ///< amount
            HYPHEN, ///< 'RULES' is '-': we have not set our clocks ahead of
                    ///< standard time
        };

        ZoneRule() = default;

        constexpr explicit ZoneRule( i32 offset ) noexcept
        : data_( fromOffsetBytes( u32( offset ) ) ) {}

        constexpr ZoneRule( hyphen_t ) noexcept
        : data_{ 17, { '-' } } {}

        constexpr ZoneRule( FixStr<15> str ) noexcept
        : data_( str ) {}

        constexpr Kind kind() const noexcept {
            return isNamed() ? NAMED : isOffset() ? OFFSET : HYPHEN;
        }

        /// Returns true if there is no rule for the zone entry (eg, the rule
        /// was '-')
        constexpr bool isHyphen() const noexcept { return data_.size_ == 17; }
        /// Returns true if this rule is an offset
        constexpr bool isOffset() const noexcept { return data_.size_ == 16; }
        /// Returns true if this is a named rule (eg, 'US')
        constexpr bool isNamed() const noexcept { return data_.size_ < 16; }

        constexpr char const* data() const noexcept { return data_.buff_; }
        constexpr size_t size() const noexcept { return data_.size_ & 0xf; }
        /// Returns the name (or an empty string if the rule is an offset)
        constexpr string_view name() const noexcept {
            return string_view( data_.buff_, data_.size_ & 0xf );
        }

        /// Return the offset. This is the 'save' of the zone at the current moment.
        constexpr i32 offset() const noexcept {
            static_assert( offsetof( FixStr<15>, buff_ ) == 1 );
            return i32( _impl::_load4( data_.buff_ + 3 ) );
        }

        /// Return the offset as a RuleSave
        constexpr RuleSave save() const noexcept { return offset(); }

        string str() const;

        bool operator==( ZoneRule const& rhs ) const noexcept {
            return B16( *this ) == B16( rhs );
        }

        bool operator!=( ZoneRule const& rhs ) const noexcept {
            return B16( *this ) != B16( rhs );
        }

      private:

        FixStr<15> data_;

        constexpr static FixStr<15> fromOffsetBytes( u32 bb ) noexcept {
            return FixStr<15>{
                16,
                {
                    0,
                    0,
                    0,
                    char( bb & 0xff ),
                    char( ( bb >> 8 ) & 0xff ),
                    char( ( bb >> 16 ) & 0xff ),
                    char( ( bb >> 24 ) & 0xff ),
                },
            };
        }
    };
    inline string format_as( ZoneRule const& rule ) { return rule.str(); }


    static_assert( ZoneRule().kind() == ZoneRule::NAMED );
    static_assert( ZoneRule().name() == string_view() );
    static_assert(
        ZoneRule( FixStr<15>{ 2, "US" } ).kind() == ZoneRule::NAMED );
    static_assert( ZoneRule( FixStr<15>{ 2, "US" } ).name() == "US" );
    static_assert( ZoneRule( FixStr<15>{ 12, "Indianapolis" } ).kind()
                   == ZoneRule::NAMED );
    static_assert(
        ZoneRule( FixStr<15>{ 12, "Indianapolis" } ).name() == "Indianapolis" );

    static_assert( ZoneRule( ZoneRule::hyphen ).name() == "-" );
    static_assert( ZoneRule( ZoneRule::hyphen ).kind() == ZoneRule::HYPHEN );

    static_assert( ZoneRule( 0 ).kind() == ZoneRule::OFFSET );
    static_assert( ZoneRule( 3600 ).offset() == 3600 );
    static_assert( ZoneRule( -3600 ).offset() == -3600 );
} // namespace vtz
