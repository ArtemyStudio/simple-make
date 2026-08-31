#pragma once

#include <expected>
#include <vector>
#include <string>

using Result = std::expected<std::string, std::vector<std::string>>;

Result search_on_gh(std::string_view pkg, bool just_search = false,  int print_urls = 30);