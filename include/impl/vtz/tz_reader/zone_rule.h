#pragma once

#include <vtz/impl/bit.h>
#include <vtz/strings.h>
#include <vtz/tz_reader/zone_save.h>

namespace vtz {
    using rule_name = fix_str<15>;

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

    struct alignas( u64 ) zone_rule {
        struct hyphen_t {};
        constexpr static hyphen_t hyphen{};

        enum kind_t {
            NAMED,  ///< Zone Rule is a named rule (eg, 'US')
            OFFSET, ///< We have set our clocks ahead of STDOFF by the given
                    ///< amount
            HYPHEN, ///< 'RULES' is '-': we have not set our clocks ahead of
                    ///< standard time
        };

        zone_rule() = default;

        constexpr explicit zone_rule( i32 offset ) noexcept
        : data_( from_offset_bytes( u32( offset ) ) ) {}

        constexpr zone_rule( hyphen_t ) noexcept
        : data_{ 17, { '-' } } {}

        constexpr zone_rule( rule_name str ) noexcept
        : data_( str ) {}

        constexpr kind_t kind() const noexcept {
            return is_named() ? NAMED : is_offset() ? OFFSET : HYPHEN;
        }

        /// Returns true if there is no rule for the zone entry (eg, the rule
        /// was '-')
        constexpr bool is_hyphen() const noexcept { return data_.size_ == 17; }
        /// Returns true if this rule is an offset
        constexpr bool is_offset() const noexcept { return data_.size_ == 16; }
        /// Returns true if this is a named rule (eg, 'US')
        constexpr bool is_named() const noexcept { return data_.size_ < 16; }

        constexpr char const* data() const noexcept { return data_.buff_; }
        constexpr size_t size() const noexcept { return data_.size_ & 0xf; }
        /// Returns the name (or an empty string if the rule is an offset)
        constexpr string_view name() const noexcept {
            return string_view( data_.buff_, data_.size_ & 0xf );
        }

        /// Return the offset. This is the 'save' of the zone at the current
        /// moment.
        constexpr i32 offset() const noexcept {
            static_assert( offsetof( rule_name, buff_ ) == 1 );
            return i32( _impl::_load4( data_.buff_ + 3 ) );
        }

        /// Return the offset as a zone_save
        constexpr zone_save save() const noexcept { return offset(); }

        bool operator==( zone_rule const& rhs ) const noexcept {
            return _b16( *this ) == _b16( rhs );
        }

        bool operator!=( zone_rule const& rhs ) const noexcept {
            return _b16( *this ) != _b16( rhs );
        }

      private:

        rule_name data_;

        constexpr static rule_name from_offset_bytes( u32 bb ) noexcept {
            return rule_name{
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


    static_assert( zone_rule().kind() == zone_rule::NAMED );
    static_assert( zone_rule().name() == string_view() );
    static_assert(
        zone_rule( rule_name{ 2, "US" } ).kind() == zone_rule::NAMED );
    static_assert( zone_rule( rule_name{ 2, "US" } ).name() == "US" );
    static_assert( zone_rule( rule_name{ 12, "Indianapolis" } ).kind()
                   == zone_rule::NAMED );
    static_assert(
        zone_rule( rule_name{ 12, "Indianapolis" } ).name() == "Indianapolis" );

    static_assert( zone_rule( zone_rule::hyphen ).name() == "-" );
    static_assert( zone_rule( zone_rule::hyphen ).kind() == zone_rule::HYPHEN );

    static_assert( zone_rule( 0 ).kind() == zone_rule::OFFSET );
    static_assert( zone_rule( 3600 ).offset() == 3600 );
    static_assert( zone_rule( -3600 ).offset() == -3600 );
} // namespace vtz
