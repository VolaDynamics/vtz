#include <iterator>
#include <vtz/embedded_tzdb.h>

namespace vtz::impl {
#if VTZ_EMBED_TZDB
    /// Holds the contents of `tzdata.zi` as a single, chonky string literal.
    ///
    /// C++26 introduces a really nice feature, #embed
    /// See: https://en.cppreference.com/w/cpp/preprocessor/embed.html
    ///
    /// Unfortunately, vtz intends to support older compilers and systems that do
    /// not yet have #embed.
    constexpr static char const tzdata[] =
    #include <vtz/embedded_tzdb_content.h>
        ;
#endif


    std::string_view get_embedded_tzdata() noexcept {
#if VTZ_EMBED_TZDB
        constexpr size_t tzdata_size = std::size( tzdata ) - 1;
        return std::string_view( tzdata, tzdata_size );
#else
        return std::string_view();
#endif
    }
} // namespace vtz::impl
