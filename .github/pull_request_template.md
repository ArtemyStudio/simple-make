## What this changes

## How it was tested

- [ ] Builds with no new warnings
- [ ] Tried the affected paths by hand

## If this touches privileges or shell commands

- [ ] Build/configure/extract commands still go through `platform::wrap_unprivileged()`
- [ ] Dynamic values reaching a shell go through `platform::shell_quote()`
- [ ] Ownership changes use `lchown` and only on paths simple-make created
