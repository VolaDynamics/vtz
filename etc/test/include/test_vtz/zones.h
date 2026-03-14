#pragma once
#include <string_view>
#include <vector>

auto get_test_zones() -> std::vector<std::string_view> const&;

auto esc_zone_name( std::string_view zone_name ) -> std::string;
