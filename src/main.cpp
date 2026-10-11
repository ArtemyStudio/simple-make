#include "../include/search.hpp"
#include "../include/main.hpp"
#include "../include/db.hpp"
#include "../include/build.hpp"
#include "../include/parse_cfg.hpp"
#include "../include/platform.hpp"

#include "cpr/callback.h"

#include <print>
#include <thread>
#include <mutex>
#include <vector>
#include <format>
#include <string>
#include <cstdlib>
#include <fstream>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <cctype>
#include <mutex>
#include <semaphore>
#include <sstream>

namespace fs = std::filesystem;

std::string home;
std::mutex mtx;

namespace {
    std::counting_semaphore<> build_slots{ static_cast<std::ptrdiff_t>(std::max(1u, std::thread::hardware_concurrency())) };

    constexpr int kHelpWidth = 78;

    std::vector<std::string> wrap_text(const std::string& text, int width) {
        std::vector<std::string> lines;
        std::string current;
        std::istringstream words(text);
        std::string word;

        while (words >> word) {
            std::string candidate = current.empty() ? word : current + " " + word;
            if (static_cast<int>(candidate.size()) > width) {
                if (!current.empty())
                    lines.push_back(current);
                current = word;
            } else {
                current = candidate;
            }
        }

        if (!current.empty() || lines.empty())
            lines.push_back(current);

        return lines;
    }

    void print_box(std::string_view heading, const std::vector<std::string>& lines) {
        int inner = kHelpWidth - 6;

        std::string border;
        for (int i = 0; i < kHelpWidth; i++)
            border += "█";

        auto row = [&](const std::string& text) {
            std::string content = text.substr(0, inner);
            content += std::string(inner - content.size(), ' ');
            std::println("██ {} ██", content);
        };

        std::println("{}", border);
        row("");
        row(std::string(heading));
        row("");

        for (auto& l : lines) {
            if (static_cast<int>(l.size()) <= inner) {
                row(l);
            } else {
                for (auto& wrapped : wrap_text(l, inner))
                    row(wrapped);
            }
        }

        row("");
        std::println("{}", border);
        std::println("");
    }

    class SlotGuard {
    public:
        explicit SlotGuard(std::counting_semaphore<>& sem) : sem_(sem) { sem_.acquire(); }
        ~SlotGuard() { sem_.release(); }
        SlotGuard(const SlotGuard&) = delete;
        SlotGuard& operator=(const SlotGuard&) = delete;
    private:
        std::counting_semaphore<>& sem_;
    };
}

