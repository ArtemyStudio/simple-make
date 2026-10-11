#include "../include/search.hpp"
#include "../include/parse_cfg.hpp"
#include "../include/main.hpp"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <format>
#include <string>

namespace {

std::string lower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
    return out;
}

void report_rate_limit(const cpr::Response& r, std::string_view provider, std::string_view pkg) {
    auto it = r.header.find("x-ratelimit-remaining");
    bool rate_limited = (r.status_code == 403 || r.status_code == 429) && (it == r.header.end() || it->second == "0");

    if (rate_limited) {
        if (provider == "github") {
            locked_println("\x1b[31m(failed) [{} api rate limit exceeded] package: {} -> set github_token in ~/.config/simple-make/simple-make.conf (sudo simple-make --setup-config) to go from 60 to 5000 requests/hour\x1b[0m", provider, pkg);
        } else {
            locked_println("\x1b[31m(failed) [{} api rate limit exceeded] package: {} -> try again shortly, or add a {}_token to the config\x1b[0m", provider, pkg, provider);
        }
    }
}

}

std::vector<Candidate> search_on_gh(std::string_view pkg, int limit) {
    std::string api_url = std::format("https://api.github.com/search/repositories?q={}+in:name&sort=stars&order=desc", pkg);

    cpr::Header headers = {{"User-Agent", "simple-make"}};
    if (!config.github_token.empty())
        headers.insert({"Authorization", std::format("token {}", config.github_token)});

    cpr::Response r = cpr::Get(cpr::Url{api_url}, cpr::Header{headers});

    if (r.status_code != 200) {
        report_rate_limit(r, "github", pkg);
        return {};
    }

    nlohmann::json data;
    try {
        data = nlohmann::json::parse(r.text);
    } catch (...) {
        return {};
    }

    if (!data.contains("items"))
        return {};

    std::vector<Candidate> result;
    for (auto& item : data["items"]) {
        if (static_cast<int>(result.size()) >= limit)
            break;
        result.push_back(Candidate{
            item.value("html_url", std::string{}),
            item.value("name", std::string{}),
            item.value("stargazers_count", 0L),
            "github"
        });
    }

    return result;
}

std::vector<Candidate> search_on_gitlab(std::string_view pkg, int limit) {
    std::string api_url = std::format("https://gitlab.com/api/v4/projects?search={}&order_by=star_count&sort=desc&per_page={}", pkg, limit);

    cpr::Header headers = {{"User-Agent", "simple-make"}};
    if (!config.gitlab_token.empty())
        headers.insert({"PRIVATE-TOKEN", config.gitlab_token});

    cpr::Response r = cpr::Get(cpr::Url{api_url}, cpr::Header{headers});

    if (r.status_code != 200) {
        report_rate_limit(r, "gitlab", pkg);
        return {};
    }

    nlohmann::json data;
    try {
        data = nlohmann::json::parse(r.text);
    } catch (...) {
        return {};
    }

    if (!data.is_array())
        return {};

    std::vector<Candidate> result;
    for (auto& item : data) {
        if (static_cast<int>(result.size()) >= limit)
            break;
        result.push_back(Candidate{
            item.value("web_url", std::string{}),
            item.value("name", std::string{}),
            item.value("star_count", 0L),
            "gitlab"
        });
    }

    return result;
}

std::vector<Candidate> search_on_codeberg(std::string_view pkg, int limit) {
    std::string api_url = std::format("https://codeberg.org/api/v1/repos/search?q={}&limit={}", pkg, limit);

    cpr::Response r = cpr::Get(cpr::Url{api_url}, cpr::Header{{"User-Agent", "simple-make"}});

    if (r.status_code != 200) {
        report_rate_limit(r, "codeberg", pkg);
        return {};
    }

    nlohmann::json data;
    try {
        data = nlohmann::json::parse(r.text);
    } catch (...) {
        return {};
    }

    if (!data.contains("data"))
        return {};

    std::vector<Candidate> result;
    for (auto& item : data["data"]) {
        if (static_cast<int>(result.size()) >= limit)
            break;
        result.push_back(Candidate{
            item.value("html_url", std::string{}),
            item.value("name", std::string{}),
            item.value("stars_count", 0L),
            "codeberg"
        });
    }

    return result;
}

std::vector<Candidate> search_all_sources(std::string_view pkg, int limit) {
    std::vector<Candidate> all;

    for (auto& provider : config.search_providers) {
        std::vector<Candidate> found;

        if (provider == "github")
            found = search_on_gh(pkg, limit);
        else if (provider == "gitlab")
            found = search_on_gitlab(pkg, limit);
        else if (provider == "codeberg")
            found = search_on_codeberg(pkg, limit);
        else
            continue;

        all.insert(all.end(), found.begin(), found.end());
    }

    return all;
}

std::optional<Candidate> pick_best_candidate(const std::vector<Candidate>& candidates, std::string_view pkg) {
    if (candidates.empty())
        return std::nullopt;

    std::string pkg_lower = lower(pkg);

    std::vector<const Candidate*> exact;
    for (auto& c : candidates) {
        if (lower(c.name) == pkg_lower)
            exact.push_back(&c);
    }
    
    if (exact.size() == 1)
        return *exact.front();
    if (exact.size() > 1)
        return std::nullopt;

    const Candidate* best = &candidates.front();
    bool tied = false;

    for (auto& c : candidates) {
        if (c.stars > best->stars) {
            best = &c;
            tied = false;
        } else if (c.stars == best->stars && &c != best) {
            tied = true;
        }
    }

    if (tied)
        return std::nullopt;

    return *best;
}
