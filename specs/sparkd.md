# sparkd Specification

## 1. Project Goal

Build a focused, show-safe MIDI-to-DMX lighting engine intended to replace heavier lighting tools for a specific workflow.

The system is a local C daemon (`sparkd`) that:

- Receives MIDI notes and controller events through a single input port.
- Maps MIDI triggers to lighting scenes or direct DMX channel actions.
- Outputs DMX continuously through an ENTTEC-compatible interface.
- Recovers automatically if the DMX interface is unplugged and reconnected.
- Exposes a local HTTP API and WebSocket event stream.
- Serves a Svelte web UI for configuration, scene testing, and monitoring.
- Stores a named fixture patch so scenes and mappings use human-readable names instead of raw DMX addresses.

The guiding principle is:

> MIDI input and show state stay alive even when the DMX output temporarily disappears.

One project equals one daemon instance, one MIDI input, one DMX universe, one DMX output.

---

## 2. Architecture

```text
MIDI controller / DAW / sequencer
        |
        v
   MIDI routing (ALSA / PipeWire / virtual port)
        |
        v
+------------------------------+
|           sparkd             |
|------------------------------|
| MIDI input backend           |
| Mapping engine               |
| Scene state                  |
| Fixture patch resolver       |
| DMX frame renderer           |
| Reconnectable DMX backend    |
| Safety / heartbeat           |
| Embedded HTTP + WebSocket    |
+------------------------------+
        |
        v
ENTTEC Open DMX USB
or ENTTEC DMX USB Pro
or dummy backend
        |
        v
DMX universe (512 channels)
```

The daemon owns the live show state.

The HTTP UI, CLI (`sparkctl`), and REAPER are all clients. They do not own lighting state.

---

## 3. Core Design Decisions

### 3.1 Naming Convention

User-facing keys use **kebab-case**:

```text
YAML config keys
HTTP API JSON payloads and responses
WebSocket event fields
CLI long options
REAPER cue-map fields
```

C internals use **snake_case**:

```text
structs, functions, variables
```

### 3.2 Configuration Format

Project files use **YAML** with `.spark.yaml` extension.

YAML is chosen because:

- Human-editable with comments.
- Natural for the project's nested structure.
- The daemon validates strictly at load time.

### 3.3 MIDI Backend

The daemon uses **portmidi** as the cross-platform MIDI backend.

Two port ownership modes:

```text
create-virtual
  daemon creates the MIDI input endpoint
  recommended for Linux

open-existing
  daemon opens an already-existing MIDI input
  recommended for Windows or systems with external virtual MIDI tools
```

### 3.4 Named Fixture Patch

DMX addresses are declared only in fixtures. Scenes and mappings use logical target names:

```text
leftpar.dimmer
leftpar.red
```

Fixtures own their expanded channel definitions. Fixture templates are prefill helpers only.

### 3.5 Reconnectable DMX Backend

DMX output is a replaceable, reconnectable sink. The mapping engine and current DMX frame remain alive even if hardware disappears. When the interface reconnects, the daemon immediately sends the current rendered frame.

### 3.6 Threading Model

Version 1 uses two execution contexts:

```text
main thread:
  HTTP/WebSocket request handling
  MIDI input processing
  logical stage state updates
  safety and heartbeat updates
  semantic event generation

DMX output thread:
  DMX serial device ownership
  DMX frame timing at configured refresh rate
  DMX reconnect attempts
  DMX write error handling
```

The stage renderer has one mutex. The lock is held only for short CPU work (applying events, rendering a frame). No I/O occurs while holding the lock.

### 3.7 Show Safety

The UI is not show-critical. If the browser disconnects, the daemon continues running MIDI input, rendering, and DMX output unchanged.

---

## 4. Runtime Lifecycle

### 4.1 Startup

```text
1. Parse command-line arguments.
2. Initialize logging.
3. Load project YAML.
4. Validate project.
5. Resolve fixture/channel targets to DMX indexes.
6. Initialize stage renderer.
7. Initialize MIDI input backend.
8. Initialize HTTP and WebSocket server.
9. Initialize DMX output backend.
10. Start DMX output thread.
11. Enter main loop.
```

If startup project validation fails, the daemon exits with a clear error.

### 4.2 Main Loop

The main loop is event-driven:

```text
while running:
  poll HTTP and MIDI inputs
  handle ready HTTP requests
  drain pending MIDI messages
  apply due safety/heartbeat checks
  dispatch WebSocket events
```

The main loop does not send DMX frames.

### 4.3 DMX Output Thread

