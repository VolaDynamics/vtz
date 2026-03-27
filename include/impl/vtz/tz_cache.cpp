#include <atomic>
#include <mutex>
#include <vtz/date_types.h>
#include <vtz/embedded_tzdb.h>
#include <vtz/known_zones.h>
#include <vtz/tz_cache.h>

// Needed for declarations with VTZ_EXPORT annotation
#include <vtz/tz.h>

#include <string>
#include <string_view>
#include <vtz/util/microfmt.h>


#ifndef VTZ_TZDATA_PATH_VARS
    #define VTZ_TZDATA_PATH_VARS "VTZ_TZDATA_PATH"
#endif


#if defined( VTZ_REQUIRE_TZDATA )
    #define REQUIRE_TZDATA 1
#else
    #define REQUIRE_TZDATA 0
#endif

#if defined( VTZ_CHECK_IN_TZDATA )
    #define CHECK_IN_TZDATA 1
#else
    #define CHECK_IN_TZDATA 0
#endif

namespace vtz {

    rule_eval_result const& time_zone_cache::locate_rule(
        string_view name ) const {
        try
        {
            auto& entry = rule_cache.at( name );
            if( auto ptr = entry.load() ) return *ptr;

            // Add the rule to the rule cache
            return *entry.assign_once( load_rule( name ) );
        }
        catch( std::exception const& ex )
        {
            throw std::runtime_error( util::join( "Unable to load rule ",
                escape_string( name ),
                ". ",
                ex.what() ) );
        }
    }


    std::unique_ptr<time_zone> time_zone_cache::load_zone(
        string_view name ) const {
        return std::make_unique<time_zone>( name, compute_states( name ) );
    }


    time_zone const* time_zone_cache::try_locate_zone(
        string_view name ) const {
        auto it = zone_cache.find( name );

        if( it == zone_cache.end() ) return nullptr;

        auto& ent = it->second;
        if( auto ptr = ent.load() ) return ptr;

        return ent.assign_once( load_zone( it->first ) );
    }


    time_zone const& time_zone_cache::locate_zone( string_view name ) const {
        // If we successfully found the zone, dereference + return
        if( auto ptr = try_locate_zone( name ) ) return *ptr;

        auto it = data.links.find( name );
        if( it == data.links.end() )
        {
            // If the zone info directory is empty, don't bother checking the
            // fallback cache, because that will fail.
            if( zoneinfo_dir.empty() )
            {
                throw std::runtime_error( util::join( "Could not find ",
                    escape_string( name ),
                    " (checked both canonical "
                    "zones & legacy names; zoneinfo_dir is empty so no "
                    "attempt "
                    "was made to load from a OS tzfile)" ) );
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
                throw std::runtime_error( util::join( "Could not find ",
                    escape_string( name ),
                    " (checked both canonical zones & legacy "
                    "names). Attempted to load tzfile from ",
                    escape_string( zoneinfo_dir ),
                    ", but the following error occurred: ",
                    e.what() ) );
            }
        }

        // Get the zone this link refers to
        auto canonical = it->second;
        if( auto ptr = try_locate_zone( canonical ) ) return *ptr;

        throw std::runtime_error( util::join( escape_string( name ),
            " is an alias for ",
            escape_string( canonical ),
            ", but ",
            escape_string( canonical ),
            " could not be found" ) );
    }


    constexpr static char const* tzdata_env_vars[]{ VTZ_TZDATA_PATH_VARS };

    static std::string _get_path_from( char const* tzdata ) {
#if CHECK_IN_TZDATA
        return join_path( tzdata, "tzdata" );
#else
        return tzdata;
#endif
    }

    /// Attempts to determine the tzdata path.
    static std::string get_tzdata_path() {
        for( char const* env_var : tzdata_env_vars )
        {
            char const* tzdata = std::getenv( env_var );
            if( tzdata ) return _get_path_from( tzdata );
        }

        return std::string();
    }

