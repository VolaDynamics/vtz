#pragma once

#include <vtz/civil.h>
#include <vtz/date_types.h>
#include <vtz/impl/bit.h>

namespace vtz {
    class rule_on {
        u16 repr_;

        constexpr explicit rule_on( u16 repr ) noexcept
        : repr_( repr ) {}

      public:

        enum Kind {
            DAY,      ///< 'on' is a specific day of the month, eg '13'
            DOW_LAST, ///< 'on' is, eg 'lastSun' or 'lastMon'
            DOW_GE,   ///< 'on' is 'Sun>=13', for example
            DOW_LE,   ///< 'on' is 'Fri<=1', for example
        };

        rule_on() = default;

        constexpr rule_on( Kind kind, u8 day, DOW dow ) noexcept
        : repr_(
              u16( u32( kind ) | ( u32( dow ) << 2 ) | ( u32( day ) << 5 ) ) ) {
        }
        constexpr Kind kind() const noexcept { return Kind( repr_ & 0x3 ); }
        constexpr u16  day() const noexcept { return repr_ >> 5; }
        constexpr DOW  dow() const noexcept {
            return DOW( ( repr_ >> 2 ) & 0x7 );
        }


        constexpr static rule_on on( u8 day ) noexcept {
            return { DAY, day, {} };
        }
        constexpr static rule_on last( DOW dow ) noexcept {
            return { DOW_LAST, {}, dow };
        }
        constexpr static rule_on ge( DOW dow, u8 day ) noexcept {
            return { DOW_GE, day, dow };
        }
        constexpr static rule_on le( DOW dow, u8 day ) noexcept {
            return { DOW_LE, day, dow };
        }

        constexpr bool operator==( rule_on const& rhs ) const noexcept {
            return repr_ == rhs.repr_;
        }

        constexpr static rule_on from_repr( u16 repr ) noexcept {
            return rule_on( repr );
        }

        constexpr sysdays_t resolve_date( i32 year, Mon mon ) const noexcept {
            return resolve_date( year, u32( mon ) );
        }

        constexpr sysdays_t resolve_date( i32 year, u32 mon ) const noexcept {
            switch( kind() )
            {
            case DAY: return resolve_civil( year, mon, day() );
            case DOW_LAST: return resolve_last_dow( year, mon, dow() );
            case DOW_GE: return resolve_dow_ge( year, mon, day(), dow() );
            case DOW_LE: return resolve_dow_le( year, mon, day(), dow() );
            }

            VTZ_UNREACHABLE();
        }


        /// Evaluate the rule for a given year/month in order to obtain an
        /// actual date
        constexpr civil_ymd eval( i32 year, Mon mon ) const noexcept {
            switch( kind() )
            {
            case DAY: return civil_ymd( year, mon, day() );
            case DOW_LAST:
                return civil_ymd( year,
                    mon,
                    u16( get_last_dowin_month( year, u32( mon ), dow() ) ) );
            case DOW_GE:
                return get_ymd_dow_ge( year, u32( mon ), day(), dow() );
            case DOW_LE:
                return get_ymd_dow_le( year, u32( mon ), day(), dow() );
            }

            VTZ_UNREACHABLE();
        }
    };


    template<>
    struct opt_traits<rule_on> {
        /// Corresponds to kind() == DAY, dow() == Sun, day() == 0, which is not
        /// a valid day of the month
        constexpr static rule_on NULL_VALUE = rule_on::from_repr( 0 );
    };

    using OptRuleOn = opt_class<rule_on>;
} // namespace vtz