```text
while running:
  now = monotonic time
  lock stage
  render complete 512-channel universe for now
  unlock stage
  send complete frame to backend
  handle send errors / reconnect if needed
  sleep until next frame tick
```

At 40 Hz the output interval is 25 ms.

### 4.4 Shutdown

```text
1. Stop accepting HTTP/WebSocket requests.
2. Stop MIDI input.
3. Signal DMX output thread to stop.
4. Send blackout frame if configured.
5. Close DMX backend.
6. Exit.
```

Shutdown sources: Ctrl+C, SIGTERM.

### 4.5 Error and Degraded States

The daemon does not exit for runtime failures. They become degraded states:

- **DMX output failure**: status -> disconnected, output thread retries device, stage state remains alive.
- **MIDI heartbeat timeout**: safety state -> degraded-hold, hazard outputs forced safe.
- **MIDI backend failure**: MIDI status -> failed, DMX output continues.
- **Invalid live reload**: new project rejected, previous valid project remains active.

---

## 5. MIDI Input

### 5.1 Port Ownership Modes

**create-virtual** (recommended Linux):

```yaml
midi:
  backend: portmidi
  mode: create-virtual
  port-name: spark
```

The daemon creates and owns the virtual MIDI endpoint. It appears when the daemon starts and disappears when it exits.

**open-existing** (recommended Windows):

```yaml
midi:
  backend: portmidi
  mode: open-existing
  input-name: spark
  reconnect:
    enabled: true
    retry-ms: 1000
```

The daemon opens an already-existing port created by an external virtual MIDI tool.

### 5.2 Supported MIDI Events

Version 1:

- Note On
- Note Off
- Note On with velocity 0 (treated as Note Off)
- Control Change (CC)

Future: program change, pitch bend, aftertouch, clock/transport.

### 5.3 Normalized MIDI Event

The mapping engine uses a backend-independent event:

```c
typedef enum {
    MIDI_EVENT_NOTE_ON,
    MIDI_EVENT_NOTE_OFF,
    MIDI_EVENT_CC
} midi_event_type_t;

typedef struct {
    midi_event_type_t type;
    uint8_t channel;   // 0-15 internally
    uint8_t note;      // 0-127
    uint8_t velocity;  // 0-127
    uint8_t cc;        // 0-127
    uint8_t value;     // 0-127
} midi_event_t;
```

User-facing channel numbers are 1-16. Internal channel numbers are 0-15.

### 5.4 MIDI Reconnect

Both modes are reconnectable:

- `create-virtual`: if backend fails, recreate virtual port.
- `open-existing`: if port is missing, scan available inputs by name, reopen when found.

---

## 6. DMX Output

### 6.1 DMX Frame

The daemon maintains a 512-byte DMX frame:

```c
uint8_t dmx_frame[512];
```

User-facing channels are 1-512. Internal indexes are 0-511.

### 6.2 Backend Interface

All DMX backends implement the same interface:

```c
typedef struct dmx_backend dmx_backend_t;

typedef struct {
    int  (*open)(dmx_backend_t *backend);
    void (*close)(dmx_backend_t *backend);
    int  (*send_frame)(dmx_backend_t *backend, const uint8_t frame[512]);
    int  (*is_connected)(dmx_backend_t *backend);
} dmx_backend_ops_t;
```

### 6.3 Pro Packet Serial Backend

For Eurolite/ENTTEC Pro-style devices. Uses a framed serial protocol:

```text
0x7E 0x06 length-lsb length-msb 0x00 <512 bytes> 0xE7
```

Where length = 513 (start code + 512 channels).

The Pro backend is preferred for show safety because the hardware handles DMX timing internally.

### 6.4 Open DMX Backend

For ENTTEC Open DMX USB / FTDI-style devices. Timing-sensitive because the host generates the DMX stream:

```text
Set serial to 250000 baud, 8N2.
Generate break (ioctl TIOCSBRK, ~100 us).
Generate mark-after-break (~12 us).
Write start code 0x00 + 512 channel bytes.
Repeat continuously.
```

### 6.5 Dummy Backend

For development without hardware:

- Accepts frames.
- Reports always connected.
- Optionally logs changed channels.

### 6.6 Reconnection State Machine

```text
DISCONNECTED -> CONNECTING -> CONNECTED -> ERROR -> DISCONNECTED
```

```c
typedef enum {
    DMX_STATE_DISCONNECTED,
    DMX_STATE_CONNECTING,
    DMX_STATE_CONNECTED,
    DMX_STATE_ERROR
} dmx_state_t;
```

On disconnect: close fd, keep MIDI/HTTP/scene state alive, keep rendering frames internally, retry opening device periodically.