int main(int argc, char* argv[]) {
    platform::enable_ansi_colors();

    if (platform::requires_elevation() && !platform::is_elevated()) {
        std::println("\x1b[31m(failed) [not elevated] run this as root (Linux/BSD: sudo; Windows: an elevated/Administrator terminal). macOS doesn't need this.\x1b[0m");
        return 1;
    }

    auto args = std::span(argv, argc).subspan(1);
    if (args.empty()) {
        std::println("usage: sudo simple-make <package-name> [package-name...]\n");
        return 1;
    }

    load_cfg();

    home = config.home_path.empty() ? platform::resolve_home_dir() : config.home_path;

    if (std::string_view(args[0]) == "--help" || std::string_view(args[0]) == "-h") {
        std::println("\nsimple-make — modern package downloader & builder (C++23)\n");

        print_box("usage:", {
            "simple-make <package-name>         search, download and build a package by name",
            "simple-make <github-url>           download and build a package directly from a GitHub URL",
            "simple-make <url.com/pkg.tar.gz>   download, extract and build a package from any URL",
        });

        print_box("options:", {
            "-h, --help                    print this help and exit",
            "-r, --remove, --delete, -d    remove installed packages",
            "-s, --search                  search for a package across all configured sources",
            "-u, --update [package...]     rebuild installed packages (all of them, if none given)",
            "--self-update                 check simple-make's own releases and rebuild itself if newer",
            "--setup-config                interactive config wizard",
        });

        print_box("examples:", {
            "simple-make peaclock       (fast local database match)",
            "simple-make yq             (live search across GitHub/GitLab/Codeberg)",
            "simple-make ripgrep        (tries your system package manager first)",
            "simple-make https://github.com/ArtemyStudio/simple-make                     (from a GitHub URL)",
            "simple-make https://openbox.org/dist/openbox/openbox-3.6.1.tar.xz           (from a tarball URL)",
        });

        print_box("dependencies:", {
            "version control:  git",
            "",
            "compilers & build systems:",
            "  c/c++        -> make, cmake, meson, ninja, gcc/g++, autoconf/automake",
            "  rust         -> cargo, rustc",
            "  go           -> go",
            "  python       -> pip",
            "  java         -> mvn or gradle, plus native-image or javac",
            "",
            "archivers & extractors:",
            "  tar/gz/xz/bz2/zst -> tar, gzip, xz, bzip2, zstd",
            "  zip / 7z / rar    -> unzip, 7z, unrar",
            "",
            "system package managers (tried before building from source):",
            "  apt-get, dnf, pacman, zypper, apk (Linux) / brew (macOS) / pkg (BSD) / winget, choco (Windows)",
        });

        print_box("config (~/.config/simple-make/simple-make.conf):", {
            "[bool]   auto_confirm                 automatically approve build prompts",
            "[bool]   keep_sources                 keep sources after installation",
            "[int]    jobs                          cores to use for compilation (0 = auto)",
            "[string] home_path                     override the detected home directory",
            "[string] install_prefix                where built packages are installed",
            "[string] github_token                  raises GitHub's limit from 60 to 5000 req/hour",
            "[string] gitlab_token                  raises GitLab's anonymous API rate limit",
            "[bool]   use_system_package_manager    try apt/dnf/pacman/brew/pkg/winget/choco first",
            "[bool]   auto_update_check             check simple-make's own releases on startup",
            "[list]   search_providers              comma-separated: github, gitlab, codeberg",
            "",
            "example:",
            "  github_token = \"ghp_x87yA2b3C4d5E6f7G8h9I0j1K2l3M4n5O6p7\"  # not a real token",
            "  keep_sources = true",
            "  install_prefix = \"/usr/local\"",
            "  search_providers = \"github, gitlab\"",
        });

        print_box("paths:", {
            "config:         ~/.config/simple-make/simple-make.conf",
            "sources:        ~/.simple-make/sources",
            "installed_list: ~/.simple-make/installed",
            "compressed:     ~/.simple-make/compressed",
            "logs:           ~/.simple-make/logs",
        });

        return 0;
    } else if (std::string_view(args[0]) == "--remove" || std::string_view(args[0]) == "-r" || std::string_view(args[0]) == "--delete" || std::string_view(args[0]) == "-d") {
        std::vector<std::string_view> packages(args.begin() + 1, args.end());
        std::vector<std::string> deleting;
        std::vector<std::string> read_list_vector;

        for (auto& package : packages) {
            read_list_vector = read_list(std::format("{}/.simple-make/installed/{}.list", home, package));
            if (read_list_vector.empty()) {
                {
                    std::scoped_lock lock(mtx);
                    std::println("\x1b[31m███╗   ██╗ ██████╗ ████████╗    ███████╗ ██████╗ ██╗   ██╗███╗   ██╗██████╗ \x1b[0m");
                    std::println("\x1b[31m████╗  ██║██╔═══██╗╚══██╔══╝    ██╔════╝██╔═══██╗██║   ██║████╗  ██║██╔══██╗\x1b[0m");
                    std::println("\x1b[31m██╔██╗ ██║██║   ██║   ██║       █████╗  ██║   ██║██║   ██║██╔██╗ ██║██║  ██║\x1b[0m");
                    std::println("\x1b[31m██║╚██╗██║██║   ██║   ██║       ██╔══╝  ██║   ██║██║   ██║██║╚██╗██║██║  ██║\x1b[0m");
                    std::println("\x1b[31m██║ ╚████║╚██████╔╝   ██║       ██║     ╚██████╔╝╚██████╔╝██║ ╚████║██████╔╝\x1b[0m");
                    std::println("\x1b[31m╚═╝  ╚═══╝ ╚═════╝    ╚═╝       ╚═╝      ╚═════╝  ╚═════╝ ╚═╝  ╚═══╝╚═════╝ \x1b[0m");
                }

                std::println("\x1b[31m(failed) [cannot find package] package: {}\x1b[0m", package);
                return 1;
            }

            for (auto& entry : read_list_vector) {
                if (entry != "" && entry != " ")
                    deleting.push_back(entry);
            }
        }

        if (deleting.empty()) {
            std::println("\x1b[31m(failed) [nothing to delete] package\x1b[0m");
            return 1;
        }

        for (auto& entry : deleting) {
            fs::remove(entry);
        }

        for (auto& package : packages) {
            fs::remove(std::format("{}/.simple-make/installed/{}.list", home, package));
            fs::remove(std::format("{}/.simple-make/installed/{}.url", home, package));
        }

        {
            std::scoped_lock lock(mtx);
            std::println("\x1b[32m██████╗  ██████╗ ███╗   ██╗███████╗       ██╗   \x1b[0m");
            std::println("\x1b[32m██╔══██╗██╔═══██╗████╗  ██║██╔════╝    ██╗╚██╗  \x1b[0m");
            std::println("\x1b[32m██║  ██║██║   ██║██╔██╗ ██║█████╗      ╚═╝ ██║  \x1b[0m");
            std::println("\x1b[32m██║  ██║██║   ██║██║╚██╗██║██╔══╝      ██╗ ██║  \x1b[0m");
            std::println("\x1b[32m██████╔╝╚██████╔╝██║ ╚████║███████╗    ╚═╝██╔╝  \x1b[0m");
            std::println("\x1b[32m╚═════╝  ╚═════╝ ╚═╝  ╚═══╝╚══════╝       ╚═╝ \n\x1b[0m");
        }

        return 0;
    } else if (std::string_view(args[0]) == "--setup-config") {
        {
            std::scoped_lock lock(mtx);
            std::println(" ██████╗ ██████╗ ███╗   ██╗███████╗");
            std::println("██╔════╝██╔═══██╗████╗  ██║██╔════╝");
            std::println("██║     ██║   ██║██╔██╗ ██║█████╗  ");
            std::println("██║     ██║   ██║██║╚██╗██║██╔══╝  ");
            std::println("╚██████╗╚██████╔╝██║ ╚████║██║     ");
            std::println(" ╚═════╝ ╚═════╝ ╚═╝  ╚═══╝╚═╝     \n");
        }

        std::error_code dir_err;
        fs::create_directories(std::format("{}/.config/simple-make", home), dir_err);

        std::ofstream cfg_file(std::format("{}/.config/simple-make/simple-make.conf", home));
        if (!cfg_file.is_open()) {
            std::println("\x1b[31m(failed) [cannot open config file for writing]\x1b[0m");
            return 1;
        }

        std::print("do you want to set auto confirm? (y/N) >");
        std::string auto_confirm_input;
        std::getline(std::cin, auto_confirm_input);

        if (auto_confirm_input == "y" || auto_confirm_input == "Y")
            cfg_file << "auto_confirm = true\n";
        else
            cfg_file << "auto_confirm = false\n";
        std::println("");

        std::print("do you want to set keep sources? (y/N) >");
        std::string keep_sources_input;
        std::getline(std::cin, keep_sources_input);

        if (keep_sources_input == "y" || keep_sources_input == "Y")
            cfg_file << "keep_sources = true\n";
        else
            cfg_file << "keep_sources = false\n";
        std::println("");

        while (1) {
            std::print("how many cores will be used for compilation? (default: auto) >");
            std::string jobs_input;
            std::getline(std::cin, jobs_input);

            if (jobs_input.empty()) {
                config.jobs = 0;
                break;
            }

            try {
                int num = std::stoi(jobs_input);
                if (num > 0)
                    cfg_file << "jobs = " + std::to_string(num) + "\n";
                else
                    break;
                break;
            } catch (...) {
                std::println("i think you'd better enter a valid number\n");
            }
        }
        std::println("");

        while (1) {
            std::print("install prefix? (default: /usr/local/) >");
            std::string install_prefix_input;
            std::getline(std::cin, install_prefix_input);

            if (install_prefix_input.empty()) {
                break;
            } else if (install_prefix_input.contains('/')) {
                cfg_file << "install_prefix = \"" + install_prefix_input + "\"\n";
                break;
            } else {
                std::println("i think you'd better enter a valid path\n");
            }
        }
        std::println("");

        while (1) {
            std::print("github token? (default: empty) >");
            std::string github_token_input;
            std::getline(std::cin, github_token_input);

            if (github_token_input.empty()) {
                break;
            }

            cpr::Response r = cpr::Get(cpr::Url{"https://api.github.com/user"}, cpr::Header{{"Authorization", std::format("token {}", github_token_input)}, {"User-Agent", "simple-make"}});
            if (r.status_code == 200) {
                cfg_file << "github_token = \"" + github_token_input + "\"\n";
                break;
            }

            std::println("github token is isn't working. please check it again.\n");
        }

        std::println("");

        std::print("gitlab token, for raising GitLab's anonymous search rate limit? (default: empty) >");
        std::string gitlab_token_input;
        std::getline(std::cin, gitlab_token_input);
        if (!gitlab_token_input.empty())
            cfg_file << "gitlab_token = \"" + gitlab_token_input + "\"\n";
        std::println("");

        std::print("try the system package manager (apt/dnf/pacman/brew/pkg/winget/choco) before building from source? (Y/n) >");
        std::string use_pkg_mgr_input;
        std::getline(std::cin, use_pkg_mgr_input);
        cfg_file << "use_system_package_manager = " << ((use_pkg_mgr_input == "n" || use_pkg_mgr_input == "N") ? "false" : "true") << "\n";
        std::println("");

        std::print("check simple-make's own releases for updates on startup? (y/N) >");
        std::string auto_update_input;
        std::getline(std::cin, auto_update_input);
        cfg_file << "auto_update_check = " << ((auto_update_input == "y" || auto_update_input == "Y") ? "true" : "false") << "\n";
        std::println("");

        while (1) {
            std::print("what is your user's home path? (default: auto) >");
            std::string home_path_input;
            std::getline(std::cin, home_path_input);

            if (home_path_input.empty()) {
                break;
            } else if (home_path_input.contains('/')) {
                cfg_file << "home_path = \"" + home_path_input + "\"\n";
                break;
            } else {
                std::println("i think you'd better enter a valid path\n");
            }
        }

        std::println("");

        print_box("saved:", {
            std::format("auto_confirm = {}", config.auto_confirm ? "true" : "false"),
            std::format("keep_sources = {}", config.keep_sources ? "true" : "false"),
            std::format("jobs = {}", config.jobs == 0 ? "auto" : std::to_string(config.jobs)),
            std::format("install_prefix = {}", config.install_prefix),
            std::format("github_token = {}", config.github_token.empty() ? "empty" : config.github_token),
            std::format("gitlab_token = {}", config.gitlab_token.empty() ? "empty" : config.gitlab_token),
            std::format("use_system_package_manager = {}", config.use_system_package_manager ? "true" : "false"),
            std::format("auto_update_check = {}", config.auto_update_check ? "true" : "false"),
            std::format("home_path = {}", config.home_path.empty() ? "/root" : config.home_path),
        });

        cfg_file.close();
        platform::fix_ownership(std::format("{}/.config/simple-make", home));
        return 0;
    } else if (std::string_view(args[0]) == "-s" || std::string_view(args[0]) == "--search") {
        std::vector<std::string_view> packages(args.begin() + 1, args.end());

        for (auto& package : packages) {
            auto candidates = search_all_sources(package, 3);

            bool installed = fs::exists(std::format("{}/.simple-make/installed/{}.list", home, package));

            if (candidates.empty()) {
                locked_println("\x1b[31m(failed) [no links found] package: {}\x1b[0m", package);
            } else {
                locked_println("\x1b[32m(finished) [done] package: {}{}\x1b[0m", package, installed ? " (installed)" : "");
                for (const auto& c : candidates) {
                    locked_println("  -> \x1b[34m{}\x1b[0m ({}★, {})", c.url, c.stars, c.source);
                }
            }
            std::println("");
        }

        return 0;
    } else if (std::string_view(args[0]) == "-u" || std::string_view(args[0]) == "--update") {
        std::vector<std::string> packages(args.begin() + 1, args.end());

        if (packages.empty()) {
            std::string installed_dir = std::format("{}/.simple-make/installed", home);
            std::error_code ec;
            if (fs::exists(installed_dir, ec)) {
                for (auto& entry : fs::directory_iterator(installed_dir, ec)) {
                    if (entry.path().extension() == ".list")
                        packages.push_back(entry.path().stem().string());
                }
            }
        }

        if (packages.empty()) {
            std::println("\x1b[31m(failed) [nothing installed to update]\x1b[0m");
            return 1;
        }

        std::vector<std::string> targets;
        targets.reserve(packages.size());

        for (auto& pkg : packages) {
            std::string url_path = std::format("{}/.simple-make/installed/{}.url", home, pkg);
            std::ifstream url_file(url_path);
            std::string saved_url;
            if (url_file.is_open() && std::getline(url_file, saved_url) && !saved_url.empty()) {
                targets.push_back(saved_url);
            } else {
                locked_println("(note) [{}] no recorded source, re-resolving by name", pkg);
                targets.push_back(pkg);
            }
        }

        std::vector<std::thread> update_runners;
        update_runners.reserve(targets.size());
        for (auto& target : targets)
            update_runners.emplace_back(process_package, std::string_view(target), targets.size());
        for (auto& runner : update_runners)
            runner.join();

        return 0;
    } else if (std::string_view(args[0]) == "--self-update") {
        cpr::Response r = cpr::Get(cpr::Url{"https://api.github.com/repos/ArtemyStudio/simple-make/releases/latest"}, cpr::Header{{"User-Agent", "simple-make"}});

        if (r.status_code != 200) {
            std::println("\x1b[31m(failed) [cannot check for updates]\x1b[0m");
            return 1;
        }

        auto data = nlohmann::json::parse(r.text, nullptr, false);
        std::string latest = data.is_discarded() ? "" : data.value("tag_name", std::string{});
        if (!latest.empty() && latest.front() == 'v')
            latest.erase(0, 1);

        if (latest.empty() || latest == SIMPLE_MAKE_VERSION) {
            std::println("simple-make is already up to date (v{})", SIMPLE_MAKE_VERSION);
            return 0;
        }

        std::println("a newer version is available: v{} -> v{}", SIMPLE_MAKE_VERSION, latest);
        if (!do_continue())
            return 0;

        process_package("https://github.com/ArtemyStudio/simple-make", 1);
        return 0;
    }



    std::vector<std::thread> runners;
    runners.reserve(args.size());

    for (char* arg : args) {
        runners.emplace_back(process_package, std::string_view(arg), args.size());
    }

    for (auto& runner : runners) {
        runner.join();
    }

    {
        std::scoped_lock lock(mtx);
        std::println("\x1b[32m██████╗  ██████╗ ███╗   ██╗███████╗       ██╗   \x1b[0m");
        std::println("\x1b[32m██╔══██╗██╔═══██╗████╗  ██║██╔════╝    ██╗╚██╗  \x1b[0m");
        std::println("\x1b[32m██║  ██║██║   ██║██╔██╗ ██║█████╗      ╚═╝ ██║  \x1b[0m");
        std::println("\x1b[32m██║  ██║██║   ██║██║╚██╗██║██╔══╝      ██╗ ██║  \x1b[0m");
        std::println("\x1b[32m██████╔╝╚██████╔╝██║ ╚████║███████╗    ╚═╝██╔╝  \x1b[0m");
        std::println("\x1b[32m╚═════╝  ╚═════╝ ╚═╝  ╚═══╝╚══════╝       ╚═╝ \n\x1b[0m");
    }
}

