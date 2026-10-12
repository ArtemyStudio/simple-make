#pragma once

#include <string>
#include <cstddef>
#include <string_view>

enum class Builders {
    Cmake,
    Meson,
    Ninja,
    Make,
    Autotools,
    Rust,
    Go,
    Python,
    Java,
    Unknown
};

bool build_pkg(std::string path, std::size_t total_packages, std::string_view package);
bool do_continue();