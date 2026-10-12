#include "../include/platform.hpp"

#include <cstdlib>
#include <cstdio>
#include <format>
#include <filesystem>

#if !defined(_WIN32)
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#else
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace platform {
    std::string resolve_home_dir() {
    #if !defined(_WIN32)
        if (const char* sudo_user = std::getenv("SUDO_USER")) {
            if (struct passwd* pw = getpwnam(sudo_user); pw && pw->pw_dir)
                return pw->pw_dir;
            return std::format("/home/{}", sudo_user);
        }

        if (const char* home_env = std::getenv("HOME"))
            return home_env;

        if (struct passwd* pw = getpwuid(getuid()); pw && pw->pw_dir)
            return pw->pw_dir;

        return "/root";
    #else
        if (const char* profile = std::getenv("USERPROFILE"))
            return profile;

        const char* drive = std::getenv("HOMEDRIVE");
        const char* path = std::getenv("HOMEPATH");
        if (drive && path)
            return std::string(drive) + path;

        return "C:/Users/Default";
    #endif
    }

    bool requires_elevation() {
        return !is_macos;
    }

    bool is_elevated() {
    #if defined(_WIN32)
        bool elevated = false;
        HANDLE token = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
            TOKEN_ELEVATION elevation{};
            DWORD size = sizeof(elevation);
            if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size))
                elevated = elevation.TokenIsElevated != 0;
            CloseHandle(token);
        }
        return elevated;
    #else
        return geteuid() == 0;
    #endif
    }

    std::string default_install_prefix() {
    #if defined(_WIN32)
        if (const char* local_appdata = std::getenv("LOCALAPPDATA"))
            return std::string(local_appdata) + "/simple-make";
        return resolve_home_dir() + "/simple-make";
    #else
        return resolve_home_dir() + "/.local";
    #endif
    }

    void fix_ownership(const std::string& path) {
    #if defined(_WIN32)
        (void)path;
    #else
        const char* sudo_uid_s = std::getenv("SUDO_UID");
        const char* sudo_gid_s = std::getenv("SUDO_GID");

        uid_t uid;
        gid_t gid;

        if (sudo_uid_s && sudo_gid_s) {
            try {
                uid = static_cast<uid_t>(std::stoul(sudo_uid_s));
                gid = static_cast<gid_t>(std::stoul(sudo_gid_s));
            } catch (...) {
                return;
            }
        } else if (const char* sudo_user = std::getenv("SUDO_USER")) {
            struct passwd* pw = getpwnam(sudo_user);
            if (!pw)
                return;
            uid = pw->pw_uid;
            gid = pw->pw_gid;
        } else {
            return;
        }

        [[maybe_unused]] int rc = ::lchown(path.c_str(), uid, gid);
    #endif
    }

    bool tool_available(const std::string& tool) {
    #if defined(_WIN32)
        std::string cmd = std::format("where {} >nul 2>&1", shell_quote(tool));
    #else
        std::string cmd = std::format("command -v {} > /dev/null 2>&1", shell_quote(tool));
    #endif
        return std::system(cmd.c_str()) == 0;
    }

    std::vector<PackageManager> available_package_managers() {
        static const std::vector<PackageManager> candidates = {
    #if defined(_WIN32)
            {"winget", "winget install --silent --accept-package-agreements --accept-source-agreements {}", "winget list --exact -q {} > nul 2>&1", true},
            {"choco",  "choco install -y {}", "choco list --local-only --exact {} > nul 2>&1", true},
    #elif defined(__APPLE__)
            {"brew",   "brew install {}", "brew list {} > /dev/null 2>&1", false},
    #elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
            {"pkg",    "pkg install -y {}", "pkg info {} > /dev/null 2>&1", true},
    #else
            {"apt-get", "apt-get install -y {}", "dpkg -s {} > /dev/null 2>&1", true},
            {"dnf",     "dnf install -y {}", "rpm -q {} > /dev/null 2>&1", true},
            {"pacman",  "pacman -S --noconfirm {}", "pacman -Q {} > /dev/null 2>&1", true},
            {"zypper",  "zypper install -y {}", "rpm -q {} > /dev/null 2>&1", true},
            {"apk",     "apk add {}", "apk info -e {} > /dev/null 2>&1", true},
    #endif
        };

        std::vector<PackageManager> found;
        for (auto& candidate : candidates) {
            if (tool_available(candidate.name))
                found.push_back(candidate);
        }

        return found;
    }

    bool try_install_via_package_manager(const std::string& pkg) {
        bool elevated = is_elevated();

        for (auto& mgr : available_package_managers()) {
            if (mgr.needs_root != elevated)
                continue;

            std::string cmd = mgr.install_cmd;
            if (auto pos = cmd.find("{}"); pos != std::string::npos)
                cmd.replace(pos, 2, shell_quote(pkg));

            if (std::system(cmd.c_str()) == 0)
                return true;
        }

        return false;
    }

    bool is_installed_via_package_manager(const std::string& pkg) {
        for (auto& mgr : available_package_managers()) {
            if (mgr.query_cmd.empty())
                continue;

            std::string cmd = mgr.query_cmd;
            if (auto pos = cmd.find("{}"); pos != std::string::npos)
                cmd.replace(pos, 2, shell_quote(pkg));

            if (std::system(cmd.c_str()) == 0)
                return true;
        }

        return false;
    }

    void enable_ansi_colors() {
#if defined(_WIN32)
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD mode = 0;
        if (out != INVALID_HANDLE_VALUE && GetConsoleMode(out, &mode))
            SetConsoleMode(out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
    }

    std::string shell_quote(const std::string& value) {
#if defined(_WIN32)
        std::string out = "\"";
        for (char c : value) {
            if (c == '"')
                out += "\\\"";
            else
                out += c;
        }
        out += '"';
        return out;
#else
        std::string out = "'";
        for (char c : value) {
            if (c == '\'')
                out += "'\\''";
            else
                out += c;
        }
        out += '\'';
        return out;
#endif
    }

#if !defined(_WIN32)
    static std::string unprivileged_user() {
        const char* sudo_user = std::getenv("SUDO_USER");
        if (!sudo_user)
            return {};

        std::string user = sudo_user;
        if (user.empty() || user == "root")
            return {};

        if (!getpwnam(user.c_str()))
            return {};

        return user;
    }
#endif

    bool can_drop_privileges() {
#if defined(_WIN32)
        return false;
#else
        if (::geteuid() != 0)
            return true;

        if (unprivileged_user().empty())
            return false;

        return tool_available("sudo") || tool_available("runuser") || tool_available("su");
#endif
    }

    std::string wrap_unprivileged(const std::string& cmd) {
#if defined(_WIN32)
        return cmd;
#else
        if (::geteuid() != 0)
            return cmd;

        std::string user = unprivileged_user();
        if (user.empty())
            return cmd;

        if (tool_available("sudo"))
            return std::format("sudo -u {} -- sh -c {}", shell_quote(user), shell_quote(cmd));

        if (tool_available("runuser"))
            return std::format("runuser -u {} -- sh -c {}", shell_quote(user), shell_quote(cmd));

        if (tool_available("su"))
            return std::format("su {} -c {}", shell_quote(user), shell_quote(cmd));

        return cmd;
#endif
    }
}
