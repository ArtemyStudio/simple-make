#pragma once

#include <string>
#include <vector>

namespace platform {

#if defined(_WIN32)
    inline constexpr bool is_windows = true;
#else
    inline constexpr bool is_windows = false;
#endif

#if defined(__APPLE__)
    inline constexpr bool is_macos = true;
#else
    inline constexpr bool is_macos = false;
#endif

#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
    inline constexpr bool is_bsd = true;
#else
    inline constexpr bool is_bsd = false;
#endif

    inline constexpr bool is_linux = !is_windows && !is_macos && !is_bsd;

    std::string resolve_home_dir();
    bool requires_elevation();
    bool is_elevated();
    std::string default_install_prefix();
    void fix_ownership(const std::string& path);

    struct PackageManager {
        std::string name;
        std::string install_cmd;
    };

    std::vector<PackageManager> available_package_managers();
    bool try_install_via_package_manager(const std::string& pkg);
    bool tool_available(const std::string& tool);
    void enable_ansi_colors();
}
