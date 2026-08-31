#include "../include/build.hpp"
#include "../include/main.hpp"
#include "../include/parse_cfg.hpp"
#include "../include/installed_list.hpp"

#include <print>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <mutex>
#include <thread>
#include <algorithm>
#include <cstddef>

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

    std::string jobs = (config.jobs > 0) ? std::to_string(config.jobs) : std::to_string(auto_jobs_per_package(total_packages));

    std::string log_path = std::format("{}/.simple-make/logs/{}.log", home, package);
    if (!fs::exists(std::format("{}/.simple-make/logs", home)))
        fs::create_directory(std::format("{}/.simple-make/logs", home));
    
    switch (builder) {
    case Builders::Cmake:
        if (!do_continue()) 
            return false;
        cmd = std::format("cd {} && {{ cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX={} && cmake --build build -j{} && cmake --install build; }} > {} 2>&1", path, config.install_prefix, jobs, log_path);
        break;
    case Builders::Meson:
        if (!do_continue())
            return false;
        cmd = std::format("cd {} && {{ meson setup build --prefix={} --buildtype=release && meson compile -C build -j{} && meson install -C build; }} > {} 2>&1", path, config.install_prefix, jobs, log_path);
        break;
    case Builders::Ninja:
        if (!do_continue())
            return false;
        cmd = std::format("cd {} && {{ ninja -j{} && ninja install; }} > {} 2>&1", path, jobs, log_path);
        break;
    case Builders::Make:
        if (!do_continue())
            return false;
        cmd = std::format("cd {} && {{ make -j{} && make install PREFIX={} DESTDIR= INSTALL_PREFIX={}; }} > {} 2>&1", path, jobs, config.install_prefix, config.install_prefix, log_path);
        break;
    case Builders::Autotools:
        if (!do_continue())
            return false;
        cmd = std::format("cd {} && {{ if [ -x ./autogen.sh ]; then ./autogen.sh; fi && if [ ! -f ./configure ]; then autoreconf -fi; fi && ./configure --prefix={} && make -j{} && make install; }} > {} 2>&1", path, config.install_prefix, jobs, log_path);
        break;
    case Builders::Rust:
        if (!do_continue()) 
            return false;
        {
            std::string bin_dir = config.install_prefix + "/bin";
            std::string bash_loop = "for f in " + home + "/.bashrc " + home + "/.zshrc; do grep -q '" + bin_dir + "' \"$f\" 2>/dev/null || echo 'export PATH=\"" + bin_dir + ":$PATH\"' >> \"$f\"; done";
            cmd = std::format("cd {} && cargo install --path . --root {} > {} 2>&1 && {}", path, config.install_prefix, log_path, bash_loop);
        }
        break;
    case Builders::Go:
        if (!do_continue())
            return false;
        {
            std::string bin_dir = config.install_prefix + "/bin";
            std::string bash_loop = "for f in " + home + "/.bashrc " + home + "/.zshrc; do grep -q '" + bin_dir + "' \"$f\" 2>/dev/null || echo 'export PATH=\"" + bin_dir + ":$PATH\"' >> \"$f\"; done";
            cmd = std::format("cd {} && GOBIN={}/bin go install -v ./... > {} 2>&1 && {}", path, config.install_prefix, log_path, bash_loop);
        }
            break;
    case Builders::Python:
        if (!do_continue()) 
            return false;
        cmd = std::format("cd {} && pip install --prefix={} . > {} 2>&1", path, config.install_prefix, log_path);
        break;
            
    case Builders::Java:
        if (!do_continue())
            return false;
        {
            std::string jar_dir = fs::exists(path + "/build.gradle") ? "build/libs" : "target";
            std::string pkg_build = fs::exists(path + "/build.gradle") ? "./gradlew build" : "mvn package";
            cmd = std::format("cd {} && {} > {} 2>&1 && JAR=$(find {} -maxdepth 1 -name '*.jar' ! -name '*-sources.jar' ! -name '*-javadoc.jar' | head -1) && native-image -jar \"$JAR\" -o {}/bin/{} >> {} 2>&1", 
                path, pkg_build, log_path, jar_dir, config.install_prefix, package, log_path);
        }
        break;
            
    case Builders::Unknown:
        locked_println("\x1b[31munknown build tool. cannot compile this repository\x1b[0m");
        return false;
    }

    std::string prefix = config.install_prefix;

    if (prefix.empty()) {
        prefix = "/usr/local/";
    } else {
        if (prefix.front() != '/')
            prefix = '/' + prefix;
        if (prefix.back() != '/')
            prefix += '/';

        if (!fs::exists(prefix))
            fs::create_directory(prefix);
    }

    std::scoped_lock install_lock(install_mtx);

    std::set<std::string> first = snapshot_dir(prefix);

    int status = system(cmd.c_str());

    if (status == 0) {
        std::set<std::string> second = snapshot_dir(prefix);
        std::vector<std::string> diff_ = diff(first, second);

        if (!fs::exists(std::format("{}/.simple-make/installed", home)))
            fs::create_directory(std::format("{}/.simple-make/installed", home));

        std::ofstream out_file(std::format("{}/.simple-make/installed/{}.list", home, package));
        for (auto entry : diff_)
            out_file << entry << '\n';
        out_file.close();
    
        return true;
    } else {
        return false;
    }
}
