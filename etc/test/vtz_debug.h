#pragma once


#include "vtz_testing.h"


#include <vtz/tz_reader/ZoneTransition.h>
#include <vtz/math.h>
#include <vtz/tz.h>
#include <fmt/chrono.h>
#include <vtz/tz_reader.h>

DECLARE_STRINGLIKE( vtz::ZoneAbbr );

STRUCT_INFO( vtz::sys_info,
    FIELD( vtz::sys_info, begin ),
    FIELD( vtz::sys_info, end ),
    FIELD( vtz::sys_info, offset ),
    FIELD( vtz::sys_info, save ),
    FIELD( vtz::sys_info, abbrev ) );

STRUCT_INFO( vtz::local_info,
    FIELD( vtz::local_info, result ),
    FIELD( vtz::local_info, first ),
    FIELD( vtz::local_info, second ) );

STRUCT_INFO( vtz::ZoneState,
    FIELD( vtz::ZoneState, stdoff ),
    FIELD( vtz::ZoneState, walloff ),
    FIELD( vtz::ZoneState, abbr ) );


STRUCT_INFO( vtz::Location, FIELD( vtz::Location, line ), FIELD( vtz::Location, col ) );

STRUCT_INFO( vtz::RuleEntry,
    FIELD( vtz::RuleEntry, from ),
    FIELD( vtz::RuleEntry, to ),
    FIELD( vtz::RuleEntry, in ),
    FIELD( vtz::RuleEntry, on ),
    FIELD( vtz::RuleEntry, at ),
    FIELD( vtz::RuleEntry, save ),
    FIELD( vtz::RuleEntry, letter ) );

STRUCT_INFO( vtz::ZoneEntry,
    FIELD( vtz::ZoneEntry, stdoff ),
    FIELD( vtz::ZoneEntry, rules ),
    FIELD( vtz::ZoneEntry, format ),
    FIELD( vtz::ZoneEntry, until ) );

STRUCT_INFO( vtz::Zone, FIELD( vtz::Zone, name ), FIELD( vtz::Zone, ents ) );

STRUCT_INFO( vtz::Link, FIELD( vtz::Link, canonical ), FIELD( vtz::Link, alias ) );

STRUCT_INFO( vtz::TZDataFile,
    FIELD( vtz::TZDataFile, rules ),
    FIELD( vtz::TZDataFile, zones ),
    FIELD( vtz::TZDataFile, links ) );


STRUCT_INFO( vtz::math::div_t<int>,
    FIELD( vtz::math::div_t<int>, quot ),
    FIELD( vtz::math::div_t<int>, rem ) );

STRUCT_INFO( vtz::YMD, FIELD( vtz::YMD, year ), FIELD( vtz::YMD, month ), FIELD( vtz::YMD, day ) );
STRUCT_INFO( vtz::year_doy, FIELD( vtz::year_doy, year ), FIELD( vtz::year_doy, doy ) );
