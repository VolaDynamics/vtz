#pragma once

#include <string>
#include <vtz/tz_reader.h>
#include <vtz/tz_reader/RuleOn.h>
#include <vtz/tz_reader/RuleSave.h>
#include <vtz/tz_reader/ZoneUntil.h>


namespace vtz {
    using std::string;

    string format_as( ZoneRule const& rule );
    string format_as( RuleTrans const& rule );
    string format_as( RuleOn r );
    string format_as( ZoneFormat const& f );
    string format_as( RuleAt r );
    string format_as( ZoneUntil );
    string format_as( RuleSave );
    string format_as( FromUTC off );
    string format_as( AbbrBlock b );
} // namespace vtz
