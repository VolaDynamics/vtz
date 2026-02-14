#include <vtz/impl/bit.h>
#include <vtz/civil.h>
#include <vtz/files.h>
#include <vtz/impl/math.h>
#include <vtz/tz.h>
#include <vtz/tz_cache.h>
#include <vtz/tz_reader.h>

#include <algorithm>
#include <ctime>
#include <fmt/format.h>
#include <initializer_list>
#include <limits>

namespace vtz {
    template<class T>
    std::unique_ptr<T[]> _copy_unique( T const* data, size_t count ) {
        // We use memcpy here, so we expect the input to be trivially copyable
        static_assert( std::is_trivially_copyable_v<T> );

        T* buff = new T[count];
        memcpy( buff, data, count * sizeof( T ) );
        return std::unique_ptr<T[]>( buff );
    }

    template<class T>
    auto _copy_unique( span<T> values ) {
        return _copy_unique( values.data(), values.size() );
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
        return std::unique_ptr<T[]>{ new T[sizeof...(args)]{
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

    int compute_g( span<sysseconds_t const> tt ) {
        // If there are no transition times, or only one transition time, return
        // the maximum possible g
        if( tt.size() <= 1 ) return 63;
        u64 min_delta = UINT64_MAX;

        for( size_t i = 1; i < tt.size(); ++i )
        {
            auto const& t0 = tt[i - 1];
            auto const& t1 = tt[i];
            if( t1 <= t0 )
            {
                throw std::runtime_error(
                    "time_zone(): zone transitions were not ordered" );
            }
            auto delta = u64( t1 ) - u64( t0 );

            if( delta < min_delta ) min_delta = delta;
        }

        int g = _blog2( min_delta );

        if( g < 16 )
        {
            throw std::runtime_error( "Error: multiple transition times occur "
                                      "very close together (why?)" );
        }

        return g;
    }

    /// Compute the required block size, taking into account that two possible
    /// local times map to the same UTC time
    int compute_guniversal(
        i32 off0, span<sec_t const> tt, span<i32 const> off ) {
        // If there are no transition times, or only one transition time, return
        // the maximum possible g
        if( tt.size() <= 1 ) return 63;

        u64 min_delta = UINT64_MAX;

        // Latest possible interpretation of time of last block
        sec_t Tlast = tt[0] + std::max( { off0, off[1], 0 } );

        for( size_t i = 1; i < tt.size(); ++i )
        {
            // Transition time (UTC)
            auto T = tt[i];
            // Time when previous block ends (local time)
            auto local_end = T + off[i - 1];
            // Time when current block starts (local time)
            auto local_start = T + off[i];
            auto Tearly      = std::min( { T, local_end, local_start } );
            auto Tlate       = std::max( { T, local_end, local_start } );
            if( Tearly <= Tlast )
            {
                throw std::runtime_error(
                    "time_zone(): transition times overlap when converted to "
                    "local time" );
            }

            auto delta = u64( Tearly ) - u64( Tlast );
            Tlast      = Tlate;
            min_delta  = std::min( delta, min_delta );
        }

        int g = _blog2( min_delta );

        if( g < 16 )
        {
            throw std::runtime_error( "Error: multiple transition times occur "
                                      "very close together (why?)" );
        }

        return g;
    }


    template<class T, class Indexable>
    S32Table make_table( T       off0,
        span<sysseconds_t const> tt,
        Indexable                off,
        sysseconds_t             min_t,
        sysseconds_t             max_t ) {
        int g = compute_g( tt );

        i64 min_index = min_t >> g;
        i64 max_index = 1 + ( max_t >> g );

        size_t buff_count = ( max_index - min_index ) + 1;

        u64 block = _join32( u32( off0 ), u32( off[0] ) );

        auto tt_times = _new<i64>( buff_count );
        auto blocks   = _new<u64>( buff_count );

        i64 when = tt[0];

        i64 block_t
            = i64( u64( min_index ) << g ); // current start time of block
        i64    block_size = i64( 1 ) << g;  // Block size (in seconds)
        size_t zi     = 0;
        for( size_t bi = 0; bi < buff_count; bi++ )
        {
            tt_times[bi]  = when;
            blocks[bi]   = block;
            block_t      += block_size;
            if( block_t >= when )
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
                    for( ; bi < buff_count; ++bi )
                    {
                        tt_times[bi] = when;
                        blocks[bi]  = block;
                    }
                    break;
                }
            }
        }

        return S32Table{
            g,
            std::move( tt_times ),
            std::move( blocks ),
            min_index,
            buff_count,
        };
    }


    /// Construct a single table that can be used for both UTC and local
    /// lookups. Blocks are computed such that independent of whether you're
    /// looking up a UTC time, or a local time, you will end up in a block
    /// containing the relevant transition time.
    template<class T>
    S32Table make_universal_table( T off0,
        span<sysseconds_t const>     tt,
        span<T const>                off,
        sysseconds_t                 min_t,
        sysseconds_t                 max_t ) {
        int g = compute_guniversal( off0, tt, off );

        i64 min_index = min_t >> g;
        i64 max_index = 1 + ( max_t >> g );

        size_t buff_count = ( max_index - min_index ) + 1;

        u64 block = _join32( u32( off0 ), u32( off[0] ) );

        auto tt_times = _new<i64>( buff_count );
        auto blocks   = _new<u64>( buff_count );

        // this is the transition time for the current block, according to
        // `choose::latest`
        i64 when = tt[0];
        // this is the time when it is safe to advance to the next transition
        // time, when constructing blocks. it will sometimes be a little bit
        // after the current transition time, because we set the clocks back.
        i64 block_end_t = tt[0] + std::max( { off0, off[0], 0 } );

        i64 block_t
            = i64( u64( min_index ) << g ); // current start time of block
        i64    block_size = i64( 1 ) << g;  // Block size (in seconds)
        size_t zi     = 0;
        for( size_t bi = 0; bi < buff_count; bi++ )
        {
            tt_times[bi]  = when;
            blocks[bi]   = block;
            block_t      += block_size;
            if( block_t >= block_end_t )
            {
                zi++;
                if( zi < tt.size() )
                {
                    i32 off0  = off[zi - 1];
                    i32 off1  = off[zi];
                    when      = tt[zi];
                    block_end_t = tt[zi] + std::max( { off0, off1, 0 } );
                    block     = block >> 32 | ( u64( u32( off1 ) ) << 32 );
                }
                else
                {
                    ++bi;
                    for( ; bi < buff_count; ++bi )
                    {
                        tt_times[bi] = when;
                        blocks[bi]  = block;
                    }
                    break;
                }
            }
        }

        return S32Table{
            g,
            std::move( tt_times ),
            std::move( blocks ),
            min_index,
            buff_count,
        };
    }


    TransTable make_trans_table( span<sysseconds_t const> tt,

        sec_t        cycle_time,
        sysseconds_t min_trans_time = INT64_MIN,
        sysseconds_t max_trans_time = INT64_MAX ) {
        if( tt.size() == 0 )
        {
            return {
                0,
                ~u64(),
                table1( 0 ),
                cycle_time,
                _init<sysseconds_t>( min_trans_time, max_trans_time ),
            };
        }

        if( tt.size() == 1 )
        {
            return {
                0,
                ~u64(),
                // if we're before the transition time, use index of 0
                // (corresponding to when[0], when[1]),
                // otherwise use index of 1 (corresponding to when[1], when[2])
                table2( tt[0], 0, 1 ),
                0, // The cycle time doesn't matter
                _init<sysseconds_t>( min_trans_time, tt[0], max_trans_time ),
            };
        }

        // The min time is the min time under any time convention
        sec_t min_t = std::min( {
            tt.front(), // min time (UTC)
            sec_t( 0 ), // if min time would be smaller than 0, set to 0
        } );

        cycle_time  = std::max( { cycle_time, min_t, sec_t( 0 ) } );
        sec_t max_t = cycle_time + 12622780800;

        auto tz0_    = -u64( min_t );
        auto tz_max_ = u64( max_t ) + tz0_;

        struct GetIndex {
            i32 operator[]( ptrdiff_t i ) const noexcept { return i32( i + 1 ); };
        };

        auto times = _new<sysseconds_t>( tt.size() + 2 );
        times[0]   = min_trans_time;
        memcpy( times.get() + 1, tt.data(), tt.size_bytes() );
        times[tt.size() + 1] = max_trans_time;

        return {
            tz0_,
            tz_max_,
            make_table( 0, tt, GetIndex{}, min_t, max_t ),
            cycle_time,
            std::move( times ),
        };
    }


    OffTables make_off_tables( i32 off0,
        span<sysseconds_t const>   tt,
        span<i32 const>            off,
        sec_t                      cycle_time ) {
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
        sec_t min_t = std::min( {
            tt.front(),          // min time (UTC)
            tt.front() + off0,   // min time (initial local time)
            tt.front() + off[0], // min time (local time after first transition)
            sec_t( 0 ), // if min time would be smaller than 0, set to 0
        } );
        // Ensure that the cycle time is >= min_t and >= 0
        cycle_time = std::max( { cycle_time, min_t, sec_t( 0 ) } );
        // max_t is cycle_time + 400 years. cycle_time >= min_t, so we know the
        // table is at least 400 years in size
        sec_t max_t = cycle_time + 12622780800;

        auto tz0_    = -u64( min_t );
        auto tz_max_ = u64( max_t ) + tz0_;
        return {
            tz0_,
            tz_max_,
            make_universal_table( off0, tt, off, min_t, max_t ),
            cycle_time,
        };
    }

    OffTables make_off_tables( ZoneStates const& s ) {
        return make_off_tables( s.walloff_initial_,
            s.walloff_trans_,
            s.walloff_,
            s.safe_cycle_time );
    }

    AbbrTable make_abbr_table( AbbrBlock initial,
        span<sysseconds_t const>         tt,
        span<AbbrBlock const>            abbr,
        sec_t                            cycle_time,
        span<ZoneAbbr const>             abbr_table ) {
        auto table = _copy_unique( abbr_table );
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

        sec_t min_t = std::min( {
            tt.front(), // min time (UTC)
            sec_t( 0 ),
        } );
        cycle_time  = std::max( { cycle_time, min_t, sec_t( 0 ) } );
        sec_t max_t = cycle_time + 12622780800;

        auto tz0_    = -u64( min_t );
        auto tz_max_ = u64( max_t ) + tz0_;

        return {
            tz0_,
            tz_max_,
            make_table( initial, tt, abbr, min_t, max_t ),
            cycle_time,
            std::move( table ),
        };
    }

    StdoffTable make_stdoff_table( ZoneStates const& states ) {
        span tt      = states.stdoff_trans_;
        span off     = states.stdoff_;
        i32  initial = states.stdoff_initial_;
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
        return { Tmin, Tmax, make_table( initial, tt, off, Tmin, Tmax ) };
    }
    time_zone::time_zone( string_view name, ZoneStates const& states )
    : OffTables( make_off_tables( states ) )
    , AbbrTable( make_abbr_table( states.abbr_initial_,
          states.abbr_trans_,
          states.abbr_,
          states.safe_cycle_time,
          states.abbr_table_ ) )
    , StdoffTable( make_stdoff_table( states ) )
    , TransTable( make_trans_table( states.tt_, states.safe_cycle_time ) )
    , name_( name ) {}


    using TZHolder = AtomicEnt<time_zone>;


    time_zone time_zone::utc() {
        return time_zone{ "UTC",
            ZoneStates::make_static( ZoneState{
                FromUTC( 0 ),
                FromUTC( 0 ),
                ZoneAbbr{ 3, "UTC" },
            } ) };
    }


} // namespace vtz
