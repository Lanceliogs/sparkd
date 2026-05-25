# sparkd v0.1.0 — TODO

## In Progress

### DMX Auto-Detection
- [x] Split `serial.c` into `serial_posix.c`, `serial_win32.c`, `serial_common.c`
- [x] VID/PID bank of known DMX USB devices (Enttec, Eurolite, FTDI generic)
- [x] `spark_serial_enumerate()` — platform-specific USB-serial enumeration
- [x] `spark_serial_find_dmx()` — auto-resolve device by manufacturer tag
- [x] Engine integration: `dmx.device: auto` / `auto:enttec` config values
- [x] Reconnect affinity by serial number
- [x] Unit tests

### Project file rules
- [x] Fixtures and scenes id should be case insensitive
- [x] No spaces too, only [a-z0-9-] chars. No dots obviously as it's our id.channel delimiter.
- [x] This should be enforced in the editor and the CRUD should report an error (see next section).
- [x] Strict YAML parsing: unknown keys rejected with line/column error (editor + engine)
- [x] Parse error details surfaced to the UI (toast + detail modal)

### UI Error Feedback
- [x] Toast/notification system for failed API calls (start, stop, save, delete)
- [x] Loading spinners during async operations (project refresh)
- [ ] Loading state during file browser navigation

### Safety Guards
- [x] Confirmation modal before destructive deletes (fixtures, scenes)
- [x] `beforeunload` browser guard for unsaved editor changes
- [x] Hardware dirty state included in project dirty guard

### Live Page
- [x] Mark disabled scenes on pad grid, but keep them usable from the UI
- [x] WebSocket JSON.parse error handling
- [x] User feedback on failed engine operations (start/stop/blackout)
- [x] Scene reload on WebSocket reconnect
- [ ] Display resolved DMX device name next to connection state
- [ ] Display full MIDI port name (not just pattern) — e.g. "Launchpad Mini (pattern)"

### Editor
- [x] Error toasts for failed save/delete/open/create operations
- [x] Mark disabled scenes here too
- [x] Loading state during refresh
- [ ] Validation feedback on empty bank ID/directory
- [ ] Display DMX channel occupancy in fixture form (edit card)
- [ ] Template and copy-from: combobox selector + reference validator
- [ ] New button to create a new empty project


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
- [ ] Project file recovery strategies could be great (bad indents for example, or bad references). 

### DMX Pro Backend
- [x] Enttec DMX USB Pro protocol (widget API over serial)
- [ ] Auto-detect Pro vs Open based on device response (requires hardware testing)

### Infrastructure
- [ ] CI pipeline (GitHub Actions: build + test on Linux/Windows)
- [ ] Git tag + CHANGELOG for releases
- [ ] Packaging (at minimum a zip/tar of binaries + ui/dist)
- [ ] Cross-version sync check (consts.h version == package.json version)

## Open Questions
- [x] The routes can activate disabled scenes. Should we keep it that way? Yes for now.
