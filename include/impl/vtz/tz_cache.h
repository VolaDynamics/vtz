#pragma once


#include "vtz/known_zones.h"
#include <atomic>
#include <cstddef>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vtz/impl/bit.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>

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
        : tz( rhs.tz.exchange( nullptr, std::memory_order_relaxed ) ) {}

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
            auto* ptr = tz.load( std::memory_order_relaxed );
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

    template<class T, class K>
    auto init_empty_map( span<K const> keys ) -> map<K, AtomicEnt<T>> {
        using result_t   = map<string_view, AtomicEnt<T>>;
        using value_type = typename result_t::value_type;
        struct Ent {
            K key;
            Ent( K const& key )
            : key( key ) {}

            operator value_type() const { return { key, nullptr }; }
        };

        vector<Ent> values( keys.begin(), keys.end() );

        return result_t(
            values.data(), values.data() + values.size(), keys.size() * 2 );
    }

    /// Allows use of string_view as key for map<std::string, ...>
    struct string_hash {
        /// enable heterogeneous overloads
        using is_transparent = void;

        /// mark class as high quality avalanching hash
        using is_avalanching = void;

        /// Hashing operator
        [[nodiscard]] auto operator()( std::string_view str ) const noexcept
            -> uint64_t {
            return ankerl::unordered_dense::hash<std::string_view>{}( str );
        }
    };

    struct TimeZoneCache;

    /// Provides fallback cache which will be used for discovery of zones not
    /// known on construction of the `TimeZoneCache`.
    ///
    /// There are two scenarios where fallback occurs:
    ///
    /// 1. The timezone source files are out of date, and the user attempts
    ///    to look up a zone not contained within the timezone source files
    /// 2. The timezone source files were not provided, and the user attempts
    ///    to look up a zone that does not appear in the set of known zones
    ///    passed to the `TimeZoneCache`.
    ///
    /// In both cases, we need to fall back to this cache.
    ///
    /// **Why?** The primary cache is a frozen hashmap of keys to mutable,
    /// atomic values. Lookup is very fast, and no locking needs to occur.
    ///
    /// However, it must be constructed upfront: new keys cannot be added
    /// to the hashmap, because then we would have to worry about locking.
    ///
    /// Therefore, if we have a zone which is _present on the system_, but
    /// _not_ in a timezone source file (or the list of known zones), we
    /// must have a way to _add_ the zone to our cache (hence the fallback
    /// cache).
    ///
    /// This fallback cache is slower, and requires taking a lock, however
    /// it's better than failing entirely.
    ///
    /// Use of the fallback cache should be very rare. It will essentially
    /// only occur for obscure timezones that were added very recently,
    /// and as long as timezone source files are kept up to date, this
    /// cache should never be hit.

    class ZoneFallbackCache {
        /// Uses ankerl::unordered_dense::map's [heterogenous overload
        /// capability](https://github.com/martinus/unordered_dense?tab=readme-ov-file#324-heterogeneous-overloads-using-is_transparent)
        using zone_map = map<std::string,
            std::unique_ptr<time_zone>,
            string_hash,
            std::equal_to<>>;

        struct State {
            std::mutex m{};
            zone_map   zones;
        };
        // It's fine to have mutability here, because we put the whole thing
        // behind a lock. So `locate_zone` is still thread-safe.
        mutable std::unique_ptr<State> st = std::make_unique<State>();

      public:

        /// Locate the given timezone. If it is in the cache, we will use the
        /// cache entry. Otherwise, we will load the zone, and add it to the
        /// cache.
        ///
        /// Any mutation of the cache occurs behind a lock, so this function is
        /// thread safe. We will hold the lock for the minimum amount of time
        /// possible - ONLY when mutating the cache.
        ///
        /// The lock will NOT be held during `load_zone`, to ensure that the
        /// cache remains available even while a zone is being loaded.

        template<class F>
        time_zone const* locate_zone(
            std::string_view name, F const& load_zone ) const {
            // Attempt to find the zone within the map
            auto& m     = st->m;
            auto& zones = st->zones;
            {
                auto guard = std::lock_guard( m );
                auto it    = zones.find( name );
                if( it != zones.end() ) return it->second.get();
            }

            // Load the zone. Loading the zone may be a slow operation - since
            // the fallback cache is only invoked for zones loaded from OS
            // tzfiles, we will need to do file IO to load the zone.
            //
            // We DO NOT want to be holding the lock while doing file IO, so
            // loading the zone MUST be done outside of the locked portion.

            std::unique_ptr<time_zone> tz = load_zone( name );

            {
                auto  guard = std::lock_guard( m );
                auto& entry = zones[name];

                // Make sure that the entry is null. If it's not null, another
                // thread added the zone before we did (in which case our work
                // should be discarded). But if it is still null, we can assign
                // the newly loaded zone.
                //
                // Note that another thread loading the same timezone
                // simultaneously should be very rare here, but it must be
                // checked for correctness
                if( entry.get() == nullptr ) { entry = std::move( tz ); }

                return entry.get();
            }
        }
    };


    /// Provides a cache for lookup of time_zone objects.
    ///
    /// If timezone source files were provided to the cache (in the form of a
    /// `TZData` object), then the TimeZoneCache will construct zones from the
    /// source files.
    ///
    /// If a zone cannot be found within the provided source files, but
    /// `zoneinfo_dir` is set, the TimeZoneCache will attempt to load
    /// the tzfile from the zoneinfo fallback directory.
    ///
    /// If a zone cannot be found, then the `TimeZoneCache` will throw a
    /// descriptive error message.

    struct TimeZoneCache {
        /// These are the zones that have been successfully loaded. When
        /// attempting to locate a zone, `locate_zone()` will check here first.
        map<string_view, AtomicEnt<time_zone>> zone_cache;


        /// These are the rules that have been successfully loaded. This cache
        /// is used when loading a zone that has not yet been loaded.
        map<string_view, AtomicEnt<RuleEvalResult>> rule_cache;


        TZData data;

        /// Holds a path to the system zoneinfo directory where tzfiles may be
        /// found.
        ///
        /// On MacOS and Linux this path is typically `/usr/share/zoneinfo`, but
        /// it may differ on older systems.
        ///
        /// If this path is not set, then the TimeZoneCache will not attempt to
        /// load os tzfiles.
        std::string zoneinfo_dir;

        /// Provides a fallback cache, which will be used in the event that a
        /// zone can't be found in the primary zone cache. This is very rare.
        ///
        /// See the documentation for `ZoneFallbackCache` for a full description
        /// of when this occurs.
        ZoneFallbackCache fallback_cache;

        /// Initialize the TimeZoneCache. By default, no `zoneinfo_dir` is set,
        /// but one may be provided to the constructor.
        ///
        /// In the case that a `zoneinfo_dir` is provided, the `TimeZoneCache`
        /// will check there for `tzfile` objects if a timezone cannot be found
        /// within the given zone sourcefiles.

        explicit TimeZoneCache(
            TZData&& data, std::string zoneinfo_dir = std::string() )
        : zone_cache( init_empty_map<time_zone>( data.zones ) )
        , rule_cache( init_empty_map<RuleEvalResult>( data.rules ) )
        , data( std::move( data ) )
        , zoneinfo_dir( std::move( zoneinfo_dir ) ) {}


        /// Construct a `TimeZoneCache` with no source information. Zones will
        /// be loaded from the `zoneinfo_dir`.
        ///
        /// By default, the zone cache will be initialized with slots for zones
        /// in `known_zones` and links in `known_links`. Lookups of known zones
        /// (or known links) is fast, and can be done atomically with no lock.
        ///
        /// Anything not known can still be found and stored in the cache (if
        /// present within `zoneinfo_dir` as a tzfile), but doing so will
        /// require taking a lock. (This is the purpose of `fallback_cache`)

        explicit TimeZoneCache( std::string zoneinfo_dir,
            span<std::string_view const>    known_zones = KNOWN_ZONES,
            span<Link const>                known_links = KNOWN_LINKS )
        : zone_cache( init_empty_map<time_zone>( known_zones ) )
        , rule_cache()
        , data()
        , zoneinfo_dir( std::move( zoneinfo_dir ) ) {
            data.links = LinkMap( known_links.begin(), known_links.end() );
        }


        std::unique_ptr<RuleEvalResult> load_rule( string_view name ) const {
            return std::make_unique<RuleEvalResult>(
                data.evaluate_rules( name ) );
        }


        /// Computes `ZoneStates` for the given timezone.
        ///
        /// If the TimeZoneCache was constructed from a `TZData` object
        /// representing a timezone source file (or set of timezone database
        /// source files), `compute_states()` will attempt to construct the
        /// timezone from that.
        ///
        /// If the timezone source files did not contain the given zone, or the
        /// `TimeZoneCache` was not constructed from a timezone source file,
        /// `compute_states()` will fall back to loading a zone from the
        /// `zoneinfo_dir` if set.
        zone_states compute_states( string_view name ) const;

        /// Returns an evaluated rule, loading it if necessary. If a load
        /// occurs, the rule will be added to the rule cache.
        RuleEvalResult const& locate_rule( string_view name ) const;

        /// Attempts to load a zone by the canonical name.
        ///
        /// @return the loaded zone
        /// @throws an exception if there was an error parsing or loading the
        /// zone
        std::unique_ptr<time_zone> load_zone( string_view name ) const;

        /// Attempt to locate a zone. Loads the zone if necessary.
        ///
        /// Returns null if the zone could not be found.
        time_zone const* try_locate_zone( string_view name ) const;

        /// Returns a timezone, loading it if necessary. If a load occurs,
        /// the zone will be added to the zone cache.
        ///
        /// Any rules that are loaded as part of loading the zone will also
        /// be cached.
        time_zone const& locate_zone( string_view name ) const;

        std::vector<std::string_view> zones() {
            auto result = std::vector<std::string_view>( zone_cache.size() );

            size_t i = 0;
            for( auto const& kv : zone_cache ) { result[i++] = kv.first; }

            return result;
        }

        std::vector<Link> links() {
            auto result = std::vector<Link>( data.links.size() );

            size_t i = 0;
            for( auto const& [key, value] : data.links )
            {
                // value goes first because the key is the alias, which resolves
                // to a canonical name
                result[i++] = Link{ value, key };
            }

            return result;
        }

        /// Returns true if the TimeZoneCache has a zoneinfo directory
        /// containing OS tzfiles, which can be used if the
        bool has_zoneinfo_dir() const noexcept { return !zoneinfo_dir.empty(); }
    };

    TZData load_zone_info_from_dir( string dir );

    TimeZoneCache const& tzdb_cache();
} // namespace vtz
