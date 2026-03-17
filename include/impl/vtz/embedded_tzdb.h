#pragma once

#include <string_view>

namespace vtz::impl {
    // Returns an embedded copy of tzdata.zi. If there is no embedded tzdata,
    // returns an empty string_view
    std::string_view get_embedded_tzdata() noexcept;
} // namespace vtz::impl
