#pragma once

#include <string_view>

namespace vtz {
    using std::string_view;

    /// Represents a Link entry in a timezone source file.
    ///
    /// Examples (from `tzdata.zi`):
    ///
    /// ```
    /// L Etc/GMT GMT
    /// L Australia/Sydney Australia/ACT
    /// L Australia/Lord_Howe Australia/LHI
    /// L Australia/Sydney Australia/NSW
    /// ```
    ///
    /// Format is `L <canonical name> <alias>`.
    struct zone_link {
        string_view canonical;
        string_view alias;

        bool operator==( zone_link rhs ) const noexcept {
            return canonical == rhs.canonical //
                   && alias == rhs.alias;
        }

        /// This operator is provided so that a Link is implicitly convertible
        /// to a `{Key, Value}` pair, such that the key is the alias, and the
        /// value is the canonical entry, which it refers to.
        ///
        /// Makes it easy to construct a map from a span of links.
        template<class KV>
        explicit operator KV() const {
            return KV{ alias, canonical };
        }
    };
} // namespace vtz
