# Contributing

Thanks for taking a look. Bug reports and small, focused pull requests are both
very welcome.

## Building

```bash
git clone https://github.com/ArtemyStudio/simple-make.git
cd simple-make
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
./build/simple-make --help
```

`cpr` and `nlohmann/json` are used from your system packages when present, and
fetched (pinned) by CMake otherwise. Needs a C++23 compiler — GCC 14+ or Clang
18+.

Install your own build without touching the system copy:

```bash
cmake --install build --prefix ~/.local
```

## Before opening a pull request

- Build cleanly with no new warnings.
- Try the paths you touched by hand: `--help`, `-s <name>`, an install into a
  throwaway prefix (`HOME=/tmp/smtest ./build/simple-make <pkg>` is an easy way
  to keep your real `~/.simple-make` out of it).
- Keep the diff to one topic.

## Security-sensitive code

`src/platform.cpp` and `src/build.cpp` decide what runs with which privileges
and what gets interpolated into a shell. If you change either, please read
[SECURITY.md](SECURITY.md) first and keep these invariants:

- Build, configure and extraction commands go through
  `platform::wrap_unprivileged()`.
- Anything dynamic that reaches a shell string goes through
  `platform::shell_quote()`.
- Ownership changes use `lchown` and only on paths simple-make created.

## Style

Match the surrounding code: 4 spaces, `snake_case` for functions and variables,
`std::format`/`std::println` over streams, early returns, and no comments
unless the *why* isn't obvious from the code.

## Adding a build system

`get_builder()` in `src/build.cpp` detects one, `required_tools()` names the
binary it needs, and the `switch` in `build_pkg()` builds the command. New
builders need both the staging (`DESTDIR` or an explicit target directory) and
the direct path, so a system-wide prefix keeps working without root during the
build.
