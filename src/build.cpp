#include "../include/build.hpp"
#include "../include/main.hpp"
#include "../include/parse_cfg.hpp"
#include "../include/installed_list.hpp"
#include "../include/platform.hpp"

#include <print>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <thread>
#include <algorithm>
#include <cstddef>
#include <vector>
#include <cctype>
#include <format>
#include <set>
#include <string_view>

namespace fs = std::filesystem;

static std::mutex install_mtx;

static int auto_jobs_per_package(std::size_t total_packages) {
    unsigned int hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 1;

    long available = std::max<long>(1, static_cast<long>(hw) - 2);
    long concurrent = static_cast<long>(std::max<std::size_t>(1, total_packages));

    return static_cast<int>(std::max<long>(1, available / concurrent));
}

Builders get_builder(std::string path) {
    if (fs::exists(path + "/CMakeLists.txt"))
        return Builders::Cmake;
    else if (fs::exists(path + "/meson.build"))
        return Builders::Meson;
    else if (fs::exists(path + "/build.ninja"))
        return Builders::Ninja;
    else if (fs::exists(path + "/Makefile") || fs::exists(path + "/makefile"))
        return Builders::Make;
    else if (fs::exists(path + "/Makefile.in") || fs::exists(path + "/Makefile.am"))
        return Builders::Autotools;
    else if (fs::exists(path + "/Cargo.toml"))
        return Builders::Rust;
    else if (fs::exists(path + "/go.mod"))
        return Builders::Go;
    else if (fs::exists(path + "/pyproject.toml") || fs::exists(path + "/setup.py"))
        return Builders::Python;
    else if (fs::exists(path + "/pom.xml") || fs::exists(path + "/build.gradle"))
        return Builders::Java;
    else
        return Builders::Unknown;
}

static std::vector<std::string> detect_all_builders(const std::string& path) {
    std::vector<std::string> found;

    if (fs::exists(path + "/CMakeLists.txt")) found.push_back("cmake");
    if (fs::exists(path + "/meson.build")) found.push_back("meson");
    if (fs::exists(path + "/build.ninja")) found.push_back("ninja");
    if (fs::exists(path + "/Makefile") || fs::exists(path + "/makefile")) found.push_back("make");
    if (fs::exists(path + "/Makefile.in") || fs::exists(path + "/Makefile.am")) found.push_back("autotools");
    if (fs::exists(path + "/Cargo.toml")) found.push_back("cargo");
    if (fs::exists(path + "/go.mod")) found.push_back("go");
    if (fs::exists(path + "/pyproject.toml") || fs::exists(path + "/setup.py")) found.push_back("pip");
    if (fs::exists(path + "/pom.xml") || fs::exists(path + "/build.gradle")) found.push_back("java/gradle");

    return found;
}

static std::vector<std::string> required_tools(Builders builder) {
    switch (builder) {
        case Builders::Cmake:     return {"cmake"};
        case Builders::Meson:     return {"meson"};
        case Builders::Ninja:     return {"ninja"};
        case Builders::Make:      return {"make"};
        case Builders::Autotools: return {"make"};
        case Builders::Rust:      return {"cargo"};
        case Builders::Go:        return {"go"};
        case Builders::Python:    return {"pip"};
        case Builders::Java:      return {"native-image"};
        default:                  return {};
    }
}

static void record_installed(const std::string& prefix, const std::set<std::string>& before, std::string_view package, const std::string& log_path) {
    std::vector<std::string> added = diff(before, snapshot_dir(prefix));

    if (!fs::exists(std::format("{}/.simple-make/installed", home)))
        fs::create_directory(std::format("{}/.simple-make/installed", home));

    std::ofstream out_file(std::format("{}/.simple-make/installed/{}.list", home, package));
    for (auto& entry : added)
        out_file << entry << '\n';
    out_file.close();

    platform::fix_ownership(std::format("{}/.simple-make/installed", home));
    platform::fix_ownership(std::format("{}/.simple-make/installed/{}.list", home, package));
    platform::fix_ownership(std::format("{}/.simple-make/logs", home));
    platform::fix_ownership(log_path);
}

static std::string path_hint_cmd(const std::string& bin_dir) {
    std::string export_line = std::format("export PATH=\"{}:$PATH\"", bin_dir);

    return std::format("for f in {} {}; do grep -qF {} \"$f\" 2>/dev/null || echo {} >> \"$f\"; done",
        platform::shell_quote(home + "/.bashrc"),
        platform::shell_quote(home + "/.zshrc"),
        platform::shell_quote(bin_dir),
        platform::shell_quote(export_line));
}

bool do_continue() {
    if (config.auto_confirm)
        return true;

    std::scoped_lock lock(mtx);

    std::print("do you want to proseed? (Y/n) >");

    std::string input;
    std::getline(std::cin, input);
    
    if (input == "N" || input == "n") {
        return false;
    } else {
        return true;
    }
}

