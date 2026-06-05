# sparkd — TODO

## Planned

### IPC
- [ ] Today, most of the tools expect everything runs on the localhost, even if you can specify other addresses
      in the env. I think we might as well force everything to the localhost but the ui which can be served to
      any address.

### UI
- [ ] We should be able to convert a bad id to a good one with some rules (autocorrect button — later)

### Projects
- [ ] Make a decision regarding folder based projects
- [ ] Project file recovery strategies could be great (bad indents for example, or bad references)

### DMX Pro Backend
- [ ] Auto-detect Pro vs Open based on device response (requires hardware testing)

### Infrastructure ([spec](specs/ci-release.md))
- [ ] `make install` target with PREFIX=/opt/sparkd
- [ ] `scripts/package.sh` — local packaging (tar.gz for Linux, zip for Windows)
- [ ] `.github/workflows/ci.yml` — build + test on push, release on tag
- [ ] Cross-compile Windows from Linux with mingw-w64
- [ ] Cross-version sync check (consts.h version == package.json version)

### Packaging
- [ ] Maybe also package the ui and serve it with mongoose. We might have to change some things, but using cres_find(...) seems op.