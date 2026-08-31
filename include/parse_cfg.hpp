#pragma once

#include <string>

struct Config {
    bool auto_confirm = false;
    bool keep_sources = false;

    int jobs = 0;

    std::string install_prefix = "/usr/local/";
    std::string github_token = "";
    std::string home_path = "";
};

extern Config config;

void load_cfg();