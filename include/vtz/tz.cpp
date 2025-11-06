#include <vtz/civil.h>
#include <vtz/files.h>
#include <vtz/math.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>

#include <algorithm>
#include <atomic>
#include <climits>
#include <ctime>
#include <fmt/format.h>
#include <initializer_list>
#include <limits>

namespace vtz {
    template<class T>
    std::unique_ptr<T[]> _copyUnique( T const* data, size_t count ) {
        // We use memcpy here, so we expect the input to be trivially copyable
        static_assert( std::is_trivially_copyable_v<T> );

        T* buff = new T[count];
        memcpy( buff, data, count * sizeof( T ) );
        return std::unique_ptr<T[]>( buff );
    }

    template<class T>
    auto _copyUnique( span<T> values ) {
        return _copyUnique( values.data(), values.size() );
    }


    /// Return a new (uninitialized) array of the desired size,
    /// containing elements of the desired type
    template<class T>
    std::unique_ptr<T[]> _new( size_t count ) {
        return std::unique_ptr<T[]>{ new T[count] };
    }

    /// Return a new (initialized) array of the desired size,
    /// containing elements of the desired type
    template<class T, class... Args>
    std::unique_ptr<T[]> _init( Args&&... args ) {
        return std::unique_ptr<T[]>{ new T[]{
            T( static_cast<Args&&>( args ) )... } };
    }

    template<class T>
    using UP = std::unique_ptr<T>;


    /// Construct a lookup table corresponding to a constant state (eg, a
    /// timezone with no zone transitions)
    S32Table table1( u32 x ) {
        auto block = _join32( x, x );
        return S32Table{
            63,
            _init<i64>( 0, 0 ),
            _init<u64>( block, block ),
            -1,
            2,
        };
    }


    /// Construct a lookup table with two possible values and one transition
    /// time
    S32Table table2( i64 trans, u32 initial, u32 final ) {
        auto block = _join32( initial, final );
        return S32Table{
            63,
            _init<i64>( trans, trans ),
            _init<u64>( block, block ),
            -1,
            2,
        };
    }

    int computeG( span<sysseconds_t const> tt ) {
        // If there are no transition times, or only one transition time, return
        // the maximum possible g
        if( tt.size() <= 1 ) return 63;
        u64 minDelta = UINT64_MAX;

        for( size_t i = 1; i < tt.size(); ++i )
        {
            auto const& t0 = tt[i - 1];
            auto const& t1 = tt[i];
            if( t1 <= t0 )
            {
                throw std::runtime_error(
                    "TimeZone(): zone transitions were not ordered" );
            }
            auto delta = u64( t1 ) - u64( t0 );

            if( delta < minDelta ) minDelta = delta;
        }

        int g = _blog2( minDelta );

        if( g < 16 )
        {
            throw std::runtime_error( "Error: multiple transition times occur "
                                      "very close together (why?)" );
        }

        return g;
    }

    /// Compute the required block size, taking into account that two possible
    /// local times map to the same UTC time
    int computeGUniversal(
        i32 off0, span<sec_t const> tt, span<i32 const> off ) {
        // If there are no transition times, or only one transition time, return
        // the maximum possible g
        if( tt.size() <= 1 ) return 63;

        u64 minDelta = UINT64_MAX;

        // Latest possible interpretation of time of last block
        sec_t Tlast = tt[0] + std::max( { off0, off[1], 0 } );

        for( size_t i = 1; i < tt.size(); ++i )
        {
            // Transition time (UTC)
            auto T = tt[i];
            // Time when previous block ends (local time)
            auto localEnd = T + off[i - 1];
            // Time when current block starts (local time)
            auto localStart = T + off[i];
            auto Tearly     = std::min( { T, localEnd, localStart } );
            auto Tlate      = std::max( { T, localEnd, localStart } );
            if( Tearly <= Tlast )
            {
                throw std::runtime_error(
                    "TimeZone(): transition times overlap when converted to "
                    "local time" );
            }

            auto delta = u64( Tearly ) - u64( Tlast );
            Tlast      = Tlate;
            minDelta   = std::min( delta, minDelta );
        }

        int g = _blog2( minDelta );

        if( g < 16 )
        {
            throw std::runtime_error( "Error: multiple transition times occur "
                                      "very close together (why?)" );
        }

        return g;
    }