On reconnect: open device, configure backend, send current rendered frame immediately, continue normal refresh.

### 6.7 Stable Device Paths

Prefer stable paths:

```text
/dev/serial/by-id/usb-ENTTEC_DMX_USB_PRO_XXXXXXXX-if00-port0
```

With fallback globs:

```yaml
device:
  path: auto
  fallback-glob:
    - /dev/serial/by-id/*ENTTEC*
    - /dev/serial/by-id/*FTDI*
    - /dev/ttyUSB*
```

---

## 7. Configuration Format

### 7.1 File Format

Project files use YAML with extension `.spark.yaml`.

Example: `my-show.spark.yaml`

Directory projects use `project.yaml` as the manifest.

### 7.2 One-File Mode

All sections in one file:

```yaml
format:
  name: spark-project
  version: 1

app:
  name: my-show

http: {}
midi: {}
dmx: {}
fixture-templates: []
fixtures: []
scenes: []
mappings: []
safety: {}
reaper: {}
ui: {}
```

### 7.3 Directory Mode

```text
my-show/
  project.yaml
  fixtures.yaml
  scenes.yaml
  mappings.yaml
  safety.yaml
  ...
```

`project.yaml`:

```yaml
format:
  name: spark-project
  version: 1

app:
  name: my-show

includes:
  fixtures: fixtures.yaml
  scenes: scenes.yaml
  mappings: mappings.yaml
  safety: safety.yaml
```

Include paths are relative to project root. A section must not be both inline and included.

### 7.4 Top-Level Sections

```text
format          project format identifier
app             project identity
http            HTTP server config
midi            MIDI input config
dmx             DMX output config
fixture-templates  reusable channel layout helpers
fixtures        physical lights with expanded channels
scenes          renderable lighting states
mappings        MIDI trigger to action bindings
safety          heartbeat surveillance and safety classes
reaper          REAPER export config
ui              UI preferences (scene pad groups, stage view)
```

### 7.5 HTTP Section

```yaml
http:
  bind: 127.0.0.1
  port: 8080
  ui-root: ui
```

### 7.6 MIDI Section

```yaml
midi:
  backend: portmidi
  mode: create-virtual
  port-name: spark
  reconnect:
    enabled: true
    retry-ms: 1000
```

### 7.7 DMX Section

```yaml
dmx:
  backend: pro-packet-serial
  preset: eurolite-usb-dmx512-pro-mk2
  device:
    path: auto
    fallback-glob:
      - /dev/serial/by-id/*Eurolite*
      - /dev/serial/by-id/*FTDI*
      - /dev/ttyUSB*
  serial:
    baud: 250000
    data-bits: 8
    stop-bits: 2
    parity: none
  refresh-rate-hz: 40
  reconnect:
    enabled: true
    retry-ms: 1000
    restore-frame: true
  safety:
    blackout-on-exit: true
    blackout-on-disconnect: false
```

### 7.8 Save Behavior

The UI preserves the storage mode:

- Loaded as one-file: save as one file.
- Loaded as directory: save changed sections to section files.

---

## 8. Fixture Templates and Fixtures

### 8.1 Fixture Templates

Templates are **prefill helpers**, not runtime dependencies. When imported in the UI, their channel definitions are copied into the fixture. The fixture becomes the runtime source of truth.

```yaml
fixture-templates:
  - id: rgb-par-5ch
    name: RGB PAR 5CH
    description: Generic RGB PAR with dimmer, RGB, and strobe.
    channel-count: 5
    channels:
      - name: dimmer
        offset: 0
        kind: dimmer
        default: 0
        merge: htp
        safety-class: persistent
        safe-value: 0
      - name: red
        offset: 1
        kind: color-red
        default: 0
        merge: htp
        safety-class: persistent
        safe-value: 0
      - name: green
        offset: 2
        kind: color-green
        default: 0
        merge: htp
        safety-class: persistent
        safe-value: 0
      - name: blue
        offset: 3
        kind: color-blue
        default: 0
        merge: htp
        safety-class: persistent
        safe-value: 0
      - name: strobe
        offset: 4
        kind: strobe
        default: 0
        merge: ltp
        safety-class: hazard
        safe-value: 0
```

### 8.2 Fixtures

A fixture represents one physical light and owns its expanded channel list:

