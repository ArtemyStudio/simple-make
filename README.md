# simple-make

Grab a random project, build it, and install it — all in one command. Written in C++23. Runs on Linux, macOS, BSD and Windows.
You give it a name or a URL. It figures out where the source is — your system package manager first (apt/dnf/pacman/zypper/apk/brew/pkg/winget/choco), then a built-in database of 1000 popular projects, then a live search across GitHub, GitLab and Codeberg — downloads it, detects the build system, and compiles it.
Give it multiple packages — it builds them in parallel without nuking your CPU. Oh, and it actually tracks installed files so you can cleanly uninstall stuff built from source.

## Quick start

```bash
git clone https://github.com/ArtemyStudio/simple-make.git
cd simple-make
cmake -B build && cmake --build build -j$(nproc)
sudo cmake --install build
```
*(Dependencies like `cpr` and `nlohmann/json` are fetched automatically by CMake)*

On Arch/Artix, you can instead build it as a proper package from a separate directory:

```bash
mkdir simple-make-pkg && cp PKGBUILD simple-make-pkg/ && cd simple-make-pkg
makepkg -si
```

## Usage

```bash
sudo simple-make ripgrep fd bat          # build several packages in parallel
sudo simple-make https://github.com/...  # install directly from a URL
sudo simple-make -s cmake                # just search, don't install
sudo simple-make -r ripgrep              # cleanly remove
sudo simple-make -u                      # rebuild every installed package against latest upstream
sudo simple-make -u ripgrep              # rebuild just that one
sudo simple-make --self-update           # check simple-make's own releases, and rebuild itself if newer
sudo simple-make --setup-config          # interactive config wizard
```

On Windows, run the equivalent commands from an elevated (Administrator) terminal instead of with `sudo`; on macOS, don't use `sudo` at all — Homebrew refuses to run as root, so simple-make never requires elevation there.

## Config

Run `sudo simple-make --setup-config` once to generate `~/.config/simple-make/simple-make.conf` (`%USERPROFILE%\.config\simple-make\simple-make.conf` on Windows).

It lets you set the install prefix, toggle auto-confirm, keep source files after building, add a GitHub/GitLab token (to raise their anonymous search rate limits), toggle whether the system package manager is tried before building from source, and toggle a startup check for simple-make's own updates. You can also set `home_path` if `sudo` messes up directory detection, and `search_providers` to change which of GitHub/GitLab/Codeberg get queried (and in what order).

## How it works

1. **Resolves** the name: your system package manager first, then the built-in database, then a live search across every provider in `search_providers` (an exact name match always wins over star count; a genuine tie is the only time you're asked to pick).
2. **Downloads** it into `~/.simple-make/sources/<package>` (skipped entirely when the system package manager already has it).
3. **Detects** the build system (`CMakeLists.txt`, `Cargo.toml`, `go.mod`, `meson.build`, etc.), warning you if more than one is present in the same repo.
4. **Builds & installs**, logging everything to `~/.simple-make/logs/<package>.log` and printing the log's tail straight to the console if the build fails. Compile jobs are split evenly across parallel builds so your machine doesn't choke.
5. **Tracks** every file that landed in the install prefix (`~/.simple-make/installed/<package>.list`) so `-r` can wipe them cleanly, plus the exact source URL it resolved (`~/.simple-make/installed/<package>.url`) so `-u`/`--self-update` rebuilds from that same source instead of re-resolving the name and risking a different, same-named repo. Neither of these touches your own `simple-make.conf` or any other app's settings — an update only ever replaces what that package's own install step put in the prefix.

## Dependencies

Needs a C++23 compiler, `cmake`, and `git` to build itself. To build *other* stuff, it just shells out to whatever that project needs (`make`, `cargo`, `go`, `pip`, `maven`, etc.) — it checks the right tool is actually installed first and tells you which one is missing instead of failing blind.

## Platform notes

- **Linux/BSD**: works as before, run with `sudo`.
- **macOS**: run without `sudo` — Homebrew (the system-package-manager fallback) refuses to run as root, so simple-make never asks for elevation here.
- **Windows**: run from an elevated terminal. `autotools`-based projects aren't buildable natively (autoconf/automake assume a POSIX shell) unless you're inside MSYS2/Cygwin/WSL; the Rust/Go builders skip the shell-rc `PATH` convenience Linux/macOS get and just tell you which folder to add to `PATH` yourself.

## License

MIT