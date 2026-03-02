#pragma once

#include "bench_common.h"

#include <absl/time/time.h>

inline vector<absl::Time> to_absl_time( vector<i64> const& tt ) {
    vector<absl::Time> result( tt.size() );
    for( size_t i = 0; i < tt.size(); ++i )
        result[i] = absl::FromUnixSeconds( tt[i] );
    return result;
}


inline vector<absl::CivilSecond> to_absl_civil( vector<i64> const& tt ) {
    auto                      utc = absl::UTCTimeZone();
    vector<absl::CivilSecond> result( tt.size() );
    for( size_t i = 0; i < tt.size(); ++i )
        result[i] = utc.At( absl::FromUnixSeconds( tt[i] ) ).cs;
    return result;
}
