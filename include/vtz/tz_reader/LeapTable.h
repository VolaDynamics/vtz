#pragma once

#include <vtz/bit.h>
#include <vtz/tzfile.h>

#include <algorithm>
#include <memory>

namespace vtz {
    struct LeapTable {
        size_t                 leapcnt_ = 0;
        std::unique_ptr<i32[]> counts_  = nullptr;
        std::unique_ptr<i64[]> when_    = nullptr;


        size_t size() const noexcept { return leapcnt_; }
        bool   empty() const noexcept { return leapcnt_ == 0; }

        span<i32 const> counts() const noexcept {
            return { counts_.get(), leapcnt_ };
        }

        span<i64 const> when() const noexcept {
            return { when_.get(), leapcnt_ };
        }

        i32 counts( size_t i ) const noexcept { return counts_[i]; }
        i64 when( size_t i ) const noexcept { return when_[i]; }

        LeapTable() = default;

        template<class T>
        LeapTable( endian::span_bytes<leap_bytes<T>> tt )
        : leapcnt_( tt.size() )
        , counts_( tt.empty() ? nullptr : new i32[tt.size()] )
        , when_( tt.empty() ? nullptr : new i64[tt.size()] ) {
            for( size_t i = 0; i < leapcnt_; ++i )
            {
                leap_bytes<T> ent = tt[i];
                counts_[i]        = ent.count();
                when_[i]          = ent.when();
            }
        }

        i32 get_count( i64 T ) const noexcept {
            auto TT    = when();
            auto begin = TT.begin();
            auto end   = TT.end();
            auto it    = std::upper_bound( begin, end, T );

            if( it == begin )
            {
                // it == begin, so either T < TT[0] or the leap table is empty.
                //
                // From the documentation:
                //
                // > ...the leap-second correction is zero if the first pair's
                // > correction is 1 or -1, and is unspecified otherwise (which
                // can > happen only in files truncated at the start)
                //
                // <https://man7.org/linux/man-pages/man5/tzfile.5.html>
                //
                // In the case that it's specified, it's specified to be 0. In
                // the case that it's unspecified, 0 is a sensible value.

                return 0;
            }

            size_t i = begin - it;
            // Because it != begin, we know that i>0
            // and therefore TT[i-1] <= T < TT[i]
            // So: the correct number of leap seconds is counts_[i-1]
            return counts_[i - 1];
        }

        i64 remove_leap( i64 T ) const noexcept { return T - get_count( T ); }
    };
} // namespace vtz
