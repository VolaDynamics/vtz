#pragma once

#include <string>
#include <string_view>

struct path_settings {
    std::string tzdata   = "build/data/tzdata";
    std::string zoneinfo = "build/data/zoneinfo";
    std::string testdata = "etc/testdata";
};

extern path_settings paths;

std::string _join_fp( std::string_view prefix, std::string_view file );
