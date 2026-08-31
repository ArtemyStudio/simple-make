#include "../include/search.hpp"
#include "../include/parse_cfg.hpp"
#include "../include/main.hpp"

#include "cpr/api.h"
#include "cpr/cprtypes.h"
#include "cpr/response.h"

#include <expected>
#include <print>
#include <mutex>
#include <format>
#include <string>
#include <iostream>
#include <vector>
#include <cpr/cpr.h>
#include <string_view>
#include <nlohmann/json.hpp>

Result search_on_gh(std::string_view pkg, bool just_search,  int print_urls) {
    std::string api_url = std::format("https://api.github.com/search/repositories?q={}&sort=stars&order=desc", pkg);
    
    cpr::Header headers = {{"User-Agent", "simple-make"}};

    if (!config.github_token.empty())
        headers.insert({"Authorization", std::format("token {}", config.github_token)});

    cpr::Response r = cpr::Get(cpr::Url{api_url}, cpr::Header{headers});

    if (r.status_code != 200) {
        return std::unexpected(std::vector<std::string>{});
    }

    nlohmann::json data = nlohmann::json::parse(r.text);

    if (!data.contains("items") || data["items"].empty()) {
        return std::unexpected(std::vector<std::string>{});
    }

    auto& items = data["items"];

    std::scoped_lock lock(mtx);

    if (just_search) {
        int n = std::min(print_urls, static_cast<int>(items.size()));
        std::vector<std::string> result;
        for (int i = 0; i < n; i++) {
            result.push_back(std::format("{} ({}★)", items[i]["html_url"].get<std::string>(), items[i]["stargazers_count"].get<int>()));
        }
        
        return std::unexpected(result);
    }

    std::println("\x1b[32m███████╗ ██████╗ ██╗   ██╗███╗   ██╗██████╗ ██╗\x1b[0m");
    std::println("\x1b[32m██╔════╝██╔═══██╗██║   ██║████╗  ██║██╔══██╗██║\x1b[0m");
    std::println("\x1b[32m█████╗  ██║   ██║██║   ██║██╔██╗ ██║██║  ██║██║\x1b[0m");
    std::println("\x1b[32m██╔══╝  ██║   ██║██║   ██║██║╚██╗██║██║  ██║╚═╝\x1b[0m");
    std::println("\x1b[32m██║     ╚██████╔╝╚██████╔╝██║ ╚████║██████╔╝██╗\x1b[0m");
    std::println("\x1b[32m╚═╝      ╚═════╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝ ╚═╝\n\x1b[0m");

    if (data["total_count"].get<int>() > 1) {
        std::println("[{}] found more than one, sorted by stars. Choose:", pkg);

        for (int i = 0; i < items.size(); i++) {
            std::println("{}. {} ({}★)", i+1, items[i]["html_url"].get<std::string>(), items[i]["stargazers_count"].get<int>());
        }

        std::print("> ");
        std::string input;
        std::getline(std::cin, input);

        try {
            int choice = std::stoi(input) - 1;

            if (choice >= 0 && choice < static_cast<int>(items.size())) {
                return items[choice]["html_url"].get<std::string>();
            } else {
                std::println("Invalid choice!");
                return std::unexpected(std::vector<std::string>{});
            }
        } catch (...) {
            std::println("i think you'd better enter a valid number");
            return std::unexpected(std::vector<std::string>{});
        }
    }

    return data["items"][0]["html_url"].get<std::string>();
}
