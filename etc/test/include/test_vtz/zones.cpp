#include <test_vtz/zones.h>
#include <vtz/impl/macros.h>
#include <vtz/known_zones.h>

#include <string>

#include <algorithm>

auto get_test_zones() -> std::vector<std::string_view> const& {
    using vtz::KNOWN_ZONES;
    static std::vector<std::string_view> result = [] {
        auto zz
            = std::vector( std::begin( KNOWN_ZONES ), std::end( KNOWN_ZONES ) );

        // Remove the 'Factory' zone - it isn't loaded by date
        auto filt = []( std::string_view name ) { return name == "Factory"; };
        zz.erase( std::remove_if( zz.begin(), zz.end(), filt ), zz.end() );

        std::sort( zz.begin(), zz.end() );
        return zz;
    }();

    return result;
}

namespace {
    struct _get_size {
        template<size_t N>
        size_t operator()( char const ( & )[N] ) const noexcept {
            return N - 1;
        }
        size_t operator()( char ) const noexcept { return 1; }
    };
    struct _copy_to {
        char* dest;
        template<size_t N>
        void operator()( char const ( &arr )[N] ) noexcept {
            _vtz_memcpy( dest, arr, N - 1 );
            dest += N - 1;
        }
        void operator()( char c ) noexcept {
            bool is_alpha = 'a' <= c && c <= 'z'    // lower
                            || 'A' <= c && c <= 'Z' // upper
                            || '0' <= c && c <= '9' // digits
                            || c == '_';

            *dest++ = is_alpha ? c : '_';
        }
    };

    template<class F>
    void _esc_to( std::string_view input, std::string& dest, F escape_rule ) {
        size_t size = 0;
        for( char c : input ) { size += escape_rule( c, _get_size() ); }

        auto old_size = dest.size();
        dest.resize( old_size + size );
        auto do_write = _copy_to{ dest.data() + old_size };
        for( char c : input ) escape_rule( c, do_write );
    }

    template<class F>
    std::string _esc( std::string_view input, F escape_rule ) {
        std::string result;
        _esc_to( input, result, escape_rule );
        return result;
    }
} // namespace

auto esc_zone_name( std::string_view zone_name ) -> std::string {
    return _esc( zone_name, []( char ch, auto&& write ) {
        switch( ch )
        {
        case '+': return write( "_plus_" );
        }
        return write( ch );
    } );
}
