# CI and Release Pipeline

## Overview

GitHub Actions workflow that builds, tests, and packages sparkd for Linux and Windows on every push to `main` (build+test only) and on tag push `v*` (build+test+package+release).

## Architecture

Both builds run on `ubuntu-latest`. Windows artifacts are cross-compiled with `mingw-w64`.

```
ubuntu-latest runner
├── Linux native: gcc, libasound2-dev
│   ├── make clean && make && make tools
│   ├── make test
│   ├── cd ui && npm ci && npm run build
│   └── Package: sparkd-vX.Y.Z-linux-x64.tar.gz
│
└── Windows cross-compile: x86_64-w64-mingw32-gcc
    ├── make CC=x86_64-w64-mingw32-gcc OS=Windows_NT
    ├── (tests skipped — can't run .exe on Linux)
    ├── UI: same npm build
    └── Package: sparkd-vX.Y.Z-win64.zip
```

## HW Tests in CI

Tests are container-safe:
- MIDI tests skip if `/dev/snd/seq` is absent
- Serial enum tests verify VID/PID logic without opening ports
- DMX tests use the dummy backend

## Release Job (tag only)

- Downloads artifacts from both build steps
- Creates a GitHub Release via `gh release create $TAG`
- Uploads both archives

## Package Contents

```
sparkd-vX.Y.Z-{platform}/
├── sparkd(.exe)
├── spark-ui(.exe)
├── spark-midi(.exe)
├── spark-serial(.exe)
├── sparkctl(.exe)
└── ui/
    └── dist/
        └── (built frontend assets)
```

## Makefile Changes Needed

- Top-level `Makefile`: add cross-compile support via `CC` and `OS` override
- Ensure `LDFLAGS` picks up mingw libs when `OS=Windows_NT` even on Linux host
- Add `install` target with `PREFIX=/opt/sparkd` default

## Implementation Steps

1. Add `install` target to `Makefile`
2. Create `scripts/package.sh` — packages build output into tar.gz/zip
3. Create `.github/workflows/ci.yml`
4. Test with a dry-run push to a branch before tagging
