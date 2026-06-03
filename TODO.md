# sparkd — TODO

## Planned (v0.2)

### Basic REAPER integration
- [X] Generate mappings from scenes names and triggers using a `spark-reaper --project [PATH]` utility tool
- [X] See what would be the cost of creating a VST like PAD communicating
      with sparkd directly for a direct engine control from REAPER.
      Being able to START/STOP and monitor the engine from REAPER would be sick.

### sparkctl upgrade
- [X] Add `sparkctl daemon up` and `sparkctl daemon down` to control the lifecycle of sparkd
- [X] Add `sparkctl ui up` and `sparkctl ui down` to control the lifecycle of spark-ui
- [X] Add `sparkctl daemon status` and `sparkctl ui status`
- [X] Graceful shutdown via `POST /shutdown` on both sparkd and spark-ui
- [X] `/healthz` endpoint on spark-ui (sparkd already had one)

### Linux packaging
- [x] Why not release a `sparkd-vX.X.X-amd64.deb` package really?

### Windows minimal installer
- [x] Use [cres-c-resource-embedding](https://github.com/Lanceliogs/cres-c-resource-embedding) to create a tiny Windows package

### Auth Token ([spec](specs/auth-token.md))
- [X] Static `SPARK_AUTH_TOKEN` for infrastructure (sparkd, sparkctl, spark-ui management)
- [X] Dynamic share token for live access (generated at startup, shared via QR)
- [X] All `/api/*` and `/ws` routes require valid token
- [X] Role-based access: `SPARK_AUTH_TOKEN` = admin, share token = live
- [X] Live role denied editor/browse/load-with-path routes
- [X] Localhost meta tag injection for seamless local admin access
- [X] Frontend token flow: meta → URL param → localStorage → manual input
- [X] Frontend hides Editor tab for live role, hides Load Project button
- [X] QR code share modal (admin only)
- [X] Token rotation endpoint (`POST /api/auth/rotate`, admin only)
- [X] `GET /api/auth/role` for frontend role resolution
- [X] `SPARK_LAN_IFACE` CIDR-based LAN IP auto-detection for QR URLs
- [X] CORS headers on all `/api/*` responses (LAN access support)

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

### Packaging
- [ ] Maybe also package the ui and serve it with mongoose. We might have to change some things, but using cres_find(...) seems op.