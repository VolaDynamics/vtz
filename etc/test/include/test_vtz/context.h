#pragma once

template<class F>
struct _ctx : F {
    using F::operator();
};
template<class F>
_ctx( F ) -> _ctx<F>;

template<class Stream, class F>
decltype( auto ) operator<<( Stream& os, _ctx<F> const& ctx ) {
    return os << ctx();
}
