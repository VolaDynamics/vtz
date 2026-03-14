#pragma once


#include "vtz_testing.h"


#include <fmt/chrono.h>
#include <vtz/impl/math.h>
#include <vtz/tz.h>
#include <vtz/tz_reader.h>
#include <vtz/tz_reader/zone_transition.h>

DECLARE_STRINGLIKE( vtz::zone_abbr );

STRUCT_INFO(
    vtz::sys_info, FIELD( begin ), FIELD( end ), FIELD( offset ), FIELD( save ), FIELD( abbrev ) );

STRUCT_INFO( vtz::local_info, FIELD( result ), FIELD( first ), FIELD( second ) );

STRUCT_INFO( vtz::zone_state, FIELD( stdoff ), FIELD( walloff ), FIELD( abbr ) );


STRUCT_INFO( vtz::location, FIELD( line ), FIELD( col ) );

STRUCT_INFO( vtz::rule_entry,
             FIELD( from ),
             FIELD( to ),
             FIELD( in ),
             FIELD( on ),
             FIELD( at ),
             FIELD( save ),
             FIELD( letter ) );

STRUCT_INFO( vtz::zone_entry, FIELD( stdoff ), FIELD( rules ), FIELD( format ), FIELD( until ) );

STRUCT_INFO( vtz::zone, FIELD( name ), FIELD( ents ) );

STRUCT_INFO( vtz::zone_link, FIELD( canonical ), FIELD( alias ) );

STRUCT_INFO( vtz::tz_data_file, FIELD( rules ), FIELD( zones ), FIELD( links ) );


STRUCT_INFO( vtz::math::div_t<int>, FIELD( quot ), FIELD( rem ) );

STRUCT_INFO( vtz::civil_ymd, FIELD( year ), FIELD( month ), FIELD( day ) );
STRUCT_INFO( vtz::year_doy, FIELD( year ), FIELD( doy ) );
