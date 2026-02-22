#pragma once

#include <string>
#include <vtz/tz_reader.h>
#include <vtz/tz_reader/rule_on.h>
#include <vtz/tz_reader/zone_save.h>
#include <vtz/tz_reader/zone_until.h>


namespace vtz {
    using std::string;

    string format_as( zone_rule const& rule );
    string format_as( RuleTrans const& rule );
    string format_as( rule_on r );
    string format_as( zone_format const& f );
    string format_as( rule_at r );
    string format_as( zone_until );
    string format_as( zone_save );
    string format_as( from_utc off );
    string format_as( AbbrBlock b );
} // namespace vtz