    template<class T>
    S32Table makeTable( T        off0,
        span<sysseconds_t const> tt,
        span<T const>            off,
        sysseconds_t             minT,
        sysseconds_t             maxT ) {
        int g = computeG( tt );

        i64 minIndex = minT >> g;
        i64 maxIndex = 1 + ( maxT >> g );

        size_t buffCount = ( maxIndex - minIndex ) + 1;

        u64 block = _join32( u32( off0 ), u32( off[0] ) );

        auto ttTimes = _new<i64>( buffCount );
        auto blocks  = _new<u64>( buffCount );

        i64 when = tt[0];

        i64 blockT = i64( u64( minIndex ) << g ); // current start time of block
        i64 blockSize = i64( 1 ) << g;            // Block size (in seconds)
        size_t zi     = 0;
        for( size_t bi = 0; bi < buffCount; bi++ )
        {
            ttTimes[bi]  = when;
            blocks[bi]   = block;
            blockT      += blockSize;
            if( blockT >= when )
            {
                zi++;
                if( zi < tt.size() )
                {
                    when      = tt[zi];
                    auto off1 = off[zi];
                    block     = block >> 32 | ( u64( u32( off1 ) ) << 32 );
                }
                else
                {
                    ++bi;
                    for( ; bi < buffCount; ++bi )
                    {
                        ttTimes[bi] = when;
                        blocks[bi]  = block;
                    }
                    break;
                }
            }
        }

        return S32Table{
            g,
            std::move( ttTimes ),
            std::move( blocks ),
            minIndex,
            buffCount,
        };
    }


    /// Construct a single table that can be used for both UTC and local
    /// lookups. Blocks are computed such that independent of whether you're
    /// looking up a UTC time, or a local time, you will end up in a block
    /// containing the relevant transition time.
    template<class T>
    S32Table makeUniversalTable( T off0,
        span<sysseconds_t const>   tt,
        span<T const>              off,
        sysseconds_t               minT,
        sysseconds_t               maxT ) {
        int g = computeGUniversal( off0, tt, off );

        i64 minIndex = minT >> g;
        i64 maxIndex = 1 + ( maxT >> g );

        size_t buffCount = ( maxIndex - minIndex ) + 1;

        u64 block = _join32( u32( off0 ), u32( off[0] ) );

        auto ttTimes = _new<i64>( buffCount );
        auto blocks  = _new<u64>( buffCount );

        // this is the transition time for the current block, according to
        // `choose::latest`
        i64 when = tt[0];
        // this is the time when it is safe to advance to the next transition
        // time, when constructing blocks. it will sometimes be a little bit
        // after the current transition time, because we set the clocks back.
        i64 blockEndT = tt[0] + std::max( { off0, off[0], 0 } );

        i64 blockT = i64( u64( minIndex ) << g ); // current start time of block
        i64 blockSize = i64( 1 ) << g;            // Block size (in seconds)
        size_t zi     = 0;
        for( size_t bi = 0; bi < buffCount; bi++ )
        {
            ttTimes[bi]  = when;
            blocks[bi]   = block;
            blockT      += blockSize;
            if( blockT >= blockEndT )
            {
                zi++;
                if( zi < tt.size() )
                {
                    i32 off0  = off[zi - 1];
                    i32 off1  = off[zi];
                    when      = tt[zi];
                    blockEndT = tt[zi] + std::max( { off0, off1, 0 } );
                    block     = block >> 32 | ( u64( u32( off1 ) ) << 32 );
                }
                else
                {
                    ++bi;
                    for( ; bi < buffCount; ++bi )
                    {
                        ttTimes[bi] = when;
                        blocks[bi]  = block;
                    }
                    break;
                }
            }
        }

        return S32Table{
            g,
            std::move( ttTimes ),
            std::move( blocks ),
            minIndex,
            buffCount,
        };
    }


    OffTables makeOffTables( i32 off0,
        span<sysseconds_t const> tt,
        span<i32 const>          off,
        sec_t                    cycleTime ) {
        if( tt.size() == 0 )
        {
            // The zone is contains no transitions, so all of the tables are
            // valid for the full range of possible inputs.
            return {
                0,
                ~u64(),
                table1( off0 ),
                0, // The cycle time doesn't matter.
            };
        }

        if( tt.size() == 1 )
        {
            auto off1 = off[0];
            // The zone contains a single transition. All tables are valid for
            // the full range of possible inputs.
            return {
                0,
                ~u64(),
                table2( tt[0], off0, off1 ),
                0, // The cycle time doesn't matter
            };
        }

        // The min time is the min time under any time convention
        sec_t minT = std::min( {
            tt.front(),          // min time (UTC)
            tt.front() + off0,   // min time (initial local time)
            tt.front() + off[0], // min time (local time after first transition)
            sec_t( 0 ), // if min time would be smaller than 0, set to 0
        } );
        // Ensure that the cycle time is >= minT and >= 0
        cycleTime = std::max( { cycleTime, minT, sec_t( 0 ) } );
        // maxT is cycleTime + 400 years. cycleTime >= minT, so we know the
        // table is at least 400 years in size
        sec_t maxT = cycleTime + 12622780800;

        auto tz0_   = -u64( minT );
        auto tzMax_ = u64( maxT ) + tz0_;
        return {
            tz0_,
            tzMax_,
            makeUniversalTable( off0, tt, off, minT, maxT ),
            cycleTime,
        };
    }

