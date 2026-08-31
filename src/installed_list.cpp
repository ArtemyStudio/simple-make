#include "../include/installed_list.hpp"

#include <set>
#include <string>
#include <vector>
#include <filesystem>

namespace fs = std::filesystem;

std::set<std::string> snapshot_dir(const std::string path) {
    std::set<std::string> result;

    if (!fs::exists(path))
        return result;

    for (auto entry : fs::recursive_directory_iterator(path)) {
        if (entry.is_directory())
            continue;

        result.insert(entry.path().string());
    }

    return result;
}

std::vector<std::string> diff(const std::set<std::string>& old, const std::set<std::string>& new_) {
    std::vector<std::string> result;

    for (auto entry : new_) {
        if (old.find(entry) == old.end())
            result.push_back(entry);
    }

    return result;
}