#pragma once

#include "platform.hpp"

#include <string>
#include <vector>

struct Config {
    bool auto_confirm = false;
    bool keep_sources = false;
    bool use_system_package_manager = true;
    bool auto_update_check = false;

    int jobs = 0;

    std::string install_prefix = platform::default_install_prefix() + "/";
    std::string github_token = "";
    std::string gitlab_token = "";
    std::string home_path = "";

    std::vector<std::string> search_providers = {"github", "gitlab", "codeberg"};
};

extern Config config;

void load_cfg();