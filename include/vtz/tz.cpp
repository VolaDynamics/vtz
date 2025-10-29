#include "vtz/civil.h"
#include <algorithm>
#include <initializer_list>
#include <vtz/tz.h>

#include <vtz/tz_reader.h>

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
                table1( -off0 ),
                table1( -off0 ),
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
                table2( tt[0] + off0, off0, off1 ),
                table2( tt[0] + off1, off0, off1 ),
                0, // The cycle time doesn't matter
            };
        }

        int g = computeG( tt );

        sysseconds_t minT = std::min( tt.front(), sysseconds_t( 0 ) );
        sysseconds_t maxT = cycleTime + 12622780800;

        i64 minIndex = minT >> g;
        i64 maxIndex = 1 + ( maxT >> g );

        size_t buffCount = ( maxIndex - minIndex ) + 1;

        auto ttTimes = _new<i64>( buffCount );
        auto blocks  = _new<u64>( buffCount );

        i32 off1  = off[0];
        u64 block = _join32( u32( off0 ), u32( off1 ) );

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
                    when     = tt[zi];
                    i32 off1 = off[zi];
                    block    = block >> 32 | ( u64( u32( off1 ) ) << 32 );
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

        auto tz0_   = -u64( minT );
        auto tzMax_ = u64( maxT ) + tz0_;
        return { tz0_,
            tzMax_,
            S32Table{
                g,
                std::move( ttTimes ),
                std::move( blocks ),
                minIndex,
                buffCount,
            },
            {},
            {},
            cycleTime };
    }

    OffTables makeOffTables( ZoneStates const& s ) {
        return makeOffTables(
            s.walloffInitial_, s.walloffTrans_, s.walloff_, s.safeCycleTime );
    }

    AbbrTable makeAbbrTable( ZoneStates const& s ) {
        // TODO
        return {};
    }

    StdoffTable makeStdoffTable( ZoneStates const& s ) {
        // TODO
        return {};
    }
    TimeZone::TimeZone( string_view name, ZoneStates const& states )
    : OffTables( makeOffTables( states ) )
    , AbbrTable( makeAbbrTable( states ) )
    , StdoffTable( makeStdoffTable( states ) )
    , name_( name ) {}
} // namespace vtz
