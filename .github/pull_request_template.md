## What this changes

<!-- One or two sentences. Link the issue if there is one. -->

## How it was tested

<!-- Commands you actually ran, e.g. a build into a throwaway prefix:
     HOME=/tmp/smtest ./build/simple-make <pkg> -->

- [ ] Builds with no new warnings
- [ ] Tried the affected paths by hand

## If this touches privileges or shell commands

- [ ] Build/configure/extract commands still go through `platform::wrap_unprivileged()`
- [ ] Dynamic values reaching a shell go through `platform::shell_quote()`
- [ ] Ownership changes use `lchown` and only on paths simple-make created
