#pragma once

#include <vtz/types.h>
#include <string_view>

namespace vtz {
    using std::string_view;

    template<size_t N>
    struct FixStr {
        u8   size_{};
        char buff_[N]{};

        constexpr static size_t max_size = N;


        constexpr string_view sv() const noexcept { return { data(), size() }; }

        explicit operator string_view() const noexcept { return sv(); }

        constexpr size_t      size() const noexcept { return size_; }
        constexpr char const* data() const noexcept { return buff_; }

        constexpr bool operator==( FixStr const& rhs ) const noexcept {
            return sv() == rhs.sv();
        }

        constexpr char const* begin() const noexcept { return buff_; }
        constexpr char const* end() const noexcept { return buff_ + size_; }

        using value_type = char;
    };
} // namespace vtz
