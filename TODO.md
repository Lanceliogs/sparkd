# sparkd — TODO

## Planned (v0.2)

### Basic REAPER integration
- [X] Generate mappings from scenes names and triggers using a `spark-reaper --project [PATH]` utility tool
- [ ] Maybe allow a user to download the mapping for a project from the Editor page too. We could create
      a tiny reaper module in tools/reaper
- [X] See what would be the cost of creating a VST like PAD communicating
      with sparkd directly for a direct engine control from REAPER.
      Being able to START/STOP and monitor the engine from REAPER would be sick.

### sparkctl upgrade
- [X] Add `sparkctl daemon up` and `sparkctl daemon down` to control the lifecycle of sparkd
- [X] Add `sparkctl ui up` and `sparkctl ui down` to control the lifecycle of spark-ui
- [X] Add `sparkctl daemon status` and `sparkctl ui status`
- [X] Graceful shutdown via `POST /shutdown` on both sparkd and spark-ui
- [X] `/healthz` endpoint on spark-ui (sparkd already had one)
- [ ] Add `sparkctl ensure-path` to add the binaries folder to the PATH

### Linux packaging 
- [ ] Why not release a `sparkd-vX.X.X-amd64.deb` package really? 

### Windows minimal installer 
- [ ] Use [cres-c-resource-embedding](https://github.com/Lanceliogs/cres-c-resource-embedding) to create a tiny Windows package
- [ ] Maybe also package the ui and serve it with  mongoose. We might have to change some things, but using cres_find(...) seems op.

### Auth Token ([spec](specs/auth-token.md))
- [ ] Two tokens at startup: admin (full) + live (show control only)
- [ ] All `/api/*` and `/ws` routes require `Authorization: Bearer <token>`
- [ ] Permission levels: live token denied editor/browse/load-with-path routes
- [ ] Localhost meta tag injection for seamless local admin access
- [ ] Frontend token flow: meta → URL param → localStorage → manual input
- [ ] Frontend hides Editor tab for live-level tokens
- [ ] QR code modal: "Share Live Control" vs "Share Full Access"
- [ ] Token rotation endpoint (`POST /api/auth/rotate`, admin only)

## Planned (post v0.2)

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