    OffTables makeOffTables( ZoneStates const& s ) {
        return makeOffTables(
            s.walloffInitial_, s.walloffTrans_, s.walloff_, s.safeCycleTime );
    }

    AbbrTable makeAbbrTable( AbbrBlock initial,
        span<sysseconds_t const>       tt,
        span<AbbrBlock const>          abbr,
        sec_t                          cycleTime,
        span<ZoneAbbr const>           abbrTable ) {
        auto table = _copyUnique( abbrTable );
        // If the timezone contains no transitions, our table is valid for the
        // full range of possible inputs
        if( tt.size() == 0 )
        {
            return {
                0,
                ~u64(),
                table1( u32( initial ) ),
                0, // The cycle time doesn't matter
                std::move( table ),
            };
        }

        // If the timezone contains a single transition, our table is still
        // valid for the full range of possible inputs (although now we need
        // to include the fact that we have a transition)
        if( tt.size() == 1 )
        {
            return {
                0,
                ~u64(),
                table2( tt[0], u32( initial ), u32( abbr[0] ) ),
                0,
                std::move( table ),
            };
        }

        sec_t minT = std::min( {
            tt.front(), // min time (UTC)
            sec_t( 0 ),
        } );
        cycleTime  = std::max( { cycleTime, minT, sec_t( 0 ) } );
        sec_t maxT = cycleTime + 12622780800;

        auto tz0_   = -u64( minT );
        auto tzMax_ = u64( maxT ) + tz0_;

        return {
            tz0_,
            tzMax_,
            makeTable( initial, tt, abbr, minT, maxT ),
            cycleTime,
            std::move( table ),
        };
    }

    StdoffTable makeStdoffTable( ZoneStates const& states ) {
        span tt      = states.stdoffTrans_;
        span off     = states.stdoff_;
        i32  initial = states.stdoffInitial_;
        if( tt.size() == 0 )
        {
            return {
                std::numeric_limits<sysseconds_t>::min(),
                std::numeric_limits<sysseconds_t>::max(),
                table1( u32( initial ) ),
            };
        }
        if( tt.size() == 1 )
        {
            return {
                std::numeric_limits<sysseconds_t>::min(),
                std::numeric_limits<sysseconds_t>::max(),
                table2( tt[0], u32( initial ), u32( off[0] ) ),
            };
        }

        auto Tmin = tt.front() - 1;
        auto Tmax = tt.back();
        return { Tmin, Tmax, makeTable( initial, tt, off, Tmin, Tmax ) };
    }
    TimeZone::TimeZone( string_view name, ZoneStates const& states )
    : OffTables( makeOffTables( states ) )
    , AbbrTable( makeAbbrTable( states.abbrInitial_,
          states.abbrTrans_,
          states.abbr_,
          states.safeCycleTime,
          states.abbrTable_ ) )
    , StdoffTable( makeStdoffTable( states ) )
    , name_( name ) {}


    struct TZHolder {
        std::atomic<time_zone const*> tz;

        TZHolder( nullptr_t )
        : tz( nullptr ) {}

        TZHolder( std::unique_ptr<time_zone>&& tz ) noexcept
        : tz( tz.release() ) {}

        TZHolder( TZHolder&& rhs )
        : tz( rhs.tz.exchange(
              nullptr, std::memory_order::memory_order_relaxed ) ) {}


        time_zone const* atomicAssignZoneIfNull(
            std::unique_ptr<time_zone> tzptr ) {
            time_zone const* expected = nullptr;

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

        ~TZHolder() {
            auto* ptr = tz.load( std::memory_order::memory_order_relaxed );
            delete ptr;
        }
    };
    struct TimeZoneCache {
        TZDataFile file;
        using _table = map<string_view, TZHolder>;
        _table lookup;

