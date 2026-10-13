# simple-make

**simple-make is a cross-platform CLI that installs any project from source with one command.**
Give it a name or a URL — it finds the source, picks the right build system, compiles it, and
tracks every installed file so you can remove it again. Written in C++23, runs on Linux, macOS,
BSD and Windows.

```bash
simple-make ripgrep          # finds it, builds it, installs it
simple-make -r ripgrep       # removes exactly what it installed
```

It resolves a name in three steps: your system package manager first
(apt/dnf/pacman/zypper/apk/brew/pkg/winget/choco), then a built-in database of 1000 popular
projects, then a live search across GitHub, GitLab and Codeberg. It detects CMake, Meson, Ninja,
Make, Autotools, Cargo, Go, pip, Maven and Gradle on its own. Multiple packages build in parallel
without saturating your CPU, and **builds never run as root** — see
[Privilege model](#privilege-model).

> Not a distro package manager and not trying to be one. If you want curated recipes,
> reproducibility and rollbacks, use Nix or Portage — see
> [vs. Nix / Portage](#vs-nix--portage).

## Quick start

```bash
git clone https://github.com/ArtemyStudio/simple-make.git
cd simple-make
cmake -B build && cmake --build build -j$(nproc)
sudo cmake --install build
```
*(Dependencies like `cpr` and `nlohmann/json` are fetched automatically by CMake)*

On Arch/Artix, grab `PKGBUILD` from the [latest release](https://github.com/ArtemyStudio/simple-make/releases/latest) and build it as a proper package from a separate directory:

```bash
mkdir simple-make-pkg && cd simple-make-pkg
curl -LO https://github.com/ArtemyStudio/simple-make/releases/latest/download/PKGBUILD
makepkg -si
```

## Usage

```bash
simple-make ripgrep fd bat          # build several packages in parallel
simple-make https://github.com/...  # install directly from a URL
simple-make -s cmake                # just search, don't install
simple-make -r ripgrep              # cleanly remove
simple-make -u                      # rebuild every installed package against latest upstream
simple-make -u ripgrep              # rebuild just that one
simple-make --self-update           # check simple-make's own releases, and rebuild itself if newer
simple-make --setup-config          # interactive config wizard
```

None of this needs `sudo` with the default install prefix (`~/.local`). Point `install_prefix` at a system-wide path instead (`/usr/local`, `/opt`, ...) and run the same commands with `sudo` in front — on Windows, from an elevated terminal instead. macOS is the one exception that stays unprivileged either way: Homebrew itself refuses to run as root, so simple-make never elevates there, even for a system-wide prefix.

## Config

Run `sudo simple-make --setup-config` once to generate `~/.config/simple-make/simple-make.conf` (`%USERPROFILE%\.config\simple-make\simple-make.conf` on Windows).

It lets you set the install prefix, toggle auto-confirm, keep source files after building, add a GitHub/GitLab token (to raise their anonymous search rate limits), toggle whether the system package manager is tried before building from source, and toggle a startup check for simple-make's own updates. You can also set `home_path` if `sudo` messes up directory detection, and `search_providers` to change which of GitHub/GitLab/Codeberg get queried (and in what order).

## How it works

1. **Checks** whether it's already installed — by simple-make itself, by your system package manager, or just a same-named binary already on `PATH` — and stops with a clear error instead of silently rebuilding over it. Use `-u` to update an existing install instead, or `-r` to remove it first.
2. **Resolves** the name: your system package manager first, then the built-in database, then a live search across every provider in `search_providers` (an exact name match always wins over star count; a genuine tie is the only time you're asked to pick).
3. **Downloads** it into `~/.simple-make/sources/<package>` (skipped entirely when the system package manager already has it).
4. **Detects** the build system (`CMakeLists.txt`, `Cargo.toml`, `go.mod`, `meson.build`, etc.), warning you if more than one is present in the same repo.
5. **Builds & installs**, logging everything to `~/.simple-make/logs/<package>.log` and printing the log's tail straight to the console if the build fails. Compile jobs are split evenly across parallel builds so your machine doesn't choke.
6. **Tracks** every file that landed in the install prefix (`~/.simple-make/installed/<package>.list`) so `-r` can wipe them cleanly, plus the exact source URL it resolved (`~/.simple-make/installed/<package>.url`) so `-u`/`--self-update` rebuilds from that same source instead of re-resolving the name and risking a different, same-named repo. Neither of these touches your own `simple-make.conf` or any other app's settings — an update only ever replaces what that package's own install step put in the prefix.

## Dependencies

Needs a C++23 compiler, `cmake`, and `git` to build itself. To build *other* stuff, it just shells out to whatever that project needs (`make`, `cargo`, `go`, `pip`, `maven`, etc.) — it checks the right tool is actually installed first and tells you which one is missing instead of failing blind.

## Privilege model

Resolving, downloading, and *building* a package never needs root, on any platform — configure and compile steps run as you, because that's where a malicious `CMakeLists.txt`, `build.rs`, `setup.py`, or `Makefile` would actually execute arbitrary code, and root has no business being anywhere near that. Elevation is only ever used for the last step — copying already-built files into a prefix you don't own.

With the default prefix (`~/.local`), that step doesn't need root either, so the whole pipeline runs as you end to end. Point `install_prefix` at something system-wide (`/usr/local`, `/opt`, ...) and simple-make builds into a staging directory as you, then uses a single elevated copy to place the result — root never touches the build.

## Upgrading from root-run installs

If you used simple-make through `sudo` before, `~/.simple-make` is owned by root and a normal-user run will say `cannot write to ~/.simple-make`. Fix it once:

```bash
sudo chown -R "$(id -un)": ~/.simple-make
```

Packages recorded before the `.url` source file existed are re-resolved by name on `-u`.

## Platform notes

- **Linux/BSD**: run with `sudo` if you've pointed `install_prefix` outside your home; not needed for the default.
- **macOS**: never run with `sudo` — Homebrew (the system-package-manager fallback) refuses to run as root, so simple-make won't ask for elevation here even for a system-wide prefix.
- **Windows**: run from an elevated terminal. `autotools`-based projects aren't buildable natively (autoconf/automake assume a POSIX shell) unless you're inside MSYS2/Cygwin/WSL; the Rust/Go builders skip the shell-rc `PATH` convenience Linux/macOS get and just tell you which folder to add to `PATH` yourself.

## vs. Nix / Portage

Nix and Portage are full package managers built around curated, reviewed build recipes (Nix derivations, Gentoo ebuilds) — sandboxed and often reproducible builds, a real dependency graph, atomic upgrades/rollbacks, USE-flag-style customization. They manage your whole system, and someone has already vetted how each package is supposed to build.

simple-make has none of that. It has no recipes of its own — it resolves a name to a repo and shells out to whatever build system that repo *already* ships (CMake, Cargo, pip, a bare Makefile, ...), exactly as upstream wrote it, unsandboxed, with your system package manager tried first as a shortcut. There's no dependency graph, no reproducibility guarantee, no rollback — just "build it the normal way, and remember what landed in the prefix so `-r` can take it back out."

It's for "I want this one GitHub project installed right now" without hand-cloning it and reading its README. It's not a substitute for Nix, Portage, or your system package manager for anything you need to still be working the same way in a year.

## FAQ

**What is simple-make?**
A single command-line tool that installs software from source. You give it a package name or a
Git/tarball URL; it resolves where the source lives, detects the project's own build system
(CMake, Meson, Ninja, Make, Autotools, Cargo, Go, pip, Maven, Gradle), builds it, installs it into
a prefix you choose, and records what it installed so `simple-make -r <name>` can remove it.

**How is it different from `make install`?**
`make install` assumes you already cloned the right repository, know which build system it uses,
and are willing to leave untracked files all over your prefix. simple-make does the finding, picks
the build system, keeps the build out of root's hands, and keeps a file list for clean removal.

**Does it replace apt / pacman / dnf / Homebrew?**
No. It *uses* them: if your system package manager already has the thing, simple-make installs it
from there and stops. It only builds from source when the package isn't packaged for you.

**Is it safe to run?**
Building any third-party project runs that project's build scripts, so only install things you'd
be willing to build by hand. simple-make never runs those scripts as root, shell-quotes everything
it passes to a shell, and never follows symlinks when fixing ownership. Details and known limits:
[SECURITY.md](SECURITY.md).

**Do I need sudo?**
Not with the default prefix (`~/.local`). Only a system-wide `install_prefix` like `/usr/local`
needs elevation, and then only for the final copy step.

**Which platforms work?**
Linux, macOS, BSD and Windows. The pipeline is the same everywhere; see
[Platform notes](#platform-notes) for the per-OS caveats.

**How do I update or uninstall?**
`simple-make -u` rebuilds everything it installed against the latest upstream (`-u <name>` for
one), and `simple-make -r <name>` removes exactly the files that package installed.

**Where does it put things?**
Binaries go to your `install_prefix` (`~/.local` by default). Its own state lives in
`~/.simple-make` (`sources`, `installed`, `logs`) and its config in
`~/.config/simple-make/simple-make.conf`.

## License

MIT
