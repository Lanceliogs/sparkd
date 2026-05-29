# sparkd — TODO

## Planned (v0.2)

### Basic REAPER integration
- [ ] Generate mappings from scenes names and triggers using a `spark-reaper --project [PATH]` utility tool
- [ ] Maybe allow a user to download the mapping for a project from the Editor page too. We could create
      a tiny reaper module in tools/reaper
- [ ] See what would be the cost of creating a VST like PAD communicating
      with sparkd directly for a direct engine control from REAPER.
      Being able to START/STOP and monitor the engine from REAPER would be sick.

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

### Infrastructure
- [ ] CI pipeline (GitHub Actions: build + test on Linux/Windows)
- [ ] Git tag + CHANGELOG for releases
- [ ] Packaging (at minimum a zip/tar of binaries + ui/dist)
- [ ] Cross-version sync check (consts.h version == package.json version)
