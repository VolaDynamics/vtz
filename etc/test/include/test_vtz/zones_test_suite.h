#pragma once

#include <gtest/gtest.h>
#include <string_view>
#include <test_vtz/zones.h>

class zones : public testing::TestWithParam<std::string_view> {};


constexpr auto pull_zone_name
    = []( testing::TestParamInfo<std::string_view> const& info ) {
          return esc_zone_name( info.param );
      };
