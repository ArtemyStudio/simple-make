#include "../include/parse_cfg.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>

Config config;

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return {};

    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

void load_cfg() {
    const char* tmp = std::getenv("SUDO_USER");
    std::string home = tmp ? std::format("/home/{}", tmp) : "/root";

    std::string cfg_path = std::format("{}/.config/simple-make/simple-make.conf", home);

    std::error_code err;
    if (!std::filesystem::exists(cfg_path, err) || err)
        return;

    std::ifstream cfg_file(cfg_path);
    if (!cfg_file.is_open())
        return;

    std::string line;

    while (std::getline(cfg_file, line)) {
        line = trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        size_t pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));

        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
            value = value.substr(1, value.size() - 2);

        if (key == "auto_confirm") {
            config.auto_confirm = (value == "true" || value == "1");
        } else if (key == "keep_sources") {
            config.keep_sources = (value == "true" || value == "1");
        } else if (key == "jobs") {
            try { config.jobs = std::stoi(value); } catch (...) {}
        } else if (key == "install_prefix") {
            config.install_prefix = value;
        } else if (key == "github_token") {
            config.github_token = value;
        } else if (key == "home_path") {
            config.home_path = value;
        }
    }
}