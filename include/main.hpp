#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstddef>
#include <print>
#include <utility>

extern std::string home;

extern std::mutex mtx;

template <typename... Args>
void locked_println(std::format_string<Args...> fmt, Args&&... args) {
    std::scoped_lock lock(mtx);
    std::println(fmt, std::forward<Args>(args)...);
}

bool prepare_paths(std::string_view package);

bool clone_repo_via_git(std::string_view url, std::string_view package);
bool download_file(std::string_view url);

bool unpack(std::string path);

void process_package(std::string_view package, std::size_t total_packages);

std::vector<std::string> read_list(std::string_view path);