#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct Candidate {
    std::string url;
    std::string name;
    long stars = 0;
    std::string source;
};

std::vector<Candidate> search_on_gh(std::string_view pkg, int limit = 10);
std::vector<Candidate> search_on_gitlab(std::string_view pkg, int limit = 10);
std::vector<Candidate> search_on_codeberg(std::string_view pkg, int limit = 10);
std::vector<Candidate> search_all_sources(std::string_view pkg, int limit = 10);
std::optional<Candidate> pick_best_candidate(const std::vector<Candidate>& candidates, std::string_view pkg);
