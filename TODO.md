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

## Planned (post v0.1.0)

### UI Error Feedback
- [ ] Toast/notification system for failed API calls (start, stop, save, delete)
- [ ] Loading spinners during async operations (file browse, project open/save)

### Safety Guards
- [ ] Confirmation modal before destructive deletes (fixtures, scenes)
- [ ] `beforeunload` browser guard for unsaved editor changes
- [ ] Hardware dirty state included in project dirty guard

### Live Page
- [ ] Hide or grey out disabled scenes on pad grid
- [ ] WebSocket JSON.parse error handling
- [ ] User feedback on failed engine operations (start/stop/blackout)
- [ ] Scene reload on WebSocket reconnect

### Editor
- [ ] Error toasts for failed save/delete/open/create operations
- [ ] Loading state during refresh and file browser navigation
- [ ] Validation feedback on empty bank ID/directory

### DMX Pro Backend
- [x] Enttec DMX USB Pro protocol (widget API over serial)
- [ ] Auto-detect Pro vs Open based on device response (requires hardware testing)

### Infrastructure
- [ ] CI pipeline (GitHub Actions: build + test on Linux/Windows)
- [ ] Git tag + CHANGELOG for releases
- [ ] Packaging (at minimum a zip/tar of binaries + ui/dist)
- [ ] Cross-version sync check (consts.h version == package.json version)

### Open Questions
- [ ] The routes can activate disabled scenes. Should we keep it that way?