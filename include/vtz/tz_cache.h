#pragma once


#include <atomic>
#include <fmt/format.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vtz/bit.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>
#include <cstddef>

#include <ankerl/unordered_dense.h>


namespace vtz {
    using std::string;
    using std::string_view;
    using std::vector;

    using ankerl::unordered_dense::map;

    template<class T>
    class AtomicEnt {
        mutable std::atomic<T const*> tz = nullptr;

      public:

        AtomicEnt() = default;

        AtomicEnt( std::nullptr_t )
        : tz( nullptr ) {}

        AtomicEnt( std::unique_ptr<T>&& tz ) noexcept
        : tz( tz.release() ) {}

        AtomicEnt( AtomicEnt&& rhs )
        : tz( rhs.tz.exchange(
              nullptr, std::memory_order::memory_order_relaxed ) ) {}

        T const* load() const noexcept {
            return tz.load( std::memory_order_relaxed );
        }
        /// If this entry does not already have a value, atomically assigns the
        /// given input.
        ///
        /// This method is marked as 'const' because it can be safely
        /// invoked by multiple threads simultaneously, and because it's
        /// idempotent - once a value is assigned, it won't be changed.
        ///
        /// If the assignment was successful, then this entry will take
        /// ownership of the input.
        ///
        /// @return the new value (if an assignment occurred) or the existing
        /// value (if this entry already held some value)
        [[nodiscard]] T const* assign_once(
            std::unique_ptr<T> tzptr ) const noexcept {
            T const* expected = nullptr;

            if( tz.compare_exchange_strong( expected, tzptr.get() ) )
            {
                // If the exchange succeeded, then
                // 1. Our _old_ value was null, so there was nothing to destroy
                // 2. Our current value is now set to 'ptr', so we own ptr.
                //
                // We can return ptr, which is the newly owned timezone. Also,
                // we can release it from the unique_ptr, since we now own it.
                return tzptr.release();
            }
            else
            {
                // If the exchange failed, then we already stored some timezone!
                // If this happens, then no release occurs, and we can just
                // return the observed value
                return expected;
            }
        }

        ~AtomicEnt() {
            auto* ptr = tz.load( std::memory_order::memory_order_relaxed );
            delete ptr;
        }
    };


    template<class T, class K, class V>
    auto init_empty_map( map<K, V> const& m ) -> map<K, AtomicEnt<T>> {
        using result_t   = map<string_view, AtomicEnt<T>>;
        using value_type = typename result_t::value_type;
        struct Ent {
            K key;

            operator value_type() const { return { key, nullptr }; }
        };

        vector<Ent> values( m.size() );
        size_t      i = 0;
        for( auto const& [k, _] : m ) { values[i++] = Ent{ k }; }

        return result_t(
            values.data(), values.data() + values.size(), m.size() * 2 );
    }

    struct TimeZoneCache {
        /// These are the zones that have been successfully loaded
        map<string_view, AtomicEnt<TimeZone>> zone_cache;

        /// These are the rules that have been successfully loaded
        map<string_view, AtomicEnt<RuleEvalResult>> rule_cache;

        TZData data;

        /// Initialize the TimeZoneCache.
        explicit TimeZoneCache( TZData&& data )
        : zone_cache( init_empty_map<TimeZone>( data.zones ) )
        , rule_cache( init_empty_map<RuleEvalResult>( data.rules ) )
        , data( std::move( data ) ) {}

        std::unique_ptr<RuleEvalResult> load_rule( string_view name ) const {
            return std::make_unique<RuleEvalResult>(
                data.evaluate_rules( name ) );
        }

        ZoneStates compute_states( string_view name ) const;

        /// Returns an evaluated rule, loading it if necessary. If a load
        /// occurs, the rule will be added to the rule cache.
        RuleEvalResult const& locate_rule( string_view name ) const;

        /// Attempts to load a zone by the canonical name.
        ///
        /// @return the loaded zone
        /// @throws an exception if there was an error parsing or loading the
        /// zone
        std::unique_ptr<TimeZone> load_zone( string_view name ) const;

        /// Attempt to locate a zone. Loads the zone if necessary.
        ///
        /// Returns null if the zone could not be found.
        TimeZone const* try_locate_zone( string_view name ) const;

        /// Returns a timezone, loading it if necessary. If a load occurs,
        /// the zone will be added to the zone cache.
        ///
        /// Any rules that are loaded as part of loading the zone will also
        /// be cached.
        TimeZone const& locate_zone( string_view name ) const;
    };


    TimeZoneCache const& tzdb_cache();
} // namespace vtz