    bool disable_embedded_tzdb() {
        const static bool is_disabled = [] {
            char const* disallow_embed
                = std::getenv( "VTZ_DISALLOW_EMBEDDED_TZDB" );
            return disallow_embed && disallow_embed == std::string_view( "1" );
        }();
        return is_disabled;
    }

    static std::string get_tzdata_path_error(
        std::string_view what, std::string_view install_path ) {
        std::string msg = //
            util::join( "Unable to load the tz database: ",
                what,
                "\n\nChecked the following locations:\n\n" );

        bool env_vars_null = true;
        for( char const* env_var : tzdata_env_vars )
        {
            char const* tzdata = std::getenv( env_var );
            if( tzdata == nullptr )
            {
                msg += util::join(
                    "- getenv(", escape_string( env_var ), ") -> nil\n" );
                continue;
            }

            env_vars_null = false;

            msg += util::join( "- getenv(",
                escape_string( env_var ),
                ") -> ",
                escape_string( _get_path_from( tzdata ) ),
                '\n' );

            break; // We don't check additional env vars after the first
                   // non-skipped env var
        }
        if( !install_path.empty() )
        {
            msg += util::join(
                "- get_install() -> ", escape_string( install_path ), '\n' );
        }

        std::string_view embedded_tzdata     = impl::get_embedded_tzdata();
        bool             has_embedded_tzdata = !embedded_tzdata.empty();

        if( !has_embedded_tzdata )
            msg += "- get_embedded_tzdata() -> None\n";
        else
        {
            msg += util::join( "- get_embedded_tzdata() -> \"...\" (size=",
                embedded_tzdata.size(),
                " bytes)\n" );

            if( disable_embedded_tzdb() )
            {
                msg += "  (embedded tzdata is present, but disabled due to "
                       "environment settings)";
            }
        }


        if( !install_path.empty() && env_vars_null )
        {
            msg += "\n"
                   "All env vars are null, so vtz::set_install() must have "
                   "been called.\n";
        }

        if( has_embedded_tzdata )
        {
            msg += "\n"
                   "Note: env variables and the install path take precedence "
                   "over embedded tzdata, so if either of those are set, "
                   "embedded tzdata will not be used.";
        }

        msg += "\n"
               "Please configure one of the above (or call vtz::set_install()) "
               "so that your application can find the tz database.\n"
               "\n"
               "The timezone database may be downloaded at "
               "https://www.iana.org/time-zones\n"
               "\n"
               "To use the timezone database, unpack one of these source "
               "files, and configure the environment to point to that "
               "directory.\n";

#if CHECK_IN_TZDATA
        msg += "\n"
               "Note: This application checks for tzdata source files in the "
               "directory given by getenv(...)/tzdata/\n";
#else
        msg += "\n"
               "Note: This application checks for tzdata source files in the "
               "directory given by getenv(...)\n";
#endif

        return msg;
    }


    static std::atomic_bool TIMEZONE_DATABASE_HAS_BEEN_LOADED = false;
    static std::mutex       INSTALL_PATH_MUTEX{};

    /// Return a reference to the install path. The first time this function is
    /// called, the _install path is initialized.
    ///
    /// If none of the tzpath environment variables are set, the returned path
    /// will be empty.
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

