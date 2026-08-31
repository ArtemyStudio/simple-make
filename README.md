# simple-make

Grab a random GitHub project, build it, and install it — all in one command. Written in C++23.
You give it a name or a URL. It figures out where the source is (either from a built-in database of 1000 popular projects, or via a live GitHub search by stars), downloads it, detects the build system, and compiles it. 
Give it multiple packages — it builds them in parallel without nuking your CPU. Oh, and it actually tracks installed files so you can cleanly uninstall stuff built from source.

## Quick start

```bash
git clone https://github.com/ArtemyStudio/simple-make.git
cd simple-make
cmake -B build && cmake --build build -j$(nproc)
sudo cp build/simple-make /usr/local/bin/simple-make
sudo chmod +x /usr/local/bin/simple-make
```
*(Dependencies like `cpr` and `nlohmann/json` are fetched automatically by CMake)*

## Usage

```bash
sudo simple-make ripgrep fd bat          # build several packages in parallel
sudo simple-make https://github.com/...  # install directly from a URL
sudo simple-make -s cmake                # just search, don't install
sudo simple-make -r ripgrep              # cleanly remove
sudo simple-make --setup-config          # interactive config wizard
```

## Config

Run `sudo simple-make --setup-config` once to generate `~/.config/simple-make/simple-make.conf`. 

It lets you set the install prefix (`/usr/local` by default), toggle auto-confirm, keep source files after building, and add a GitHub token (to bump the API rate limit from 60 to 5000 requests/hour). You can also set `home_path` if `sudo` messes up directory detection.

## How it works

1. **Resolves** the name to a URL (database -> GitHub API -> raw URL).
2. **Downloads** it into `~/.simple-make/sources/<package>`.
3. **Detects** the build system (`CMakeLists.txt`, `Cargo.toml`, `go.mod`, `meson.build`, etc.).
4. **Builds & installs**, logging everything to `~/.simple-make/logs/<package>.log`. Compile jobs are split evenly across parallel builds so your machine doesn't choke.
5. **Tracks** every file that landed in the install prefix (`~/.simple-make/installed/<package>.list`) so `-r` can wipe them cleanly.

## Dependencies

Needs a C++23 compiler, `cmake`, and `git` to build itself. To build *other* stuff, it just shells out to whatever that project needs (`make`, `cargo`, `go`, `pip`, `maven`, etc.).

## License

MIT