```yaml
fixtures:
  - id: leftpar
    name: Left PAR
    description: Front-left RGB PAR in 5-channel mode.
    template-source: rgb-par-5ch
    start-address: 1
    channel-count: 5
    position:
      x: 120
      y: 320
    channels:
      - name: dimmer
        offset: 0
        kind: dimmer
        default: 0
        merge: htp
        safety-class: persistent
        safe-value: 0
      - name: red
        offset: 1
        kind: color-red
        default: 0
        merge: htp
        safety-class: persistent
        safe-value: 0
      - name: green
        offset: 2
        kind: color-green
        default: 0
        merge: htp
        safety-class: persistent
        safe-value: 0
      - name: blue
        offset: 3
        kind: color-blue
        default: 0
        merge: htp
        safety-class: persistent
        safe-value: 0
      - name: strobe
        offset: 4
        kind: strobe
        default: 0
        merge: ltp
        safety-class: hazard
        safe-value: 0
```

### 8.3 Channel Fields

```text
name            used in target syntax: leftpar.strobe
description     optional UI note
offset          zero-based offset inside the fixture
kind            semantic: dimmer, color-red, color-green, color-blue, strobe, pan, tilt, gobo, mode, speed, custom
default         value when no scene controls this channel
min / max       optional allowed range, default 0-255
merge           htp, ltp, or priority
safety-class    persistent, temporary, or hazard
safe-value      value forced during safety failure
```

### 8.4 Target Names

Scenes reference fixture channels using:

```text
fixture-id.channel-name
```

Resolution rule:

```text
dmx-address = fixture.start-address + channel.offset
```

`start-address` is 1-based for users. C internals convert to 0-based.

---

## 9. Scenes

### 9.1 Scene Types

Version 1 supports:

```text
static      fixed target values
sequence    timed steps with transitions
```

A blink/strobe effect is represented as a two-step looping sequence with hold transitions.

### 9.2 Static Scene

```yaml
scenes:
  - id: red-wash
    name: Red Wash
    type: static
    safety-class: persistent
    priority: 10
    values:
      leftpar.dimmer: 220
      leftpar.red: 255
      leftpar.green: 0
      leftpar.blue: 0
```

### 9.3 Sequence Scene

```yaml
scenes:
  - id: color-cycle
    name: Color Cycle
    type: sequence
    safety-class: persistent
    priority: 10
    phase: start-on-activation
    loop: true
    steps:
      - duration-ms: 1000
        transition: linear
        values:
          leftpar.red: 255
          leftpar.green: 0
          leftpar.blue: 0
      - duration-ms: 1000
        transition: linear
        values:
          leftpar.red: 0
          leftpar.green: 255
          leftpar.blue: 0
      - duration-ms: 1000
        transition: linear
        values:
          leftpar.red: 0
          leftpar.green: 0
          leftpar.blue: 255
```

### 9.4 Blink (Poor Man Strobe)

```yaml
scenes:
  - id: white-blink
    name: White Blink
    type: sequence
    safety-class: hazard
    priority: 50
    phase: start-on-activation
    loop: true
    max-active-ms: 3000
    steps:
      - duration-ms: 80
        transition: hold
        values:
          leftpar.dimmer: 255
          leftpar.red: 255
          leftpar.green: 255
          leftpar.blue: 255
      - duration-ms: 80
        transition: hold
        values:
          leftpar.dimmer: 0
```

### 9.5 Scene Fields

```text
id              stable identifier
name            display name
description     optional note
type            static or sequence
safety-class    persistent, temporary, or hazard
priority        merge priority number
values          target/value pairs (static)
steps           timed steps (sequence)
loop            boolean (sequence)
phase           start-on-activation or global-clock
max-active-ms   auto-release timeout
```

### 9.6 Scene Merge Behavior

```text
htp       highest takes precedence (dimmer, RGB)
ltp       latest takes precedence (strobe, gobo, pan, tilt)
priority  explicit priority wins
```

Practical v1: use the merge mode defined on each fixture channel.

---

## 10. MIDI Mappings

### 10.1 Mapping Model

A mapping connects a MIDI trigger to an action:

```yaml
mappings:
  - id: pad-60-red-wash
    name: Red Wash
    trigger:
      type: note
      channel: 1
      note: 60
    action:
      type: scene
      scene: red-wash
      mode: gate
      intensity-source: velocity
```

### 10.2 Trigger Types

```yaml
# note trigger
trigger:
  type: note
  channel: 1
  note: 60

# CC trigger
trigger:
  type: cc
  channel: 1
  cc: 7
```

### 10.3 Action Types

```text
scene           activate a scene
raw-value       CC value maps to a named target (0-127 -> 0-255)
raw-velocity    note velocity maps to a named target
blackout        toggle/enable blackout
clear           clear active states
```

### 10.4 Scene Action Modes

```text
gate
  note-on activates, note-off releases
  primary mode for REAPER timeline use

flash
  same as gate, may use max-hold

toggle
  note-on toggles active/inactive, note-off ignored
  intended for manual controllers, not REAPER timeline
```

