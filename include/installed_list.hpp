#pragma once

#include <set>
#include <string>
#include <vector>

std::set<std::string> snapshot_dir(const std::string path);
std::vector<std::string> diff(const std::set<std::string>& old, const std::set<std::string>& new_);