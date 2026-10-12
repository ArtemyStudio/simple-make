# Security Policy

## Reporting a vulnerability

Please report security issues privately via [GitHub's private vulnerability
reporting](https://github.com/ArtemyStudio/simple-make/security/advisories/new)
rather than opening a public issue. If that isn't available to you, email
artemystudio.co@gmail.com.

Please include what you were running, what happened, and a reproduction if you
have one. You'll get a first reply within a few days.

## Threat model

simple-make downloads third-party source code and runs that project's own build
system. **Any package you install can execute arbitrary code during its build.**
That is inherent to building from source — simple-make does not sandbox or vet
build scripts, and it does not try to. Only install things you'd be willing to
`git clone && make` by hand.

What simple-make does guarantee:

- **Builds never run as root.** Cloning, extraction, configure and compile run
  as the invoking user. When started via `sudo`, the build is re-dropped to
  `SUDO_USER`; if privileges cannot be dropped, simple-make warns instead of
  quietly building as root.
- **Root is only used for one step.** With the default prefix (`~/.local`),
  root is never needed at all. With a system-wide prefix, the build lands in a
  staging directory as you, and a single elevated copy places the result.
- **No unquoted shell interpolation.** Package names, URLs and paths are
  shell-quoted, and package names are restricted to
  `[A-Za-z0-9._+-]`.
- **Ownership fixes never follow symlinks.** `lchown` is used, and
  simple-make only adjusts paths it created itself — never files inside a
  downloaded repository.

Known limits, by design:

- Build scripts are not sandboxed; they run with your user's full privileges
  and network access.
- Packages resolved by live search are matched by name and stars. An exact name
  match wins, and ties ask you to pick, but nothing verifies that a repository
  is the "official" one. Prefer a URL or your system package manager when it
  matters.
- Installing a package does not verify signatures or checksums of upstream
  source.

## Supported versions

Fixes land on `master` and go out in the next release. Only the latest release
is supported.
