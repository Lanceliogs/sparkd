# Changelog

## v0.1.0 — 2026-05-29

First release of sparkd — a lightweight DMX lighting engine with MIDI triggers.

### Engine (sparkd)
- MIDI-triggered scene engine with gate/toggle modes, static and sequence scene types
- DMX output via Enttec Open (serial) and Enttec Pro (widget API) backends
- DMX auto-detection: `auto`, `auto:enttec`, `auto:eurolite` device resolution
- Reconnect affinity by serial number
- Strict YAML project parsing with detailed error reporting (line/column)
- Compile-time configurable limits via `-DSPARK_XXX=value`

### Editor UI (spark-ui)
- Full project lifecycle: new, open, save, save-as, close
- Fixture CRUD with template references (bank:fixture) and copy-from
- Scene CRUD with static values and sequence steps
- Hardware configuration (MIDI device/mode, DMX device/backend/refresh rate)
- Fixture bank management with multi-directory support
- File browser with history navigation and drive/places sidebar
- ID validation (inline feedback + backend enforcement)
- DMX channel occupancy display and overlap warnings
- Project validation: bad IDs, broken refs, address conflicts shown per-tab
- Toast notifications for all errors/warnings/successes
- Confirmation modals for destructive actions
- Unsaved changes guard (beforeunload + dirty state tracking)

### Live Page
- Pad grid with real-time scene state via WebSocket
- Start/stop/blackout engine controls with error feedback
- Disabled scenes visually dimmed but still triggerable
- Resolved MIDI device names and DMX port displayed in status footer
- Auto-reload scenes on WebSocket reconnect

### CLI Tools
- `spark-midi` — list MIDI devices
- `spark-serial` — list serial ports with VID/PID and auto-detection test
