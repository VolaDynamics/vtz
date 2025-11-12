#include <vtz/date_types.h>
#include <vtz/tz_cache.h>

#include <fmt/ranges.h>
#include <string>
#include <string_view>

namespace vtz {

    RuleEvalResult const& TimeZoneCache::locateRule( string_view name ) const {
        try
        {
            auto& entry = ruleCache.at( name );
            if( auto ptr = entry.load() ) return *ptr;

            // Add the rule to the rule cache
            return *entry.assignOnce( loadRule( name ) );
        }
        catch( std::exception const& ex )
        {
            throw std::runtime_error( fmt::format( "Unable to load rule {}. {}",
                escapeString( name ),
                ex.what() ) );
        }
    }


    std::unique_ptr<TimeZone> TimeZoneCache::loadZone(
        string_view name ) const {
        return std::make_unique<TimeZone>( name, computeStates( name ) );
    }


    TimeZone const* TimeZoneCache::tryLocateZone( string_view name ) const {
        auto it = zoneCache.find( name );

        if( it == zoneCache.end() ) return nullptr;

        auto& ent = it->second;
        if( auto ptr = ent.load() ) return ptr;

        return ent.assignOnce( loadZone( it->first ) );
    }


    TimeZone const& TimeZoneCache::locateZone( string_view name ) const {
        // If we successfully found the zone, dereference + return
        if( auto ptr = tryLocateZone( name ) ) return *ptr;

        auto it = data.links.find( name );
        if( it == data.links.end() )
        {
            throw std::runtime_error(
                fmt::format( "Could not find '{}' (checked both canonical "
                             "zones & legacy names)",
                    name ) );
        }

        // Get the zone this link refers to
        auto canonical = it->second;
        if( auto ptr = tryLocateZone( canonical ) ) return *ptr;

        throw std::runtime_error( fmt::format(
            "'{}' is an alias for '{}', but '{}' could not be found",
            name,
            canonical,
            canonical ) );
    }

    static std::string getTZDataPath() {
        constexpr char const* envVars[]{
            "VOLA_TZDATA_PATH",
            "VOLAR_TZDATA_PATH",
        };

        for( char const* envVar : envVars )
        {
            char const* tzdata = std::getenv( envVar );
            if( tzdata ) { return joinPath( tzdata, "tzdata" ); }
        }

        throw std::runtime_error(
            "Unable to determine VOLA_TZDATA_PATH. Please configure the "
            "VOLA_TZDATA_PATH env variable to the directory containing "
            "'tzdata'" );
    }


    TimeZoneCache const& tzdb_cache() {
        static const TimeZoneCache cache(
            loadZoneInfoFromDir( getTZDataPath() ) );
        return cache;
    }


    std::string tzdb_version() {
        return tzdb_cache().data.version;
    }

    time_zone const* locate_zone( string_view name ) {
        auto const& cache = tzdb_cache();

        try
        {
            // Attempt to load the timezone. We'll provide some context if this
            // function fails.
            return &cache.locateZone( name );
        }
        catch( std::exception const& ex )
        {
            throw std::runtime_error( fmt::format(
                "locate_zone(): Unable to locate {} (source files: {}). {}",
                escapeString( name ),
                cache.data.sourceFiles(),
                ex.what() ) );
        }
    }

    time_zone const* current_zone() {
        // TODO: get the current zone for the user
        return locate_zone( "America/New_York" );
    }
} // namespace vtz