bool build_pkg(std::string path, std::size_t total_packages, std::string_view package) {
    Builders builder = get_builder(path);
    std::string cmd;

    auto all_builders = detect_all_builders(path);
    if (all_builders.size() > 1) {
        std::string joined;
        for (std::size_t i = 0; i < all_builders.size(); i++) {
            joined += all_builders[i];
            if (i + 1 < all_builders.size()) joined += ", ";
        }

        locked_println("\x1b[33m(warning) [multiple build systems detected: {}] package: {} -> picking the first one it knows how to handle; if that's the wrong one, build it manually\x1b[0m", joined, package);
    }

    for (auto& tool : required_tools(builder)) {
        if (!platform::tool_available(tool)) {
            locked_println("\x1b[31m(failed) [missing dependency: {}] package: {} -> install it with your system package manager (apt/pacman/dnf/...) and try again\x1b[0m", tool, package);
            return false;
        }
    }

    std::string jobs = (config.jobs > 0) ? std::to_string(config.jobs) : std::to_string(auto_jobs_per_package(total_packages));

    std::string log_path = std::format("{}/.simple-make/logs/{}.log", home, package);
    if (!fs::exists(std::format("{}/.simple-make/logs", home)))
        fs::create_directory(std::format("{}/.simple-make/logs", home));

    std::string prefix = config.install_prefix;

    if (prefix.empty()) {
        prefix = platform::default_install_prefix() + "/";
    } else {
        bool is_drive_path = prefix.size() >= 2 && std::isalpha(static_cast<unsigned char>(prefix[0])) && prefix[1] == ':';
        if (!is_drive_path && prefix.front() != '/')
            prefix = '/' + prefix;
        if (prefix.back() != '/')
            prefix += '/';
    }

    // Builds and configure steps run untrusted, repo-controlled code (CMakeLists.txt,
    // build.rs, setup.py, Makefiles, ...), so they go through wrap_unprivileged() and
    // land in a staging dir first. Only the final copy into the prefix needs the
    // install lock, which keeps concurrent builds actually concurrent, and only a
    // prefix outside the user's home needs root for that copy.
    bool needs_staging = !platform::is_windows;
    std::string stage_dir = needs_staging ? std::format("{}/.simple-make/stage/{}", home, package) : "";
    std::string prefix_arg = prefix.substr(0, prefix.size() - 1);
    std::string destdir = needs_staging ? stage_dir : "";
    std::string direct_target = needs_staging ? stage_dir : prefix_arg;
    bool destdir_style = false;

    if (needs_staging) {
        fs::remove_all(stage_dir);
        fs::create_directories(stage_dir);
    } else if (!fs::exists(prefix)) {
        fs::create_directories(prefix);
    }

    std::string q_path = platform::shell_quote(path);
    std::string q_prefix = platform::shell_quote(prefix_arg);
    std::string q_destdir = platform::shell_quote(destdir);
    std::string q_direct = platform::shell_quote(direct_target);
    std::string q_log = platform::shell_quote(log_path);

    switch (builder) {
    case Builders::Cmake:
        if (!do_continue())
            return false;
        destdir_style = true;
        if (platform::is_windows)
            cmd = std::format("cd {} && ( cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX={} && cmake --build build -j{} && cmake --install build ) > {} 2>&1", q_path, q_prefix, jobs, q_log);
        else
            cmd = std::format("cd {} && {{ cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX={} && cmake --build build -j{} && DESTDIR={} cmake --install build; }} > {} 2>&1", q_path, q_prefix, jobs, q_destdir, q_log);
        break;

    case Builders::Meson:
        if (!do_continue())
            return false;
        destdir_style = true;
        if (platform::is_windows)
            cmd = std::format("cd {} && ( meson setup build --prefix={} --buildtype=release && meson compile -C build -j{} && meson install -C build ) > {} 2>&1", q_path, q_prefix, jobs, q_log);
        else
            cmd = std::format("cd {} && {{ meson setup build --prefix={} --buildtype=release && meson compile -C build -j{} && DESTDIR={} meson install -C build; }} > {} 2>&1", q_path, q_prefix, jobs, q_destdir, q_log);
        break;

    case Builders::Ninja:
        if (!do_continue())
            return false;
        destdir_style = true;
        if (platform::is_windows)
            cmd = std::format("cd {} && ( ninja -j{} && ninja install ) > {} 2>&1", q_path, jobs, q_log);
        else
            cmd = std::format("cd {} && {{ ninja -j{} && DESTDIR={} ninja install; }} > {} 2>&1", q_path, jobs, q_destdir, q_log);
        break;

    case Builders::Make:
        if (!do_continue())
            return false;
        destdir_style = true;
        if (platform::is_windows)
            cmd = std::format("cd {} && ( make -j{} && make install PREFIX={} INSTALL_PREFIX={} ) > {} 2>&1", q_path, jobs, q_prefix, q_prefix, q_log);
        else
            cmd = std::format("cd {} && {{ make -j{} && make install PREFIX={} DESTDIR={} INSTALL_PREFIX={}; }} > {} 2>&1", q_path, jobs, q_prefix, q_destdir, q_prefix, q_log);
        break;

    case Builders::Autotools:
        if (!do_continue())
            return false;

        destdir_style = true;
        cmd = std::format("cd {} && {{ if [ -x ./autogen.sh ]; then ./autogen.sh; fi && if [ ! -f ./configure ]; then autoreconf -fi; fi && ./configure --prefix={} && make -j{} && make install DESTDIR={}; }} > {} 2>&1", q_path, q_prefix, jobs, q_destdir, q_log);
        break;

    case Builders::Rust:
        if (!do_continue())
            return false;

        {
            std::string bin_dir = prefix_arg + "/bin";
            if (platform::is_windows) {
                cmd = std::format("cd {} && cargo install --path . --root {} > {} 2>&1", q_path, q_direct, q_log);
                locked_println("(note) add {} to your PATH to run the installed binary", bin_dir);
            } else {
                cmd = std::format("cd {} && cargo install --path . --root {} > {} 2>&1 && {}", q_path, q_direct, q_log, path_hint_cmd(bin_dir));
            }
        }

        break;

    case Builders::Go:
        if (!do_continue())
            return false;
        {
            std::string bin_dir = prefix_arg + "/bin";
            if (platform::is_windows) {
                cmd = std::format("cd {} && set \"GOBIN={}\\bin\" && go install -v ./... > {} 2>&1", q_path, direct_target, q_log);
                locked_println("(note) add {} to your PATH to run the installed binary", bin_dir);
            } else {
                cmd = std::format("cd {} && GOBIN={} go install -v ./... > {} 2>&1 && {}", q_path, platform::shell_quote(direct_target + "/bin"), q_log, path_hint_cmd(bin_dir));
            }
        }
        break;

    case Builders::Python:
        if (!do_continue())
            return false;
        cmd = std::format("cd {} && pip install --prefix={} . > {} 2>&1", q_path, q_direct, q_log);
        break;

    case Builders::Java:
        if (!do_continue())
            return false;
        {
            std::string jar_dir = fs::exists(path + "/build.gradle") ? "build/libs" : "target";
            std::string pkg_build = fs::exists(path + "/build.gradle") ? "./gradlew build" : "mvn package";
            if (platform::is_windows) {
                cmd = std::format("cd {} && {} > {} 2>&1 && for /f \"delims=\" %i in ('dir /b \"{}\\*.jar\" 2^>nul ^| findstr /v /i \"sources javadoc\"') do native-image -jar \"{}\\%i\" -o {}\\bin\\{} >> {} 2>&1",
                    q_path, pkg_build, q_log, jar_dir, jar_dir, direct_target, package, q_log);
            } else {
                cmd = std::format("cd {} && {} > {} 2>&1 && JAR=$(find {} -maxdepth 1 -name '*.jar' ! -name '*-sources.jar' ! -name '*-javadoc.jar' | head -1) && native-image -jar \"$JAR\" -o {} >> {} 2>&1",
                    q_path, pkg_build, q_log, platform::shell_quote(jar_dir), platform::shell_quote(std::format("{}/bin/{}", direct_target, package)), q_log);
            }
        }
        break;

    case Builders::Unknown:
        locked_println("\x1b[31munknown build tool. cannot compile this repository\x1b[0m");
        return false;
    }

    if (platform::is_elevated() && !platform::can_drop_privileges()) {
        locked_println("\x1b[33m(warning) [cannot drop privileges] package: {} -> this build will run as root. set SUDO_USER (run via sudo from a normal account) or install sudo/runuser/su so the build can run unprivileged\x1b[0m", package);
    }

    std::string wrapped_cmd = platform::wrap_unprivileged(cmd);
    int status = 0;

    if (needs_staging) {
        status = system(wrapped_cmd.c_str());

        if (status == 0) {
            std::scoped_lock install_lock(install_mtx);
            std::set<std::string> first = snapshot_dir(prefix);

            std::string merge_source = destdir_style ? (stage_dir + prefix_arg) : stage_dir;
            std::string merge_cmd = std::format("mkdir -p {} && cp -a {} {}",
                platform::shell_quote(prefix_arg),
                platform::shell_quote(merge_source + "/."),
                platform::shell_quote(prefix_arg + "/"));
            status = system(merge_cmd.c_str());

            if (status == 0)
                record_installed(prefix, first, package, log_path);
        }

        fs::remove_all(stage_dir);
    } else {
        std::scoped_lock install_lock(install_mtx);
        std::set<std::string> first = snapshot_dir(prefix);

        status = system(wrapped_cmd.c_str());

        if (status == 0)
            record_installed(prefix, first, package, log_path);
    }

    if (status == 0) {
        return true;
    } else {
        std::ifstream log_in(log_path);
        std::vector<std::string> tail;
        std::string line;
        while (std::getline(log_in, line)) {
            tail.push_back(line);
            if (tail.size() > 20)
                tail.erase(tail.begin());
        }

        locked_println("\x1b[31m(failed) [build error, last {} lines of {}]\x1b[0m", tail.size(), log_path);
        for (auto& l : tail)
            locked_println("    {}", l);

        return false;
    }
}