bool prepare_paths(std::string_view package) {
    try {
        if (home.empty()) {
            locked_println("\x1b[31mhome path is not found!\x1b[0m");
            return false;
        }

        if (!fs::exists(home + "/.simple-make")) {
            fs::create_directory(home + "/.simple-make");
            fs::create_directory(home + "/.simple-make/sources");
            fs::create_directory(home + "/.simple-make/installed");
            fs::create_directory(home + "/.simple-make/logs");
        } else {
            if (!fs::exists(home + "/.simple-make/sources")) 
                fs::create_directory(home +"/.simple-make/sources");
            if (!fs::exists(home + "/.simple-make/logs")) 
                fs::create_directory(home +"/.simple-make/logs");
            if (!fs::exists(home + "/.simple-make/installed"))
                fs::create_directory(home +"/.simple-make/installed");
        }
        std::string pkg_path = std::format("{}/.simple-make/sources/{}", home, package);
        if (!fs::exists(pkg_path)) {
            fs::create_directory(pkg_path);
        } else {
            fs::remove_all(pkg_path);
            fs::create_directory(pkg_path);
        }
    } catch (...) { return false; }

    platform::fix_ownership(std::format("{}/.simple-make", home));

    return true;
}

bool clone_repo_via_git(std::string_view url, std::string_view package) {
    std::string cmd = std::format("git clone --quiet {} {}/.simple-make/sources/{}", url, home, package);

    int status = system(cmd.c_str());

    if (status != 0) {
        return false;
    }

    return true;
}