        static _table getEmptyLookup( TZDataFile const& file ) {
            // TODO: handle links
            struct Entry {
                string_view name;
                operator _table::value_type() const {
                    return { name, nullptr };
                }
            };

            vector<Entry> entries;
            for( auto const& [key, value] : file.zones )
            {
                // construct the vector of entries
                entries.push_back( Entry{ key } );
            }

            // Convert to map
            return _table( entries.data(), entries.data() + entries.size() );
        }

        TimeZoneCache( TZDataFile&& file )
        : file( std::move( file ) )
        , lookup( getEmptyLookup( this->file ) ) {}

        std::unique_ptr<time_zone> load( string_view name ) {
            auto states = file.getZoneStates( name, 2600 );
            return std::unique_ptr<time_zone>( new TimeZone( name, states ) );
        }


        /// Locate a zone, loading it if necessary. Return a pointer to the
        /// loaded zone
        ///
        /// This function is thread safe, so that even if multiple threads
        /// try locating the same (or different) zones simultaneously,
        /// everything works out fine!
        time_zone const* locate_zone( string_view name ) {
            auto it = lookup.find( name );
            if( it == lookup.end() )
            {
                throw std::runtime_error( fmt::format(
                    "Unable to locate zone with name '{}'", name ) );
            }

            auto& entry = it->second;
            if( auto* ptr = entry.tz.load() )
            {
                // If this map entry already contained a time_zone pointer,
                // then we can just return that!
                return ptr;
            }
            else
            {
                // This occurs if the given zone has not yet been loaded.
                // We need to lead the zone, and then we atomically replace the
                // entry with the new zone.
                //
                // If another thread also attempted to
                // load the same zone at the same time, whichever thread
                // finishes loading first will fill in the entry
                return entry.atomicAssignZoneIfNull( load( name ) );
            }
        }
    };


    time_zone const* locate_zone( string_view name ) {
        struct Store {
            std::string   contents = readFile( "build/data/tzdata/tzdata.zi" );
            TimeZoneCache cache
                = parseTZData( contents, "build/data/tzdata/tzdata.zi" );
        };

        static Store store{};

        return store.cache.locate_zone( name );
    }

    string format_s(
        TimeZone const* tz, sysseconds_t t, std::string_view format ) {
        if( format.size() >= 128 )
            throw std::runtime_error(
                "formatTime(): input format string is too long" );

        auto gmtoff = tz->offset_s( t );
        auto stdoff = tz->stdoff_s( t );
        bool isDST  = gmtoff != stdoff;

        auto tLocal        = t + gmtoff;
        auto parts         = vtz::math::divFloor2<86400>( tLocal );
        auto date          = parts.quot;
        u32  timeIntraday  = parts.rem;
        int  hour          = timeIntraday / 3600;
        timeIntraday      %= 3600;
        int minute         = timeIntraday / 60;
        timeIntraday      %= 60;
        int  sec           = timeIntraday;
        auto ymd           = toCivil( date );

        auto abbrev = tz->abbrev_s( t );


        auto tmValue = std::tm{
            sec,
            minute,
            hour,
            ymd.day,
            ymd.month - 1, // the 'tm' struct uses 0-11 for the month
            // tm_year stores years since 1900
            ymd.year - 1900,
            // days since sunday - 0-6
            int( dowFromDays( date ) - DOW::Sun ),
            // days since January 1 – [​0​, 365]
            int( date - resolveCivil( ymd.year, 1, 1 ) ),
            // true if daylight savings time is in effect
            isDST,
        };

        // We need to inject the current timezone abbreviation into the format
        // string, since we do NOT want to rely on strftime's use of the
        // computer's local zone
        char fmtStr[2048];

        {
            char*  dest = fmtStr;
            size_t i    = 0;
            size_t len  = format.size();
            while( i < len )
            {
                char c = format[i++];
                if( c == '%' )
                {
                    char next = format[i];
                    // writes offset from UTC in the ISO 8601 format
                    if( next == 'z' )
                    {
                        dest += writeShortestOffset( stdoff, dest );
                        i    += 1;
                        continue;
                    }
                    // writes time zone name or abbreviation
                    else if( next == 'Z' )
                    {
                        memcpy( dest, abbrev.data(), abbrev.size() );
                        dest += abbrev.size();
                        i    += 1;
                        continue;
                    }
                }
                *dest++ = c;
            }
            *dest = '\0';
        }

        char   buffer[2048];
        size_t count
            = std::strftime( buffer, sizeof( buffer ), fmtStr, &tmValue );
        return std::string( buffer, count );
    }
} // namespace vtz
