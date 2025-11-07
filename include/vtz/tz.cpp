#include <vtz/bit.h>
#include <vtz/civil.h>
#include <vtz/files.h>
#include <vtz/math.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>

#include <algorithm>
#include <atomic>
#include <charconv>
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

    class FormatError : public std::exception {
        char const* what_;

      public:

        FormatError( char const* what ) noexcept
        : what_( what ) {}

        char const* what() const noexcept override { return what_; }
    };

    FormatError bufferTooSmall() noexcept {
        return FormatError( "format error: destination buffer is too small" );
    }

    // Write a year, assuming the year is positive and fits in 4 digits, and
    // the bugger is big enough
    VTZ_INLINE static char* _writeYear4( char* p, i64 year ) noexcept {
        u16  y      = u16( year );
        auto y1000  = y / 1000;
        y          %= 1000;
        auto y100   = y / 100;
        y          %= 100;
        auto y10    = y / 10;
        y          %= 10;
        auto y1     = y;
        p[0]        = '0' + y1000;
        p[1]        = '0' + y100;
        p[2]        = '0' + y10;
        p[3]        = '0' + y1;
        return p + 4;
    }
    /// Writes the year. Assumes there are at least 22 characters of space in
    /// the buffer
    VTZ_INLINE static char* _writeYear( char* p, i64 year ) noexcept {
        // if the year is nonnegative and fits in 4 digits, write as 4-digit
        // year
        if( u64( year ) < 10000 ) [[likely]]
            return _writeYear4( p, year );
        // fallback to std::to_chars
        return std::to_chars( p, p + 22, year ).ptr;
    }

    /// Writes the last 2 digits of the year, as a decimal number
    VTZ_INLINE static char* _writeYearShort( char* p, i64 year ) noexcept {
        u8 x = u8( std::abs( year ) % 100 );
        p[0] = '0' + x / 10;
        p[1] = '0' + x % 10;
        return p + 2;
    }

    /// %m specifier - write the month as a decimal in the range [01, 12]
    VTZ_INLINE static char* _writeMon( char* p, u8 mon ) noexcept {
        p[0] = '0' + ( mon >= 10 );
        p[1] = '0' + ( mon % 10 );
        return p + 2;
    }

    /// writes day of the month as a decimal number (range [01,31])
    VTZ_INLINE static char* _writeDOM_d( char* p, u8 day ) noexcept {
        char c0 = '0' + day / 10;
        char c1 = '0' + day % 10;
        p[0]    = c0;
        p[1]    = c1;
        return p + 2;
    }

    /// writes day of the month as a decimal number (range [1,31]).
    /// Single digit is preceded by a space.
    VTZ_INLINE static char* _writeDOM_e( char* p, u8 day ) noexcept {
        char c0 = '0' + day / 10;
        char c1 = '0' + day % 10;
        if( day < 10 ) c0 = ' ';
        p[0] = c0;
        p[1] = c1;
        return p + 2;
    }

    /// equivalent to "%H:%M"
    VTZ_INLINE static char* _writeISO_HHMM( char* p, int h, int m ) noexcept {
        p[0] = '0' + h / 10;
        p[1] = '0' + h % 10;
        p[2] = ':';
        p[3] = '0' + m / 10;
        p[4] = '0' + m % 10;
        return p + 5;
    }

    /// equivalent to "%H%M%S" (the ISO 8601 time format)
    VTZ_INLINE static char* _writeHHMMSScompact(
        char* p, int h, int m, int s ) noexcept {
        p[0] = '0' + h / 10;
        p[1] = '0' + h % 10;
        p[2] = '0' + m / 10;
        p[3] = '0' + m % 10;
        p[4] = '0' + s / 10;
        p[5] = '0' + s % 10;
        return p + 6;
    }

    /// equivalent to "%H:%M:%S" (the ISO 8601 time format)
    VTZ_INLINE static char* _writeHHMMSS(
        char* p, int h, int m, int s ) noexcept {
        p[0] = '0' + h / 10;
        p[1] = '0' + h % 10;
        p[2] = ':';
        p[3] = '0' + m / 10;
        p[4] = '0' + m % 10;
        p[5] = ':';
        p[6] = '0' + s / 10;
        p[7] = '0' + s % 10;
        return p + 8;
    }
    /// Write a 2-digit int. Assumes value is < 100
    VTZ_INLINE static char* _write2Digit( char* p, u8 x ) noexcept {
        p[0] = '0' + x / 10;
        p[1] = '0' + x % 10;
        return p + 2;
    }
    /// Write a 3-digit int. Assumes value is < 1000
    VTZ_INLINE static char* _write3Digit( char* p, u16 x ) noexcept {
        p[0]  = '0' + x / 100;
        x    %= 100;
        p[1]  = '0' + x / 10;
        p[2]  = '0' + x % 10;
        return p + 3;
    }

    /// equivalent to "%Y-%m-%d" (the ISO 8601 date format)
    /// Assumes there is at least 28 characters of space in the buffer
    /// (21 characters needed for excessively large negative/positive year)
    VTZ_INLINE static char* _writeISO_date(
        char* p, i64 y, int m, int d ) noexcept {
        if( u64( y ) < 10000 ) [[likely]]
        {
            p    = _writeYear4( p, y );
            *p++ = '-';
            p    = _writeMon( p, m );
            *p++ = '-';
            p    = _writeDOM_d( p, d );
            return p;
        }
        p    = _writeYear( p, y );
        *p++ = '-';
        p    = _writeMon( p, m );
        *p++ = '-';
        p    = _writeDOM_d( p, d );
        return p;
    }

    template<class F, class WriteFractional>
    auto _do_strformat( TimeZone const& tz,
        char const*                     format,
        size_t                          formatSize,
        sysseconds_t                    t,
        WriteFractional                 writeFrac,
        F                               func ) {
        constexpr size_t MAX_SPECIFIERS = 64;
        constexpr size_t MAX_SIZE       = MAX_SPECIFIERS * 2;
        // In the worst case, the year is 13 bytes. (this occurs if, eg, the
        // input is INT64_MIN) in which case the year would be around
        // -292471208678 - this gives the length:
        //
        //     len(str(-2**63 // (365 * 86400)))
        //
        // The longest expansion is %F, which expands to %Y-%m-%d, so (13+6), so
        // 19 characters in length
        constexpr size_t REQUIRED_BUFF_SIZE = 19 * MAX_SPECIFIERS + 1;
        // Make buffer size a power of 2
        constexpr size_t BUFF_SIZE
            = 1ull << ( 1 + _blog2_fallback( REQUIRED_BUFF_SIZE ) );

        static_assert( BUFF_SIZE >= REQUIRED_BUFF_SIZE );

        if( formatSize >= MAX_SIZE )
            throw FormatError(
                "formatTime(): input format string is too long" );

        if( formatSize == 0 ) return func( "", 0 );

        auto gmtoff        = tz.offset_s( t );
        auto tLocal        = t + gmtoff;
        auto parts         = vtz::math::divFloor2<86400>( tLocal );
        auto date          = parts.quot;
        u32  timeIntraday  = parts.rem;
        int  hr            = timeIntraday / 3600;
        timeIntraday      %= 3600;
        int mi             = timeIntraday / 60;
        timeIntraday      %= 60;
        int  sec           = timeIntraday;
        auto ymd           = toCivil( date );

        // We're going to handle as many format specifiers as we can ourselves,
        // so that we can avoid a call to strftime.
        //
        // How big do we need to make this buffer?
        // Well, we know there are at most (formatSize/2) specifiers.
        // This means a max of 64 specifiers.
        char fmtStr[BUFF_SIZE];

        char*  p = fmtStr;
        size_t i = 0;
        // Iterate until 1 before the end. This ensures we don't do an
        // out-of-bounds read on a format specifier
        size_t len           = formatSize - 1;
        bool   needsStrftime = false;
        while( i < len )
        {
            char c = format[i++];
            if( c != '%' )
            {
                // Not a format specifier? Copy the character verbatim.
                *p++ = c;
                continue;
            }
            // We've encountered a format specifier, so we consume an
            // additional character
            char next = format[i++];
            switch( next )
            {
            // writes literal %. The full conversion specification must be %%.
            case '%': *p++ = '%'; continue;
            // writes newline character
            case 'n': *p++ = '\n'; continue;
            // writes horizontal tab character
            case 't': *p++ = '\t'; continue;
            // writes year as a decimal number, e.g. 2017
            case 'Y': p = _writeYear( p, ymd.year ); continue;
            // writes last 2 digits of year as a decimal number (range
            // [00,99])
            case 'y': p = _writeYearShort( p, ymd.year ); continue;
            // writes month as a decimal number (range [01,12])
            case 'm': p = _writeMon( p, ymd.month ); continue;
            // 	writes day of the month as a decimal number (range
            // [01,31])
            case 'd': p = _writeDOM_d( p, ymd.day ); continue;
            // writes day of the year as a decimal number (range [001,366])
            case 'j':
                {
                    u16 yday = date - resolveCivil( ymd.year, 1, 1 );

                    p = _write3Digit( p, 1 + yday );
                    continue;
                }
            // writes weekday as a decimal number, where Sunday is 0 (range
            // [0-6])
            case 'w':
                {
                    static_assert( int( DOW::Sun ) == 0 );
                    auto dow = int( dowFromDays( date ) );
                    *p++     = '0' + dow;
                    continue;
                }
            // 	writes weekday as a decimal number, where Monday is 1 (ISO 8601
            // format) (range [1-7])
            case 'u':
                {
                    auto dow = int( dowFromDays( date ) );
                    if( dow == 0 ) dow = 7;
                    *p++ = '0' + dow;
                    continue;
                }
            // writes hour as a decimal number, 12 hour clock (range [01,12])
            case 'I':
                {
                    int x = hr % 12;
                    if( x == 0 ) x = 12;
                    p = _write2Digit( p, u8( x ) );
                    continue;
                }
            // writes day of the month as a decimal number (range [1,31]).
            // Single digit is preceded by a space.
            case 'e': p = _writeDOM_e( p, ymd.day ); continue;
            // equivalent to "%Y-%m-%d" (the ISO 8601 date format)
            case 'F':
                p = _writeISO_date( p, ymd.year, ymd.month, ymd.day );
                continue;
            // equivalent to "%H:%M"
            case 'R': p = _writeISO_HHMM( p, hr, mi ); continue;
            // 	writes hour as a decimal number, 24 hour clock (range
            // [00-23])
            case 'H': p = _write2Digit( p, hr ); continue;
            // writes minute as a decimal number (range [00,59])
            case 'M': p = _write2Digit( p, mi ); continue;
            // writes second as a decimal number (range [00,60]). Will also
            // write a fractional component after the second.
            case 'S': p = writeFrac( _write2Digit( p, sec ) ); continue;
            // equivalent to "%H:%M:%S" (the ISO 8601 time format)
            case 'T': p = writeFrac( _writeHHMMSS( p, hr, mi, sec ) ); continue;
            // writes offset from UTC in the ISO 8601 format (e.g. -0430)
            case 'z': p += writeShortestOffset( gmtoff, p ); continue;
            case 'Z': p += tz.abbrev_to_s( t, p ); continue;
            default:
                {
                    *p++          = c;
                    *p++          = next;
                    needsStrftime = true;
                    continue;
                }
            }
        }
        // If the format string did not end in a format specifier, make sure
        // to copy the final character.
        if( i == len ) { *p++ = format[i]; }

        if( !needsStrftime ) [[likely]]
        {
            // If we were able to handle all the specifiers, there's nothing
            // left to format. This means we don't need to do a call to
            // strftime, and we can exit early!
            return func( fmtStr, size_t( p - fmtStr ) );
        }

        // Add null terminator to format string
        *p = '\0';

        auto stdoff = tz.stdoff_s( t );
        bool isDST  = gmtoff != stdoff;

        auto tmValue = std::tm{
            sec,
            mi,
            hr,
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

        // Fill in any remaining format specifiers with strftime.
        // We will use this if there are any locale-dependent format specifiers
        // (asize from %Z)

        char   buffer[BUFF_SIZE];
        size_t count
            = std::strftime( buffer, sizeof( buffer ), fmtStr, &tmValue );
        return func( buffer, count );
    }

    size_t TimeZone::format_to_s(
        string_view format, sysseconds_t t, char* buff, size_t count ) const {
        return _do_strformat(
            *this,
            format.data(),
            format.size(),
            t,
            // There is no fractional component, so writeFrac is a noop
            []( char* s ) noexcept -> char* { return s; },
            [buff, count]( char const* s, size_t size ) {
                // clamp size
                if( size > count ) size = count;
                // write to buffer
                _vtz_memcpy( buff, s, size );
                // return size
                return size;
            } );
    }

    auto _writeNanos( u32 nanos, int precision ) noexcept {
        return [nanos, precision]( char* p ) noexcept -> char* {
            // clang-format off
            u32 n = nanos;
            int prec = precision;
            if( prec == 0 ) return p;
            p[0] = '.';
            p[1] = '0' + n / 100000000; n %= 100000000;
            if( prec == 1 ) return p + 2;
            p[2] = '0' + n / 10000000;  n %= 10000000;
            if( prec == 2 ) return p + 3;
            p[3] = '0' + n / 1000000;   n %= 1000000;
            if( prec == 3 ) return p + 4;
            p[4] = '0' + n / 100000;    n %= 100000;
            if( prec == 4 ) return p + 5;
            p[5] = '0' + n / 10000;     n %= 10000;
            if( prec == 5 ) return p + 6;
            p[6] = '0' + n / 1000;      n %= 1000;
            if( prec == 6 ) return p + 7;
            p[7] = '0' + n / 100;       n %= 100;
            if( prec == 7 ) return p + 8;
            p[8] = '0' + n / 10;        n %= 10;
            if( prec == 8 ) return p + 9;
            p[9] = '0' + n;
            return p + 10;
            // clang-format on
        };
    }

    size_t TimeZone::format_precise_to_s( string_view format,
        sysseconds_t                                  t,
        u32                                           nanos,
        int                                           precision,
        char*                                         buff,
        size_t                                        count ) const {
        return _do_strformat( *this,
            format.data(),
            format.size(),
            t,
            _writeNanos( nanos, precision ),
            [buff, count]( char const* s, size_t size ) {
                // clamp size
                if( size > count ) size = count;
                // write to buffer
                _vtz_memcpy( buff, s, size );
                // return size
                return size;
            } );
    }

    std::string TimeZone::format_precise_s(
        string_view format, sysseconds_t t, u32 nanos, int precision ) const {
        return _do_strformat( *this,
            format.data(),
            format.size(),
            t,
            _writeNanos( nanos, precision ),
            []( char const* s, size_t size ) { return string( s, size ); } );
    }

    string TimeZone::format_s( std::string_view format, sysseconds_t t ) const {
        return _do_strformat(
            *this,
            format.data(),
            format.size(),
            t,
            // There is no fractional component, so writeFrac is a noop
            []( char* s ) noexcept -> char* { return s; },
            []( char const* s, size_t size ) { return string( s, size ); } );
    }

    template<bool isSane, bool isCompact, class FWriteAbbrev, class FAction>
    auto _format_local_to( sysseconds_t tLocal,
        char* const                     p,
        char                            dateSep,
        char                            dateTimeSep,
        FWriteAbbrev                    writeAbbrev,
        FAction action ) noexcept( noexcept( action( p, writeAbbrev( p ) ) ) ) {
        // t = utc_t + gmtoff;
        auto parts         = vtz::math::divFloor2<86400>( tLocal );
        auto date          = parts.quot;
        u32  timeIntraday  = parts.rem;
        int  hr            = timeIntraday / 3600;
        timeIntraday      %= 3600;
        int mi             = timeIntraday / 60;
        timeIntraday      %= 60;
        int  sec           = timeIntraday;
        auto ymd           = toCivil( date );

        if constexpr( isSane )
        {
            // the timestamp itself is 15 characters (if compact), otherwise 19
            // characters
            constexpr size_t stampEnd = isCompact ? 15 : 19;
            if constexpr( isCompact )
            {
                // We were promised that the year is 0000-9999
                (void)_writeYear4( p, ymd.year );                // 0..4
                (void)_writeMon( p + 4, ymd.month );             // 4..6
                (void)_writeDOM_d( p + 6, ymd.day );             // 6..8
                p[8] = dateTimeSep;                              // 8..9
                (void)_writeHHMMSScompact( p + 9, hr, mi, sec ); // 9..15
            }
            else
            {
                // YYYY-MM-DD HH:MM:SS
                // 0123456789012345678
                (void)_writeYear4( p, ymd.year );          // 0..4
                p[4] = dateSep;                            // 4..5
                (void)_writeMon( p + 5, ymd.month );       // 5..7
                p[7] = dateSep;                            // 7..8
                (void)_writeDOM_d( p + 8, ymd.day );       // 8..10
                p[10] = dateTimeSep;                       // 10..11
                (void)_writeHHMMSS( p + 11, hr, mi, sec ); // 11..19
            }

            return action( p, stampEnd + writeAbbrev( p + stampEnd ) );
        }
        else
        {
            // the timestamp is longer than expected. Write the year first,
            // and then write the remainder after the year
            char*  q       = _writeYear( p, ymd.year );
            size_t yearLen = q - p;

            constexpr size_t stampEnd = isCompact ? 11 : 15;
            if constexpr( isCompact )
            {
                // MMDD HHMMSS
                // 01234567890
                (void)_writeMon( q, ymd.month );                 // 0..2
                (void)_writeDOM_d( q + 2, ymd.day );             // 2..4
                q[4] = dateTimeSep;                              // 4..5
                (void)_writeHHMMSScompact( q + 5, hr, mi, sec ); // 5..11
            }
            else
            {
                // -MM-DD HH:MM:SS
                // 012345678901234
                q[0] = dateSep;                           // 0..1
                (void)_writeMon( q + 1, ymd.month );      // 1..3
                q[3] = dateSep;                           // 3..4
                (void)_writeDOM_d( q + 4, ymd.day );      // 4..6
                q[6] = dateTimeSep;                       // 6..7
                (void)_writeHHMMSS( q + 7, hr, mi, sec ); // 7..14
            }

            return action(
                p, yearLen + stampEnd + writeAbbrev( q + stampEnd ) );
        }
    }

    template<bool includeAbbrSep, bool useStdoff>
    static auto _writeAbbrev(
        TimeZone const& tz, sysseconds_t t, i32 gmtoff, char abbrevSep ) {
        if constexpr( useStdoff )
        {
            if constexpr( includeAbbrSep )
            {
                return [gmtoff, abbrevSep]( char* p ) noexcept {
                    *p = abbrevSep;
                    return 1 + writeShortestOffset( gmtoff, p + 1 );
                };
            }
            else
            {
                return [gmtoff]( char* p ) noexcept {
                    return writeShortestOffset( gmtoff, p );
                };
            }
        }
        else
        {
            if constexpr( includeAbbrSep )
            {
                return [&tz, t, abbrevSep]( char* p ) noexcept {
                    *p = abbrevSep;
                    return 1 + tz.abbrev_to_s( t, p + 1 );
                };
            }
            else
            {
                return [&tz, t]( char* p ) noexcept {
                    return tz.abbrev_to_s( t, p );
                };
            }
        }
    }

    template<bool isCompact, bool useStdoff, bool includeAbbrSep>
    VTZ_INLINE static std::string _do_format( TimeZone const& tz,
        sysseconds_t                                          t,
        char                                                  dateSep,
        char                                                  dateTimeSep,
        char                                                  abbrSep ) {
        constexpr u64 LONG_TIME = resolveCivilTime( 10000, 1, 1, 0, 0, 0 );

        auto off    = tz.offset_s( t );
        auto localT = t + off;

        char buff[64];

        if( u64( localT ) < LONG_TIME ) [[likely]]
        {
            return _format_local_to<true, isCompact>( localT,
                buff,
                dateSep,
                dateTimeSep,
                _writeAbbrev<includeAbbrSep, useStdoff>( tz, t, off, abbrSep ),
                []( char* p, size_t s ) { return std::string( p, s ); } );
        }
        else
        {
            return _format_local_to<false, isCompact>( localT,
                buff,
                dateSep,
                dateTimeSep,
                _writeAbbrev<includeAbbrSep, useStdoff>( tz, t, off, abbrSep ),
                []( char* p, size_t s ) { return std::string( p, s ); } );
        }
    }

    /// Fast formatting - format as:
    ///
    ///     `YYYY<dateSep>MM<dateSep>DD<dateTimeSep>HH:MM:SS<abbrSep>[Abbrev]
    ///
    /// The various flags control if the separators actually appear

    template<bool isCompact, bool useStdoff, bool includeAbbrSep>
    VTZ_INLINE static size_t _do_format_to( TimeZone const& tz,
        sysseconds_t                                        t,
        char*                                               p,
        size_t                                              count,
        char                                                dateSep,
        char                                                dateTimeSep,
        char                                                abbrSep ) noexcept {
        ///
        constexpr u64 LONG_TIME = resolveCivilTime( 10000, 1, 1, 0, 0, 0 );

        auto gmtoff = tz.offset_s( t );
        auto localT = t + gmtoff;
        if( u64( localT ) < LONG_TIME ) [[likely]]
        {
            // Required size is (number of characters for timestamp) + (1 for
            // abbr sep) + (max abbr length)
            constexpr size_t REQUIRED_SIZE
                = ( isCompact ? 15 : 19 ) + includeAbbrSep + 7;
            // If we're _before_ LONG_TIME, we know the year is 0000-9999
            // So the length is at most, eg,
            // 9999-12-31T23:59:59 LongAbbr
            if( count >= REQUIRED_SIZE ) [[likely]]
            {
                // If the buffer is big enough, we can write to the buffer
                // directly
                return _format_local_to<true, isCompact>( localT,
                    p,
                    dateSep,
                    dateTimeSep,
                    _writeAbbrev<includeAbbrSep, useStdoff>(
                        tz, t, gmtoff, abbrSep ),
                    []( char*, size_t s ) noexcept { return s; } );
            }
            else
            {
                static_assert( REQUIRED_SIZE < 32 );
                char buff[32];
                return _format_local_to<true, isCompact>( localT,
                    buff,
                    dateSep,
                    dateTimeSep,
                    _writeAbbrev<includeAbbrSep, useStdoff>(
                        tz, t, gmtoff, abbrSep ),
                    [p, count]( char const* p2, size_t s ) noexcept {
                        if( s > count ) s = count;
                        _vtz_memcpy( p, p2, s );
                        return s;
                    } );
            }
        }
        else
        {
            // Required size is (number of characters for timestamp) + (1 for
            // abbr sep) + (max abbr length).
            //
            // Year may be up to 13 characters, given by:
            //
            //     len(str(-2**63 // (365 * 86400)))
            constexpr size_t REQUIRED_SIZE
                = 13 + ( isCompact ? 11 : 15 ) + includeAbbrSep + 7;
            if( count >= REQUIRED_SIZE )
            {
                // If the buffer is big enough, we can write to the buffer
                // directly
                return _format_local_to<false, isCompact>( localT,
                    p,
                    dateSep,
                    dateTimeSep,
                    _writeAbbrev<includeAbbrSep, useStdoff>(
                        tz, t, gmtoff, abbrSep ),
                    []( char*, size_t s ) noexcept { return s; } );
            }
            else
            {
                static_assert( REQUIRED_SIZE < 64 );
                char buff[64];
                return _format_local_to<false, isCompact>( localT,
                    buff,
                    dateSep,
                    dateTimeSep,
                    _writeAbbrev<includeAbbrSep, useStdoff>(
                        tz, t, gmtoff, abbrSep ),
                    [p, count]( char const* p2, size_t s ) noexcept {
                        if( s > count ) s = count;
                        _vtz_memcpy( p, p2, s );
                        return s;
                    } );
            }
        }
    }

    // clang-format off
    string TimeZone::format_iso8601_s( sysseconds_t t, char dateSep, char dateTimeSep ) const {
        return _do_format<false, true, false>( *this, t, dateSep, dateTimeSep, '\0' );
    }

    size_t TimeZone::format_iso8601_to_s( sysseconds_t t, char* buff, size_t count, char dateSep, char dateTimeSep ) const noexcept {
        return _do_format_to<false, true, false>( *this, t, buff, count, dateSep, dateTimeSep, '\0' );
    }

    string TimeZone::format_s( sysseconds_t t, char dateSep, char dateTimeSep, char abbrevSep ) const {
        return _do_format<false, false, true>( *this, t, dateSep, dateTimeSep, abbrevSep );
    }

    size_t TimeZone::format_to_s( sysseconds_t t, char* buff, size_t count, char dateSep, char dateTimeSep, char abbrevSep ) const noexcept {
        return _do_format_to<false, false, true>( *this, t, buff, count, dateSep, dateTimeSep, abbrevSep );
    }

    string TimeZone::format_compact_s( sysseconds_t t, char dateTimeSep, char abbrevSep ) const {
        return _do_format<true, false, true>( *this, t, '\0', dateTimeSep, abbrevSep );
    }

    size_t TimeZone::format_compact_to_s( sysseconds_t t, char* p, size_t count, char dateTimeSep, char abbrevSep ) const noexcept {
        return _do_format_to<true, false, true>( *this, t, p, count, '\0', dateTimeSep, abbrevSep );
    }
    // clang-format on
} // namespace vtz
