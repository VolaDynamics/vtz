#include <algorithm>
#include <vtz/tz.h>

#include <vtz/tz_reader.h>

namespace vtz {

    template<class T>
    using UP = std::unique_ptr<T>;


    /// Construct a lookup table corresponding to a constant state (eg, a
    /// timezone with no zone transitions)
    S32Table table1( u32 x ) {
        auto   block = _join32( x, x );
        size_t off   = 1;
        return S32Table{
            S32TableView{
                63,
                off + new i64[2]{ 0, 0 },
                off + new u64[2]{ block, block },
            },
            off,
        };
    }


    /// Construct a lookup table with two possible values and one transition
    /// time
    S32Table table2( i64 trans, u32 initial, u32 final ) {
        auto   block = _join32( initial, final );
        size_t off   = 1;
        return S32Table{
            S32TableView{
                63,
                off + new i64[2]{ trans, trans },
                off + new u64[2]{ block, block },
            },
            off,
        };
    }


    TimeZone::TimeZone( string name, ZoneStates const& states )
    : name_( std::move( name ) ) {
        auto const& tt = states.transitions;
        if( tt.empty() )
        {
            i32 walloff = states.initial.walloff.off;

            // Abbreviation table contains one entry, which is the initial
            // abbreviation
            abbrTable = UP<Abbr[]>( new Abbr[]{ states.initial.abbr } );
            // '0' points to the initial abbreviation here
            abbr = table1( 0 );

            utcOff           = table1( u32( walloff ) );
            localOffEarliest = table1( u32( -walloff ) );
            localOffLatest   = table1( u32( -walloff ) );

            // We don't have to worry about cycling - all inputs are valid
            tz0_      = 0;
            tzMax_    = ~u64();
            cycleTime = 0;
            return;
        }

        if( tt.size() == 1 )
        {
            ZoneState const& initial = states.initial;
            ZoneState const& final   = tt[0].state;
            i64              when    = tt[0].when;

            i32 off0 = initial.walloff.off;
            i32 off1 = final.walloff.off;

            abbrTable = UP<Abbr[]>( new Abbr[]{ initial.abbr, final.abbr } );
            abbr      = table2( when, 0, 1 );

            utcOff           = table2( when, off0, off1 );
            localOffEarliest = table2( when + off0, -off0, -off1 );
            localOffLatest   = table2( when + off1, -off0, -off1 );

            // We don't have to worry about cycling - all inputs are valid
            tz0_      = 0;
            tzMax_    = ~u64();
            cycleTime = 0;
            return;
        }


        u64 minDelta = UINT64_MAX;

        for( size_t i = 1; i < tt.size(); ++i )
        {
            auto const& t0 = tt[i - 1].when;
            auto const& t1 = tt[i].when;
            if( t1 <= t0 )
            {
                throw std::runtime_error(
                    fmt::format( "TimeZone(): zone transitions were not "
                                 "ordered (when instantiating {})",
                        name_ ) );
            }
            auto delta = u64( t1 ) - u64( t0 );

            if( delta < minDelta ) minDelta = delta;
        }

        int g = _blog2( minDelta );

        if( g < 16 )
        {
            throw std::runtime_error(
                fmt::format( "Error: multiple transition times occur very "
                             "close together (why?) zone={} minDelta={}s",
                    name_,
                    minDelta ) );
        }

        cycleTime = states.safeCycleTime;

        auto minT = std::min( tt.front().when, i64( 0 ) );
        auto maxT = cycleTime + 12622780800;

        i64 minIndex = minT >> g;
        i64 maxIndex = 1 + ( maxT >> g );

        size_t buffCount = ( maxIndex - minIndex ) + 1;


        i64* ttTimes = new i64[buffCount];
        u64* blocks  = new u64[buffCount];

        i32 off0  = states.initial.walloff.off;
        i32 off1  = tt[0].state.walloff.off;
        u64 block = _join32( u32( off0 ), u32( off1 ) );

        i64 when = tt[0].when;

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
                    when     = tt[zi].when;
                    i32 off1 = tt[zi].state.walloff.off;
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

        size_t off = size_t( -minIndex );
        utcOff     = S32Table{
            S32TableView{
                g,
                off + ttTimes,
                off + blocks,
            },
            off,
        };

        tz0_   = -u64( minT );
        tzMax_ = u64( maxT ) + tz0_;
    }
} // namespace vtz