### 10.5 Raw Velocity Action

```yaml
action:
  type: raw-velocity
  target: leftpar.dimmer
  release: zero
```

Scaling: MIDI 0-127 -> DMX 0-255 via `(velocity * 255) / 127`.

Release modes: `zero`, `keep`, `default`.

### 10.6 Raw Value Action (CC)

```yaml
action:
  type: raw-value
  target: leftpar.dimmer
```

Scaling: MIDI CC 0-127 -> DMX 0-255.

### 10.7 Debounce

```yaml
defaults:
  debounce-ms: 80
```

Prevents duplicate toggles from noisy controllers.

### 10.8 Runtime Mapping Structure

```c
typedef enum {
    TRIGGER_NOTE,
    TRIGGER_CC
} trigger_type_t;

typedef enum {
    ACTION_SCENE,
    ACTION_RAW_VELOCITY,
    ACTION_RAW_VALUE,
    ACTION_BLACKOUT,
    ACTION_CLEAR
} action_type_t;

typedef enum {
    MODE_NONE,
    MODE_GATE,
    MODE_FLASH,
    MODE_TOGGLE
} action_mode_t;
```

---

## 11. Rendering

### 11.1 Rendering Layers

```text
base defaults (from fixture channel defaults)
+ active toggle/gate scenes
+ active flash scenes
+ raw overrides
+ global master / blackout
= final DMX frame
```

### 11.2 Blackout

```text
soft blackout:
  preserves active scene/toggle state internally
  output forced to zero until blackout is disabled

hard clear:
  clears all active flash/toggle/raw states
  output becomes zero because no state remains
```

### 11.3 Frame Rendering

The renderer is evaluated at output frame time. The main thread updates logical state. The DMX output thread renders and sends:

```c
uint8_t frame[512];

pthread_mutex_lock(&stage->lock);
stage_render_universe_locked(stage, now_ms, frame);
pthread_mutex_unlock(&stage->lock);

dmx_backend_send_frame(&backend, frame);
```

---

## 12. Safety

### 12.1 Safety Classes

Each fixture channel and scene declares a safety class:

```text
persistent    on MIDI loss: keep current value
temporary     on MIDI loss: release after TTL
hazard        on MIDI loss: force to safe-value immediately
```

### 12.2 Heartbeat Surveillance

Active only during declared show playback sections. Uses reserved MIDI messages:

```text
CC 118 value 127 -> arm surveillance
CC 118 value 0   -> disarm surveillance
CC 119           -> heartbeat pulse
```

Configuration:

```yaml
safety:
  heartbeat-surveillance:
    enabled: true
    channel: 16
    arm-cc: 118
    heartbeat-cc: 119
    interval-ms: 500
    timeout-ms: 2000
    on-timeout: degraded-hold
  classes:
    persistent:
      on-midi-loss: keep
    temporary:
      on-midi-loss: release
      ttl-ms: 1500
    hazard:
      on-midi-loss: release
      ttl-ms: 500
      force-safe-value: true
```

### 12.3 Heartbeat State Machine

```text
DISARMED
  arm received -> ARMED-HEALTHY

ARMED-HEALTHY
  heartbeat timeout -> ARMED-FAILED

ARMED-FAILED
  heartbeat returns -> ARMED-HEALTHY

any state
  disarm received -> DISARMED
```

On timeout (degraded-hold):

- Keep persistent outputs.
- Release temporary outputs.
- Force hazard outputs to safe values.
- Keep MIDI port open.
- Keep DMX output running.
- Resume normal operation when heartbeat returns.

---

## 13. HTTP API and WebSocket

### 13.1 HTTP Server

Embedded in the daemon. Binds to `127.0.0.1:8080` by default.

Responsibilities:

- JSON API for runtime control and project editing.
- WebSocket event stream for live UI state.
- Static file serving for the Svelte UI.
- SPA fallback (all non-API/non-asset routes serve `index.html`).

### 13.2 Endpoints

**Live state:**

```text
GET  /api/live/snapshot
GET  /api/live/stage
```

**Scene injection (manual from UI):**

```text
POST /api/live/scenes/{scene-id}/activate
POST /api/live/scenes/{scene-id}/release
POST /api/live/scenes/{scene-id}/toggle
POST /api/live/clear-manual
```

**Safety/control:**

```text
POST /api/safety/blackout
POST /api/safety/clear
POST /api/dmx/reconnect
```

**Project:**

```text
GET  /api/project
PUT  /api/project
POST /api/project/reload
POST /api/project/save
```

**Status:**

