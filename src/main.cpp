#include "../include/search.hpp"
#include "../include/main.hpp"
#include "../include/db.hpp"
#include "../include/build.hpp"
#include "../include/parse_cfg.hpp"

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
#include <filesystem>
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <cctype>
#include <mutex>
#include <semaphore>

namespace fs = std::filesystem;

std::string home;
std::mutex mtx;

namespace {
    std::counting_semaphore<> build_slots{ static_cast<std::ptrdiff_t>(std::max(1u, std::thread::hardware_concurrency())) };

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
    if (geteuid() != 0) {
        std::println("\x1b[31m(failed) [not root] you must run this program as root\x1b[0m");
        return 1;
    }

    auto args = std::span(argv, argc).subspan(1);
    if (args.empty()) {
        std::println("usage: sudo simple-make <package-name> [package-name...]\n");
        return 1;
    }

    load_cfg();

    const char* tmp = std::getenv("SUDO_USER");
    if (config.home_path.empty())
        home = tmp ? std::format("/home/{}", tmp) : "/root";
    else
        home = config.home_path;

    if (std::string_view(args[0]) == "--help" || std::string_view(args[0]) == "-h") {
        std::println("\nsimple-make — modern package downloader & builder (C++23)\n\n");

        std::println("████████████████████████████████████████████████████████████████████████████████████████████████████████████████████");
        std::println("██                                                                                                                ██");
        std::println("██  usage:                                                                                                        ██");
        std::println("██      simple-make <package-name>                   search, download and build a package by name                 ██");
        std::println("██      simple-make <github-url>                     download and build a package directly from a github URL      ██");
        std::println("██      simple-make <any_url.com/examle.tar.gz>      download, extract and build a package directly from any URL  ██");
        std::println("██                                                                                                                ██");
        std::println("████████████████████████████████████████████████████████████████████████████████████████████████████████████████████\n\n");


        std::println("██████████████████████████████████████████████████████████████████████████████");
        std::println("██                                                                          ██");
        std::println("██  options:                                                                ██");
        std::println("██      -h, --help                    print this help information and exit  ██");
        std::println("██      -r, --remove, --delete, -d    remove all installed packages         ██");
        std::println("██      -s, --search                  search for a package                  ██");
        std::println("██      --setup-config                setup config file                     ██");
        std::println("██                                                                          ██");
        std::println("██████████████████████████████████████████████████████████████████████████████\n\n");


        std::println("██████████████████████████████████████████████████████████████████████████████████████████████████████████████");
        std::println("██                                                                                                          ██");
        std::println("██  examples:                                                                                               ██");
        std::println("██      simple-make peaclock     (Uses fast local database match)                                           ██");
        std::println("██      simple-make yq           (Triggers automated GitHub API search)                                     ██");
        std::println("██                                                                                                          ██");
        std::println("██      simple-make https://github.com/ArtemyStudio/simple-make              (Installing from GH URL)       ██");
        std::println("██      simple-make https://openbox.org/dist/openbox/openbox-3.6.1.tar.xz    (Instalinug from URL, tar.gz)  ██");
        std::println("██                                                                                                          ██");
        std::println("██████████████████████████████████████████████████████████████████████████████████████████████████████████████\n\n");


        std::println("█████████████████████████████████████████████████████████████████");
        std::println("██                                                             ██");
        std::println("██  dependencies:                                              ██");
        std::println("██     version control:                                        ██");
        std::println("██        git                 -> git                           ██");
        std::println("██                                                             ██");
        std::println("██     compilers & build systems:                              ██");
        std::println("██        c/c++ (make)        -> make                          ██");
        std::println("██        c/c++ (cmake)       -> cmake                         ██");
        std::println("██        c/c++ (meson)       -> meson                         ██");
        std::println("██        c/c++ (ninja)       -> ninja                         ██");
        std::println("██        c/c++ (gcc/g++)     -> gcc / g++                     ██");
        std::println("██        c/c++ (autotools)   -> autoconf / automake           ██");
        std::println("██        rust (cargo)        -> cargo                         ██");
        std::println("██        rust (rustc)        -> rustc                         ██");
        std::println("██        go (go)             -> go                            ██");
        std::println("██        python (pip)        -> pip                           ██");
        std::println("██        java (maven)        -> mvn                           ██");
        std::println("██        java (gradle)       -> gradle                        ██");
        std::println("██        java (native-image) -> native-image                  ██");
        std::println("██        java (javac)        -> javac                         ██");
        std::println("██                                                             ██");
        std::println("██     archivers & extractors:                                 ██");
        std::println("██        tar archives        -> tar                           ██");
        std::println("██        gzip (.gz)          -> gzip                          ██");
        std::println("██        xz (.xz)            -> xz                            ██");
        std::println("██        bzip2 (.bz2)        -> bzip2                         ██");
        std::println("██        zstandard (.zst)    -> zstd                          ██");
        std::println("██        zip (.zip)          -> unzip                         ██");
        std::println("██        7-zip (.7z)         -> 7z                            ██");
        std::println("██        rar (.rar)          -> unrar                         ██");
        std::println("██                                                             ██");
        std::println("█████████████████████████████████████████████████████████████████\n\n");


        std::println("█████████████████████████████████████████████████████████████████████████████████████████████████████");
        std::println("██                                                                                                 ██");
        std::println("██  config (~/.config/simple-make/simple-make.conf):                                               ██");
        std::println("██     options:                                                                                    ██");
        std::println("██          [bool]   auto_confirm    ->  automatically approves questions                          ██");
        std::println("██          [bool]   keep_sources    ->  keep sources after installation                           ██");
        std::println("██          [int]    jobs            ->  how many cores will be used for compilation               ██");
        std::println("██          [string] home_path       ->  the path to the home directory                            ██");
        std::println("██          [string] install_prefix  ->  where the binary will be installed                        ██");
        std::println("██          [string] github_token    ->  this is a very important thing; without it, you can only  ██");
        std::println("██                                         send 60 requests per hour                               ██");
        std::println("██                                                                                                 ██");
        std::println("██     example:                                                                                    ██");
        std::println("██           github_token = \"ghp_x87yA2b3C4d5E6f7G8h9I0j1K2l3M4n5O6p7\"                             ██");
        std::println("██           keep_sources = true                                                                   ██");
        std::println("██           # this is comment. btw, the github token isn't real; don't even bother trying         ██");
        std::println("██           install_prefix = \"/usr/local\"                                                         ██");
        std::println("██           # Be careful: the binary will be installed in /usr/local/bin/ (by default, they are   ██");
        std::println("██           #   installed there anyway, but you can change the path)                              ██");
        std::println("██          home_path = \"/home/artemy\"                                                             ██");
        std::println("██                                                                                                 ██");
        std::println("█████████████████████████████████████████████████████████████████████████████████████████████████████\n\n");

        std::println("████████████████████████████████████████████████████████████");
        std::println("██                                                        ██");
        std::println("██    paths:                                              ██");
        std::println("██        config: ~/.config/simple-make/simple-make.conf  ██");
        std::println("██        sources: ~/.simple-make/sources                 ██");
        std::println("██        installed_list: ~/.simple-make/installed        ██");
        std::println("██        compressed: ~/.simple-make/compressed           ██");
        std::println("██        logs: ~/.simple-make/logs                       ██");
        std::println("██                                                        ██");
        std::println("████████████████████████████████████████████████████████████");

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

        std::println("█████████████████████████████");
        std::println("██                         ██");
        std::println("██    auto_confirm = {}    ██", config.auto_confirm ? "true" : "false");
        std::println("██    keep_sources = {}    ██", config.keep_sources ? "true" : "false");
        std::println("██    jobs = {}            ██", config.jobs == 0 ? "auto" : std::to_string(config.jobs));
        std::println("██    install_prefix = {}  ██", config.install_prefix);
        std::println("██    github_token = {}    ██", config.github_token == "" ? "empty" : config.github_token);
        std::println("██    home_path = {}       ██", config.home_path == "" ? "/root" : config.home_path);
        std::println("██                         ██");
        std::println("█████████████████████████████");

        cfg_file.close();
        return 0;
    } else if (std::string_view(args[0]) == "-s" || std::string_view(args[0]) == "--search") {
    std::vector<std::string_view> packages(args.begin() + 1, args.end());
    
    for (auto& package : packages) {
        auto res = search_on_gh(package, true, 3);

        bool installed = fs::exists(std::format("{}/.simple-make/installed/{}.list", home, package));

        if (!res.has_value()) {
            std::vector<std::string> urls = std::move(res.error());

            if (urls.empty()) {
                locked_println("\x1b[31m(failed) [no links found] package: {}\x1b[0m", package);
            } else {
                locked_println("\x1b[32m(finished) [done] package: {}{}\x1b[0m", package, installed ? " (installed)" : "");
                for (const auto& url : urls) {
                    locked_println("  -> \x1b[34m{}\x1b[0m", url);
                }
            }
        } else {
            std::string url = res.value();
            if (url.starts_with("https")) {
                locked_println("\x1b[32m(finished) [done] package: {} -> \x1b[34m{}\x1b[0m{}", package, url, installed ? " (installed)" : "");
            } else {
                locked_println("\x1b[31m(failed) [{}] package: {}\x1b[0m", url, package);
            }
        }
        std::println("");
    }

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
            auto res = search_on_gh(pkg_name);

            if (res.has_value()) {
                url = std::move(res.value());
            } else {
                url = "";
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