    static time_zone_cache do_load_cache() {
        std::scoped_lock _lock( INSTALL_PATH_MUTEX );

        auto const& path = install_path( true );

        if( path.empty() )
        {
            // Attempt to get the embedded tzdata. If tzdata has not been
            // embedded, `get_embedded_tzdata()` returns an empty string_view
            std::string_view tzdata = impl::get_embedded_tzdata();
            if( !tzdata.empty() && !disable_embedded_tzdb() )
            {
                auto result = time_zone_cache(
                    load_zone_info_from_sv( tzdata, "(embedded tzdata.zi)" ) );

                TIMEZONE_DATABASE_HAS_BEEN_LOADED = true;

                return result;
            }

#if REQUIRE_TZDATA
            throw std::runtime_error( get_tzdata_path_error(
                "Cannot determine install path.", path ) );
#else

            std::string zoneinfo_dir;
            if( char const* tzdir = std::getenv( "TZDIR" ) )
            {
                // if the TZDIR environment variable is set, we want to use that
                // as the location for our zoneinfo files
                zoneinfo_dir = tzdir;
            }
            else
            {
    #if _WIN32
                throw std::runtime_error(
                    "Unable to load tz database (no install path is set; "
                    "embedded tz data is missing or disabled; TZDIR is not "
                    "set)" );
    #else
                // if the TZDIR env var is *not* set, use /usr/share/zoneinfo,
                // as per usual

                // TODO: do we want to support checking any other directories
                // for zoneinfo?
                zoneinfo_dir = "/usr/share/zoneinfo";
    #endif
            }

            auto result = time_zone_cache(
                std::move( zoneinfo_dir ), KNOWN_ZONES, KNOWN_LINKS );

            TIMEZONE_DATABASE_HAS_BEEN_LOADED = true;

            return result;
#endif
        }
        try
        {
            auto result = time_zone_cache( load_zone_info_from_dir( path ) );

            // if result loaded successfully, mark this as true
            TIMEZONE_DATABASE_HAS_BEEN_LOADED = true;

            return result;
        }
        catch( std::exception const& ex )
        {
            // Provide helpful context and rethrow
            throw std::runtime_error(
                get_tzdata_path_error( ex.what(), path ) );
        }
    }


    class tzdb_cache_pool {
        struct node {
            time_zone_cache             tz_cache;
            std::unique_ptr<node const> next;
        };

      public:

        tzdb_cache_pool() = default;

        tzdb_cache_pool( tzdb_cache_pool const& ) = delete;
        tzdb_cache_pool( tzdb_cache_pool&& )      = delete;

        VTZ_INLINE time_zone_cache const& cache() {
            if( auto* p = head.load( std::memory_order_relaxed ) ) VTZ_LIKELY
                return p->tz_cache;

            // Load the time zone cache
            std::call_once( once, [this] { reload(); } );

            return head.load( std::memory_order_relaxed )->tz_cache;
        }


        time_zone_cache const& reload() {
            auto new_head  = new node{ do_load_cache(), nullptr };
            auto old_head  = head.exchange( new_head );
            new_head->next = std::unique_ptr<node const>( old_head );
            return new_head->tz_cache;
        }

        bool is_tzdb_loaded() const noexcept { return head != nullptr; }

        ~tzdb_cache_pool() { delete head.load( std::memory_order_relaxed ); }

      private:

        std::atomic<node const*> head = nullptr;
        std::once_flag           once{};
    };

    static tzdb_cache_pool _tzdb_cache;

    bool is_tzdb_loaded() noexcept { return _tzdb_cache.is_tzdb_loaded(); }

    time_zone_cache const& tzdb_cache() { return _tzdb_cache.cache(); }

    std::string tzdb_version() { return tzdb_cache().data.version; }

    void reload_tzdb( std::string_view path ) {
        // If the install path is given, update the install path.
        if( !path.empty() )
        {
            auto _lock = std::scoped_lock( INSTALL_PATH_MUTEX );

            install_path( false ) = std::string( path );
        }

        _tzdb_cache.reload();
    }

    time_zone const* locate_zone( string_view name ) {
        auto const& cache = _tzdb_cache.cache();

        try
        {
            // Attempt to load the timezone. We'll provide some context if this
            // function fails.
            return &cache.locate_zone( name );
        }
        catch( std::exception const& ex )
        {
            throw std::runtime_error(
                util::join( "locate_zone(): Unable to locate ",
                    escape_string( name ),
                    " (source files: ",
                    util::joined{ cache.data.source_files(), ", " },
                    "). ",
                    ex.what() ) );
        }
    }
} // namespace vtz