```text
GET  /api/status
```

**REAPER exports:**

```text
GET  /api/reaper/note-names
GET  /api/reaper/cue-map
```

### 13.3 Status Response

```json
{
  "version": 1,
  "config-revision": 42,
  "midi": {
    "backend": "portmidi",
    "mode": "create-virtual",
    "port-name": "spark",
    "status": "active",
    "owned-by-daemon": true,
    "last-event": {
      "type": "note-on",
      "channel": 1,
      "note": 60,
      "velocity": 127
    }
  },
  "dmx": {
    "backend": "pro-packet-serial",
    "device": "/dev/serial/by-id/usb-ENTTEC_DMX_USB_PRO_XXXXXXXX-if00-port0",
    "status": "connected",
    "refresh-rate-hz": 40,
    "frames-sent": 184023,
    "write-errors": 0,
    "reconnects": 2,
    "last-error": null
  },
  "runtime": {
    "blackout": false,
    "active-toggles": ["red-wash"],
    "active-flashes": [],
    "raw-overrides": 1
  }
}
```

### 13.4 WebSocket

Endpoint:

```text
GET /ws/live/events
```

Each message:

```json
{
  "sequence": 9822,
  "server-time-ms": 18420333,
  "type": "scene-activated",
  "scene-id": "red-wash",
  "source": "midi"
}
```

Event types (v1):

```text
scene-activated
scene-released
scene-expired
manual-scenes-cleared
blackout-changed
heartbeat-armed
heartbeat-disarmed
heartbeat-failed
heartbeat-restored
dmx-connected
dmx-disconnected
dmx-reconnecting
dmx-error
midi-connected
midi-disconnected
midi-error
midi-message
project-loaded
project-reloaded
project-save-result
config-revision-changed
safety-state-changed
```

### 13.5 Sequence Handling

The UI tracks the last sequence number. If a gap is detected:

```text
request /api/live/snapshot
replace local live state
continue from snapshot sequence
```

Periodic reconciliation every 3-5 seconds regardless of gaps.

### 13.6 Static File Serving

```text
GET /api/...        -> API handler
GET /ws/...         -> WebSocket handler
GET /assets/...     -> static file from ui-root/assets
GET /               -> ui-root/index.html
GET /any-ui-route   -> ui-root/index.html (SPA fallback)
```

Path traversal must be rejected. Files outside ui-root must not be served.

---

## 14. Web UI

### 14.1 Technology

```text
Svelte + TypeScript + Vite
```

Compiled to static files. The show machine does not need npm/node at runtime.

The daemon serves the built UI:

```bash
sparkd --project projects/default --ui-root ui
```

### 14.2 Pages

```text
Live        scene pad, status bar, active scenes, manual controls
Fixtures    create/edit fixtures and channel layouts
Scenes      create/edit static and sequence scenes
Mappings    bind MIDI triggers to actions
Settings    MIDI, DMX, HTTP, safety config
REAPER      export note names and cue maps
```

### 14.3 Live Page

The main operational page:

- **Status bar**: DMX/MIDI/heartbeat/safety/WebSocket status.
- **Scene pad**: grouped buttons for scene activation. Behavior depends on safety class:
  - persistent: click toggles manual activation
  - temporary: press activates, release deactivates
  - hazard: press-and-hold only, release deactivates
- **Active scenes list**: scene name, source (midi/manual), duration, safety class.
- **Manual controls**: Blackout, Clear Blackout, Clear Manual Scenes, Reconnect DMX, Reload Project.

### 14.4 Scene Pad

Uses `ui.scene-pad` project config:

```yaml
ui:
  scene-pad:
    groups:
      - id: ambiance
        name: Ambiance
        scenes:
          - red-wash
          - blue-wash
          - color-cycle
      - id: hits
        name: Hits
        scenes:
          - white-blink
```

Scene injection uses manual scene endpoints (not fake MIDI):

```text
POST /api/live/scenes/{scene-id}/activate
POST /api/live/scenes/{scene-id}/release
POST /api/live/scenes/{scene-id}/toggle
```

### 14.5 Fixture Editor

- Enter fixture id, name, description, start address, channel count.
- Edit one row per channel (offset, name, kind, merge, safety-class, safe-value).
- Import from template (prefills form, user can still edit everything).
- Display computed DMX range and overlap warnings.

### 14.6 Scene Editor

- Static scenes: target/value table with fixture/channel dropdowns.
- Sequence scenes: step list with duration, transition (hold/linear), and values per step.
- Blink helper: creates a two-step looping sequence.

### 14.7 Mapping Editor

