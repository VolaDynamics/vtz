#include <mutex>
#include <vtz/date_types.h>
#include <vtz/tz_cache.h>

#include <fmt/ranges.h>
#include <string>
#include <string_view>

namespace vtz {

    RuleEvalResult const& TimeZoneCache::locate_rule( string_view name ) const {
        try
        {
            auto& entry = rule_cache.at( name );
            if( auto ptr = entry.load() ) return *ptr;

            // Add the rule to the rule cache
            return *entry.assign_once( load_rule( name ) );
        }
        catch( std::exception const& ex )
        {
            throw std::runtime_error( fmt::format( "Unable to load rule {}. {}",
                escape_string( name ),
                ex.what() ) );
        }
    }


    std::unique_ptr<TimeZone> TimeZoneCache::load_zone(
        string_view name ) const {
        return std::make_unique<TimeZone>( name, compute_states( name ) );
    }


    TimeZone const* TimeZoneCache::try_locate_zone( string_view name ) const {
        auto it = zone_cache.find( name );

        if( it == zone_cache.end() ) return nullptr;

        auto& ent = it->second;
        if( auto ptr = ent.load() ) return ptr;

        return ent.assign_once( load_zone( it->first ) );
    }


    TimeZone const& TimeZoneCache::locate_zone( string_view name ) const {
        // If we successfully found the zone, dereference + return
        if( auto ptr = try_locate_zone( name ) ) return *ptr;

        auto it = data.links.find( name );
        if( it == data.links.end() )
        {
            // If the zone info directory is empty, don't bother checking the
            // fallback cache, because that will fail.
            if( zoneinfo_dir.empty() )
            {
                throw std::runtime_error( fmt::format(
                    "Could not find {} (checked both canonical "
                    "zones & legacy names; zoneinfo_dir is empty so no attempt "
                    "was made to load from a OS tzfile)",
                    escape_string( name ) ) );
            }

            try
            {
                // Attempt to locate the zone within the fallback cache
                auto tz = fallback_cache.locate_zone(
                    name, [this]( string_view name ) {
                        return this->load_zone( name );
                    } );
                if( tz == nullptr )
                    throw std::runtime_error( "fallback cache returned a null "
                                              "zone (is load_zone broken?)" );

                return *tz;
            }
            catch( std::exception const& e )
            {
                throw std::runtime_error( fmt::format(
                    "Could not find {} (checked both canonical zones & legacy "
                    "names). Attempted to load tzfile from {}, but the "
                    "following error occurred: {}",
                    escape_string( name ),
                    zoneinfo_dir,
                    e.what() ) );
            }
        }

        // Get the zone this link refers to
        auto canonical = it->second;
        if( auto ptr = try_locate_zone( canonical ) ) return *ptr;

        throw std::runtime_error(
            fmt::format( "{} is an alias for {}, but {} could not be found",
                escape_string( name ),
                escape_string( canonical ),
                escape_string( canonical ) ) );
    }

    static std::string get_tzdata_path() {
        constexpr char const* env_vars[]{
            "VOLA_TZDATA_PATH",
            "VOLAR_TZDATA_PATH",
        };

        for( char const* env_var : env_vars )
        {
            char const* tzdata = std::getenv( env_var );
            if( tzdata ) { return join_path( tzdata, "tzdata" ); }
        }

        throw std::runtime_error(
            "Unable to determine VOLA_TZDATA_PATH. Please configure the "
            "VOLA_TZDATA_PATH env variable to the directory containing "
            "'tzdata'" );
    }


    static std::atomic_bool TIMEZONE_DATABASE_HAS_BEEN_LOADED = false;
    static std::mutex       INSTALL_PATH_MUTEX{};

    /// Return a reference to the install path. The first time this function is
    /// called, the _install path is initialized.
    static std::string& install_path( bool load_from_env_var ) {
        static std::string _install
            = load_from_env_var ? get_tzdata_path() : std::string();

        return _install;
    }

    void set_install( std::string path ) {
        std::scoped_lock _lock( INSTALL_PATH_MUTEX );

        if( TIMEZONE_DATABASE_HAS_BEEN_LOADED )
        {
            throw std::runtime_error(
                "set_install(): cannot change install path after timezone "
                "database has already been loaded." );
        }

        install_path( false ) = std::move( path );
    }

    std::string get_install() {
        std::scoped_lock _lock( INSTALL_PATH_MUTEX );

        // Return the install path. If no install path has been set, load it
        // from an environment variable.
        return install_path( true );
    }

    static TimeZoneCache do_load_cache() {
        std::scoped_lock _lock( INSTALL_PATH_MUTEX );

        auto result
            = TimeZoneCache( load_zone_info_from_dir( install_path( true ) ) );

        // if result loaded successfully, mark this as true
        TIMEZONE_DATABASE_HAS_BEEN_LOADED = true;

        return result;
    }

    TimeZoneCache const& tzdb_cache() {
        static const TimeZoneCache cache( do_load_cache() );
        return cache;
    }

    std::string tzdb_version() { return tzdb_cache().data.version; }

    time_zone const* locate_zone( string_view name ) {
        auto const& cache = tzdb_cache();

        try
        {
            // Attempt to load the timezone. We'll provide some context if this
            // function fails.
            return &cache.locate_zone( name );
        }
        catch( std::exception const& ex )
        {
            throw std::runtime_error( fmt::format(
                "locate_zone(): Unable to locate {} (source files: {}). {}",
                escape_string( name ),
                cache.data.source_files(),
                ex.what() ) );
        }
    }


} // namespace vtz
