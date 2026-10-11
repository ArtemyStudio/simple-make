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
    #elif defined(__APPLE__)
        return true;
    #else
        return geteuid() == 0;
    #endif
    }

    std::string default_install_prefix() {
    #if defined(_WIN32)
        return "C:/simple-make";
    #else
        return "/usr/local";
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

        std::error_code ec;
        if (!fs::exists(path, ec))
            return;

        ::chown(path.c_str(), uid, gid);

        if (fs::is_directory(path, ec)) {
            for (const auto& entry : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec))
                ::chown(entry.path().c_str(), uid, gid);
        }
    #endif
    }

    bool tool_available(const std::string& tool) {
    #if defined(_WIN32)
        std::string cmd = std::format("where {} >nul 2>&1", tool);
    #else
        std::string cmd = std::format("command -v {} > /dev/null 2>&1", tool);
    #endif
        return std::system(cmd.c_str()) == 0;
    }

    std::vector<PackageManager> available_package_managers() {
        static const std::vector<PackageManager> candidates = {
    #if defined(_WIN32)
            {"winget", "winget install --silent --accept-package-agreements --accept-source-agreements {}"},
            {"choco",  "choco install -y {}"},
    #elif defined(__APPLE__)
            {"brew",   "brew install {}"},
    #elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
            {"pkg",    "pkg install -y {}"},
    #else
            {"apt-get", "apt-get install -y {}"},
            {"dnf",     "dnf install -y {}"},
            {"pacman",  "pacman -S --noconfirm {}"},
            {"zypper",  "zypper install -y {}"},
            {"apk",     "apk add {}"},
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
        for (auto& mgr : available_package_managers()) {
            std::string cmd = mgr.install_cmd;
            if (auto pos = cmd.find("{}"); pos != std::string::npos)
                cmd.replace(pos, 2, pkg);

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
}