- Trigger editor: note or CC, channel, note/cc number.
- Action editor: scene (gate/flash/toggle), raw-value, raw-velocity, blackout, clear.

### 14.8 Communication

```text
HTTP for snapshots, project editing, and commands
WebSocket for semantic live events
periodic reconciliation every 3-5 seconds
```

The UI does not stream raw DMX frames. It consumes semantic state only.

### 14.9 Styling

- Dark-room friendly, low visual clutter.
- Large scene buttons suitable for touch.
- Hazard scenes visually distinct (warning color).
- Clear connection/failure indicators.

---

## 15. REAPER Integration

### 15.1 Design Principles

- The daemon is the source of truth.
- REAPER is a timeline and monitoring UI.
- All time-critical cue playback uses MIDI (never CLI/HTTP for cue timing).
- Generated files (note names, cue map) are disposable and regenerable.

### 15.2 Programming Model

Scenes are MIDI notes:

```text
Note 60 -> Red Wash (gate)
Note 61 -> Blue Wash (toggle)
Note 62 -> White Hit (gate)
```

Continuous controls are MIDI CC:

```text
CC 7  -> Master Dimmer
CC 20 -> Front Left / Dimmer
CC 21 -> Front Left / Red
```

In REAPER: long note = long gate, short note = quick hit.

### 15.3 Generated Artifacts

**Note-name file** (`reaper/generated/spark-note-names.txt`):

```text
60 Red Wash
61 Blue Wash
62 White Hit
```

Endpoint: `GET /api/reaper/note-names` (text/plain)

**Cue map** (`reaper/generated/spark-cue-map.yaml`):

```json
{
  "version": 1,
  "config-revision": 42,
  "generated-from": "sparkd",
  "midi": {
    "target-port": "spark",
    "mode": "create-virtual",
    "owned-by-daemon": true
  },
  "tracks": [
    {
      "id": "scenes",
      "name": "Spark Scenes",
      "type": "scene-notes",
      "channel": 1
    }
  ],
  "scene-notes": [
    {
      "scene": "red-wash",
      "label": "Red Wash",
      "channel": 1,
      "note": 60,
      "mode": "gate",
      "color": "#ff0000"
    }
  ],
  "controls": [
    {
      "id": "master-dimmer",
      "label": "Master Dimmer",
      "channel": 1,
      "cc": 7,
      "target": "master.dimmer"
    }
  ]
}
```

Endpoint: `GET /api/reaper/cue-map` (application/json)

### 15.4 REAPER Scripts

```text
reaper/scripts/
  Spark_Import cue map.lua
  Spark_Load note names.lua
  Spark_Open live stage panel.lua
  Spark_Blackout.lua
  Spark_Clear states.lua
  Spark_Reconnect DMX.lua
  Spark_Reload config.lua
  Spark_Refresh generated files.lua
```

Control scripts call `sparkctl` or the HTTP API directly.

### 15.5 Live Stage Panel

Polls `/api/live/stage` at 5-10 Hz. Displays:

- DMX/MIDI connection state.
- Active scenes.
- Fixture positions with preview colors.
- Blackout/clear/reconnect buttons.

### 15.6 Assignment Stability

Scene note and CC assignments are stable across exports:

- Existing scene IDs keep their MIDI note.
- New scenes get the next available note.
- Assignments may be manually pinned in config.

### 15.7 Config Revision

The daemon exposes a monotonically increasing `config-revision` in status, live-state, and cue-map responses. REAPER scripts can detect stale cue maps and prompt for refresh.

---

## 16. Validation Rules

### 16.1 MIDI Values

```text
channel: 1-16 user-facing
note:    0-127
cc:      0-127
value:   0-127
```

### 16.2 DMX Values

```text
address: 1-512 user-facing
value:   0-255
```

### 16.3 IDs

Pattern: `[a-zA-Z0-9_-]+`

Must be unique within their section.

### 16.4 Fixtures

```text
fixture id unique
start-address between 1 and 512
channel-count between 1 and 512
start-address + channel-count - 1 <= 512
channel names unique within fixture
channel offsets unique and sequential
offsets between 0 and channel-count - 1
fixtures do not overlap unless explicitly allowed
```

### 16.5 Scenes

```text
scene id unique
all targets must resolve to existing fixture channels
values must be 0-255
sequence steps: at least one, duration-ms > 0
transition: hold or linear
```

### 16.6 Mappings

```text
mapping id unique
trigger values in MIDI range
referenced scene must exist
referenced target must exist
mode must be valid for trigger type
```

---

## 17. Build and Run

### 17.1 Repository Layout