std::string get_filename_from_url(std::string_view url) {
    std::string_view raw_name;
    size_t slash_pos = url.rfind('/');
    
    if (slash_pos == std::string_view::npos)
        raw_name = url;
    else
        raw_name = url.substr(slash_pos + 1);

    std::string final_name;
    
    for (char c : raw_name) {
        if (c == '.') {
            break;
        }

        final_name += c;
    }

    return final_name;
}

std::string get_filename_and_extension_from_url(std::string_view url) {
    std::string name;
    size_t slash_pos = url.rfind('/');
    
    if (slash_pos == std::string_view::npos)
        name = url;
    else
        name = url.substr(slash_pos + 1);

    return name;
}

bool download_file(std::string_view url) {
    std::string name = get_filename_from_url(url);
    std::string full_name = get_filename_and_extension_from_url(url);

    {
        std::scoped_lock lock(mtx);
        std::println("██╗███╗   ██╗███████╗████████╗ █████╗ ██╗     ██╗     ██╗███╗   ██╗ ██████╗            ");
        std::println("██║████╗  ██║██╔════╝╚══██╔══╝██╔══██╗██║     ██║     ██║████╗  ██║██╔════╝            ");
        std::println("██║██╔██╗ ██║███████╗   ██║   ███████║██║     ██║     ██║██╔██╗ ██║██║  ███╗           ");
        std::println("██║██║╚██╗██║╚════██║   ██║   ██╔══██║██║     ██║     ██║██║╚██╗██║██║   ██║           ");
        std::println("██║██║ ╚████║███████║   ██║   ██║  ██║███████╗███████╗██║██║ ╚████║╚██████╔╝██╗██╗██╗  ");
        std::println("╚═╝╚═╝  ╚═══╝╚══════╝   ╚═╝   ╚═╝  ╚═╝╚══════╝╚══════╝╚═╝╚═╝  ╚═══╝ ╚═════╝ ╚═╝╚═╝╚═╝\n");
    }

    if (!fs::exists(std::format("{}/.simple-make/compressed/", home)))
        fs::create_directory(std::format("{}/.simple-make/compressed/", home));
    else if (!fs::exists(std::format("{}/.simple-make/compressed/{}", home, name)))
        fs::create_directory(std::format("{}/.simple-make/compressed/{}", home, name));
    
    std::string dir_path = std::format("{}/.simple-make/compressed/{}", home, name);
    std::string file_path = std::format("{}/{}", dir_path, full_name);
    std::string pkg_path = std::format("{}/.simple-make/sources/{}", home, name);

    std::ofstream out(file_path, std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    cpr::Response r = cpr::Get(
    cpr::Url{std::string(url)}, 
    cpr::WriteCallback([&out](std::string_view data, intptr_t) { 
        out.write(data.data(), data.size()); 
        return true; 
    }));

    out.close();

    if (r.status_code != 200) {
        fs::remove_all(dir_path);
        return false;
    }

    std::string cmd;

    if (url.ends_with(".tar.gz") || url.ends_with(".tgz")) {
        cmd = std::format("tar -xzf '{}' -C '{}' --strip-components=1", file_path, pkg_path);
    } else if (url.ends_with(".tar.bz2")) {
        cmd = std::format("tar -xjf '{}' -C '{}' --strip-components=1", file_path, pkg_path);
    } else if (url.ends_with(".tar.xz")) {
        cmd = std::format("tar -xJf '{}' -C '{}' --strip-components=1", file_path, pkg_path);
    } else if (url.ends_with(".tar.zst")) {
        cmd = std::format("tar --zstd -xf '{}' -C '{}' --strip-components=1", file_path, pkg_path);
    } else if (url.ends_with(".zip")) {
        cmd = std::format("unzip -o '{}' -d '{}'", file_path, pkg_path);
    } else if (url.ends_with(".7z")) {
        cmd = std::format("7z x '{}' -o'{}'", file_path, pkg_path);
    } else if (url.ends_with(".rar")) {
        cmd = std::format("unrar x '{}' '{}'", file_path, pkg_path);
    } else if (url.ends_with(".gz")) {
        cmd = std::format("gzip -dk '{}'", file_path);
    } else if (url.ends_with(".xz")) {
        cmd = std::format("xz -dk '{}'", file_path);
    } else if (url.ends_with(".bz2")) {
        cmd = std::format("bzip2 -dk '{}'", file_path);
    } else {
        locked_println("\x1b[31munsupported archive format!\x1b[0m");
        fs::remove(dir_path);
        return false;
    }

    int status = std::system(cmd.c_str());

    fs::remove(dir_path);

    if (status == 0) {
        return true;
    } else {
        return false;
    }
}

void process_package(std::string_view pkg, std::size_t total_packages) {
    SlotGuard slot(build_slots);

    std::string package(pkg);
    std::transform(package.begin(), package.end(), package.begin(), [](unsigned char c) { return std::tolower(c); });

    bool is_github_url = package.starts_with("https://github") || package.starts_with("http://github");
    bool is_github_bare = package.starts_with("://github.com") || package.starts_with("github.com");
    bool is_archive_url = !package.contains("github")    && (package.ends_with(".tar.gz")  ||
                package.ends_with(".tar.xz")  ||  package.ends_with(".zip")     ||
                package.ends_with(".tar.zst") ||  package.ends_with(".tar.bz2") ||
                package.ends_with(".gz")      ||  package.ends_with(".xz")      ||
                package.ends_with(".7z")      ||  package.ends_with(".bz2")     ||
                package.ends_with(".rar"));
                
    std::string url;
    std::string pkg_name = package;

    if (is_github_url || is_github_bare || is_archive_url) {
        url = is_github_bare ? std::format("https://{}", package) : package;
        while (!url.empty() && url.back() == '/')
            url.pop_back();
        pkg_name = get_filename_from_url(url);
    } else if (config.use_system_package_manager && platform::try_install_via_package_manager(package)) {
        locked_println("\x1b[32m(finished) [installed via system package manager] package: {}\x1b[0m", package);
        locked_println("    (remove it with your system package manager, not `simple-make -r`)");
        return;
    }

    if (pkg_name.empty()) {
        locked_println("\x1b[31m(failed) [cannot derive a package name from] package: {}\x1b[0m", package);
        return;
    }

    if (!prepare_paths(pkg_name)) {
        locked_println("\x1b[31m(failed) [cannot create paths] package: {}\x1b[0m", pkg_name);
        return;
    }

    bool fetch_success = true;

    if (is_github_url || is_github_bare) {
        if (!clone_repo_via_git(url, pkg_name)) {
            locked_println("\x1b[31m(failed) [cannot clone] package: {}\x1b[0m", pkg_name);
            return;
        }
    } else if (is_archive_url) {
        fetch_success = download_file(url);
    } else {
        url = get_fast_link(pkg_name);
        if (url.empty()) {
            auto candidates = search_all_sources(pkg_name);
            auto best = pick_best_candidate(candidates, pkg_name);

            if (best.has_value()) {
                url = best->url;
            } else if (!candidates.empty()) {
                {
                    std::scoped_lock lock(mtx);
                    std::println("\x1b[33m[{}] found more than one plausible match, sorted by stars. choose:\x1b[0m", pkg_name);
                    for (std::size_t i = 0; i < candidates.size(); i++)
                        std::println("{}. {} ({}★, {})", i + 1, candidates[i].url, candidates[i].stars, candidates[i].source);
                    std::print("> ");
                }

                std::string input;
                std::getline(std::cin, input);

                try {
                    int choice = std::stoi(input) - 1;
                    if (choice >= 0 && choice < static_cast<int>(candidates.size()))
                        url = candidates[choice].url;
                } catch (...) {}
            }
        }

        if (url.empty()) {
            locked_println("\x1b[31m(failed) [cannot find link] package: {}\x1b[0m", pkg_name);
            return;
        }

        if (!clone_repo_via_git(url, pkg_name)) {
            locked_println("\x1b[31m(failed) [cannot clone] package: {}\x1b[0m", pkg_name);
            return;
        }
    }

    if (!fetch_success) {
        locked_println("\x1b[31m(failed) [cannot fetch] package: {}\x1b[0m", pkg_name);
        return;
    }

    if (!build_pkg(std::format("{}/.simple-make/sources/{}", home, pkg_name), total_packages, pkg_name)) {
        locked_println("\x1b[31m(failed) [cannot build] package: {}\x1b[0m", pkg_name);
        return;
    }

    if (!url.empty()) {
        std::ofstream url_file(std::format("{}/.simple-make/installed/{}.url", home, pkg_name));
        url_file << url;
    }

    if (!config.keep_sources) {
        locked_println("(starting) [deleting source code] package: {}", pkg_name);
        fs::remove_all(std::format("{}/.simple-make/sources/{}", home, pkg_name));
    }

    locked_println("\x1b[32m(finished) [done] package: {}\x1b[0m", pkg_name);
}

std::vector<std::string> read_list(std::string_view path) {
    std::vector<std::string> result;

    if (!fs::exists(path))
        return result;

    std::ifstream file{std::string(path)};
    if (!file.is_open())
        return result;

    std::string line;
    while (std::getline(file, line)) {
        result.push_back(line);
    }

    return result;
}