```text
sparkd/
  Makefile
  README.md
  .gitignore

  vendor/
    portmidi/         # vendored MIDI library source
    mongoose/         # vendored HTTP/WebSocket (mongoose.c + mongoose.h)
    libyaml/          # vendored YAML parser source

  daemon/
    Makefile
    src/
      serial_posix.c  # our own serial port (Linux)
      serial_win32.c  # our own serial port (Windows)
      serial.h

  tools/
    sparkctl.c
    Makefile

  ui/
    package.json
    vite.config.ts
    src/

  reaper/
    scripts/
    generated/

  projects/
    example/
```

### 17.2 Dependencies

All vendored -- no system packages required:

```text
vendor/portmidi/    cross-platform MIDI (WinMM on Windows, ALSA on Linux)
vendor/mongoose/    embedded HTTP + WebSocket server (2 files)
vendor/libyaml/     YAML parser
```

Platform-provided (no install needed):

```text
pthreads            winpthreads on MinGW-w64, libc on Linux
WinMM               Windows MIDI (linked by portmidi)
Winsock2            Windows sockets (linked by mongoose)
```

Written in-house:

```text
spark_serial        thin serial port abstraction (POSIX termios / Win32 CreateFile)
```

Build requirements:

```text
C compiler (gcc/MinGW-w64 or clang)
GNU Make
```

Optional for UI development:

```text
node / npm (build-time only)
```

### 17.3 Build

```bash
make              # builds daemon/sparkd and tools/sparkctl
make ui           # builds ui/dist from Svelte sources
make clean
```

### 17.4 CLI

```text
sparkd --project PATH         project YAML file or directory
sparkd --ui-root PATH         built Svelte UI directory
sparkd --http-bind ADDRESS    default 127.0.0.1
sparkd --http-port PORT       default 8080
sparkd --log-level LEVEL      debug, info, warn, error
sparkd --validate-project PATH  validate and exit
sparkd --version
sparkd --help
```

Command-line options override project settings.

### 17.5 Development Run

```bash
make
./build/sparkd --project projects/example --log-level debug
```

UI development (separate terminal):

```bash
cd ui && npm run dev
```

Vite proxies `/api` and `/ws` to the daemon.

---

## 18. Non-Goals for V1

```text
Multiple MIDI input ports
Complex audio timeline features
Full QLC+ replacement feature set
Multi-universe DMX
Cloud connectivity
Remote access over the internet
Complex user management
Real-time collaborative editing
Full fixture profile database
Raw DMX frame streaming to UI
Full 2D graphical stage editor
Electron shell
Multi-output dashboard
```

Keep version 1 focused and reliable.

---

## 19. Development Roadmap

### Phase 1: Core Proof of Concept

- C daemon starts.
- Creates MIDI virtual port via portmidi.
- Receives note-on/note-off.
- Maintains one 512-byte DMX frame.
- Dummy DMX backend.
- Hardcoded mapping.

### Phase 2: DMX Backends

- Pro Packet Serial backend.
- Open DMX backend.
- DMX output thread with reconnect state machine.
- Backend selectable in config.
- Stable device path + fallback glob.

### Phase 3: Project Config

- YAML project loader (libyaml).
- Fixture templates and fixtures.
- Scenes (static + sequence).
- Mappings with gate/flash/toggle/raw.
- Safety classes and heartbeat surveillance.
- Full validation pass.
- Example project files.

### Phase 4: HTTP API and WebSocket

- Embedded HTTP server.
- REST endpoints for project, live state, control.
- WebSocket semantic events.
- Static file serving with SPA fallback.
- REAPER export endpoints.

### Phase 5: sparkctl CLI

- Thin HTTP client.
- Commands: status, blackout, clear, reconnect-dmx, reload-config.

### Phase 6: Web UI

- Svelte + TypeScript + Vite scaffold.
- Live page with scene pad.
- Fixture/scene/mapping editors.
- WebSocket client with reconciliation.
- Dark-room-friendly styling.

### Phase 7: Refinement

- Better merge logic.
- Fixture groups.
- Art-Net backend.
- MIDI learn mode.
- Config backup and recovery.
- LAN mode with token authentication.
- REAPER ReaImGui stage panel.

---

## 20. Open Design Questions

These are deferred to implementation time:

- **HTTP library**: mongoose (vendored, single `.c` + `.h`).
- **License**: MIT, BSD-2-Clause, or GPL-3.0.
- **Windows portmidi virtual port**: `create-virtual` is not supported on Windows. The daemon returns a clear error if attempted. Windows uses `open-existing` exclusively.
  > We will return NotImplemented or something.
- **Service/deployment**: systemd unit, udev rules, packaging -- independent concern, to be addressed separately.
