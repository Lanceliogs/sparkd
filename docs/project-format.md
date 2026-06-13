# Project File Format

sparkd projects are YAML files with the `.spark.yaml` extension. This reference covers everything you need to hand-edit project files.

## File Structure

```yaml
format:
  name: spark-project
  version: 1

app:
  name: my-show

midi:
  # MIDI input configuration

dmx:
  # DMX output configuration

fixtures:
  # Light definitions

scenes:
  # Lighting states
```

The `format` and `app` sections are metadata. The engine uses `midi`, `dmx`, `fixtures`, and `scenes`.

## Naming Conventions

- All YAML keys use **kebab-case**: `start-address`, `channel-count`, `duration-ms`
- IDs must match `[a-zA-Z0-9_-]+`
- MIDI channels are **1-based** (1-16)
- DMX addresses are **1-based** (1-512)
- DMX values are **0-255**

## MIDI

```yaml
midi:
  device: "My Controller"
  mode: open-existing
```

| Field | Required | Description |
|-------|----------|-------------|
| `device` | yes | Device name (substring match) or virtual port name |
| `mode` | yes | `open-existing` or `create-virtual` |

**Modes:**

- `open-existing` — connect to a MIDI port already on the system. The `device` string is matched as a substring against available port names (case-insensitive). For example, `"KeyStep"` matches `"Arturia KeyStep 32"`.
- `create-virtual` — create a new virtual MIDI port named `device`. Useful for routing MIDI from a DAW (Linux/macOS only).

Omit the entire `midi:` section to disable MIDI input.

> **Tip:** Run `spark-midi list` to see available MIDI device names on your system.

## DMX

```yaml
dmx:
  device: COM3
  backend: open
  refresh-rate-hz: 25
```

| Field | Required | Description |
|-------|----------|-------------|
| `device` | conditional | Serial port, `auto`, `auto:tag`, or IP address (artnet) |
| `backend` | yes | `open`, `pro`, `artnet`, or `dummy` |
| `refresh-rate-hz` | no | Update rate in Hz (default: 25, range: 1-44) |

**Backends:**

- `open` — Open DMX USB protocol (break + raw frames). Works with most generic USB-DMX interfaces (FTDI-based clones, Eurolite, etc.).
- `pro` — Enttec Pro protocol (packetized serial). For Enttec DMX USB Pro and compatibles.
- `artnet` — Art-Net protocol (ArtDmx packets over UDP, port 6454). For Ethernet/WiFi Art-Net nodes.
- `dummy` — No output. The engine runs normally but nothing is sent to hardware. Great for testing.

**Device values:**

- A serial port path: `COM3` (Windows) or `/dev/ttyUSB0` (Linux) — for `open` and `pro` backends
- `auto` — auto-detect any known DMX USB device at engine start
- `auto:enttec` — auto-detect Enttec devices only
- `auto:eurolite` — auto-detect Eurolite devices only
- `auto:ftdi` — auto-detect any FTDI-based device
- An IP address (e.g. `192.168.1.100`) — for the `artnet` backend (unicast to the Art-Net node)

Omit the `dmx:` section entirely to default to `dummy`.

> **Tip:** Use `spark-serial list` to see connected USB-serial devices and `spark-serial find` to test auto-detection from the command line.

### Art-Net example

```yaml
dmx:
  backend: artnet
  device: 192.168.1.100
  refresh-rate-hz: 30
```

Sends ArtDmx packets (universe 0) via UDP unicast to the specified IP on port 6454. The Art-Net node must be reachable on your network — use `ping` to verify.

## Fixtures

A fixture represents one physical light patched at a DMX start address.

### Basic fixture with inline channels

```yaml
fixtures:
  - id: par-left
    name: Left PAR
    start-address: 1
    channel-count: 8
    channels:
      - name: dimmer
        offset: 0
      - name: red
        offset: 1
      - name: green
        offset: 2
      - name: blue
        offset: 3
      - name: white
        offset: 4
      - name: mode
        offset: 5
      - name: color
        offset: 6
      - name: strobe
        offset: 7
```

| Field | Required | Description |
|-------|----------|-------------|
| `id` | yes | Unique identifier (used in scene values) |
| `name` | no | Display name (defaults to `id`) |
| `start-address` | yes | DMX start address (1-512) |
| `channel-count` | conditional | Number of DMX channels |
| `channels` | conditional | Channel definitions |
| `copy-from` | conditional | ID of another fixture to copy layout from |
| `template` | conditional | `bank-id:fixture-id` from a fixture bank |

Exactly one of `channels`, `copy-from`, or `template` must be present.

### Channel fields

| Field | Required | Description |
|-------|----------|-------------|
| `name` | yes | Channel name (referenced in scenes as `fixture.channel`) |
| `offset` | yes | Zero-based offset within the fixture |

### Copy from another fixture

```yaml
fixtures:
  - id: par-left
    start-address: 1
    channel-count: 4
    channels:
      - name: red
        offset: 0
      - name: green
        offset: 1
      - name: blue
        offset: 2
      - name: dimmer
        offset: 3

  - id: par-right
    start-address: 5
    copy-from: par-left
```

Inherits `channel-count` and `channels` from the referenced fixture (must be defined earlier in the file).

### From a fixture bank template

```yaml
fixtures:
  - id: par-left
    start-address: 1
    template: stairville:par-8ch
```

References `fixture-id` from `bank-id`. Banks are loaded from `SPARK_FIXTURE_BANK_PATH` directories.

## Scenes

A scene is a lighting state activated by a MIDI trigger (or the UI).

### Static scene

Fixed DMX values applied while the scene is active:

```yaml
scenes:
  - id: red-wash
    name: Red Wash
    type: static
    trigger:
      channel: 1
      note: 60
      mode: gate
    values:
      par-left.dimmer: 255
      par-left.red: 255
      par-left.green: 0
      par-left.blue: 0
```

### Sequence scene

Timed steps with transitions:

```yaml
scenes:
  - id: color-cycle
    name: Color Cycle
    type: sequence
    trigger:
      channel: 1
      note: 61
      mode: gate
    loop: true
    steps:
      - duration-ms: 1000
        transition: linear
        values:
          par-left.red: 255
          par-left.green: 0
          par-left.blue: 0
      - duration-ms: 1000
        transition: linear
        values:
          par-left.red: 0
          par-left.green: 255
          par-left.blue: 0
      - duration-ms: 1000
        transition: linear
        values:
          par-left.red: 0
          par-left.green: 0
          par-left.blue: 255
```

### Scene fields

| Field | Required | Type | Description |
|-------|----------|------|-------------|
| `id` | yes | string | Unique identifier |
| `name` | no | string | Display name (defaults to `id`) |
| `type` | yes | string | `static` or `sequence` |
| `trigger` | yes | mapping | MIDI trigger definition |
| `values` | static only | mapping | DMX values (`fixture.channel: 0-255`) |
| `loop` | no | bool | Loop the sequence (default: false) |
| `steps` | sequence only | list | Sequence step definitions |

### Trigger fields

| Field | Required | Description |
|-------|----------|-------------|
| `channel` | yes | MIDI channel (1-16) |
| `note` | yes | MIDI note number (0-127) |
| `mode` | yes | `gate` or `toggle` |

**Modes:**

- `gate` — scene is active while the MIDI key is held, released when the key goes up
- `toggle` — first press activates, second press releases

### Values syntax

Values map fixture channels to DMX levels using dot notation:

```yaml
values:
  fixture-id.channel-name: 0-255
```

The DMX address is resolved as: `(start-address - 1) + channel.offset`

### Step fields (sequences)

| Field | Required | Description |
|-------|----------|-------------|
| `duration-ms` | yes | Step duration in milliseconds |
| `transition` | yes | `snap` (instant) or `linear` (crossfade to these values) |
| `values` | yes | Target/value pairs for this step |

## Includes (Directory Mode)

For larger projects, split fixtures and scenes into separate files:

```yaml
# project.spark.yaml
format:
  name: spark-project
  version: 1

midi:
  device: "Launchpad Mini"
  mode: open-existing

dmx:
  device: /dev/ttyUSB0
  backend: open

includes:
  fixtures: fixtures.yaml
  scenes: scenes.yaml
```

Include files contain a bare YAML sequence (no top-level key):

```yaml
# fixtures.yaml
- id: par-left
  start-address: 1
  template: stairville:par-8ch

- id: par-right
  start-address: 9
  copy-from: par-left
```

```yaml
# scenes.yaml
- id: red-wash
  type: static
  trigger:
    channel: 1
    note: 60
    mode: gate
  values:
    par-left.dimmer: 255
    par-left.red: 255
```

Rules:
- A section cannot appear both inline and via includes (this is an error)
- Include paths are relative to the project file's directory

## Fixture Bank Files

Bank files define reusable templates. They live in directories from `SPARK_FIXTURE_BANK_PATH`:

```yaml
bank:
  id: stairville
  version: 1

fixtures:
  - id: par-8ch
    channel-count: 8
    channels:
      - name: dimmer
        offset: 0
      - name: red
        offset: 1
      - name: green
        offset: 2
      - name: blue
        offset: 3
      - name: white
        offset: 4
      - name: mode
        offset: 5
      - name: color
        offset: 6
      - name: strobe
        offset: 7
```

Bank fixtures don't have `start-address` — that's set per-project when you reference the template.

## Full Annotated Example

```yaml
format:
  name: spark-project
  version: 1

app:
  name: club-night

# MIDI: connect to any port containing "Launchpad" in its name
midi:
  device: Launchpad
  mode: open-existing

# DMX: Open DMX USB on COM3 at 30 Hz
dmx:
  device: COM3
  backend: open
  refresh-rate-hz: 30

fixtures:
  # Use a template from the "stairville" bank
  - id: par-1
    start-address: 1
    template: stairville:par-8ch

  # Same light model at a different address
  - id: par-2
    start-address: 9
    copy-from: par-1

  # Manual definition for a simple dimmer pack
  - id: dimmer-pack
    start-address: 17
    channel-count: 4
    channels:
      - name: ch1
        offset: 0
      - name: ch2
        offset: 1
      - name: ch3
        offset: 2
      - name: ch4
        offset: 3

scenes:
  # Static: hold note 60 for red wash
  - id: red-wash
    name: Red Wash
    type: static
    trigger:
      channel: 1
      note: 60
      mode: gate
    values:
      par-1.dimmer: 255
      par-1.red: 255
      par-1.green: 0
      par-1.blue: 0
      par-2.dimmer: 255
      par-2.red: 255
      par-2.green: 0
      par-2.blue: 0

  # Static with toggle: press once to activate, again to release
  - id: house-lights
    name: House Lights
    type: static
    trigger:
      channel: 1
      note: 72
      mode: toggle
    values:
      dimmer-pack.ch1: 200
      dimmer-pack.ch2: 200

  # Sequence: smooth RGB cycle, loops while held
  - id: rgb-cycle
    name: RGB Cycle
    type: sequence
    trigger:
      channel: 1
      note: 61
      mode: gate
    loop: true
    steps:
      - duration-ms: 2000
        transition: linear
        values:
          par-1.red: 255
          par-1.green: 0
          par-1.blue: 0
          par-2.red: 0
          par-2.green: 0
          par-2.blue: 255
      - duration-ms: 2000
        transition: linear
        values:
          par-1.red: 0
          par-1.green: 255
          par-1.blue: 0
          par-2.red: 255
          par-2.green: 0
          par-2.blue: 0
      - duration-ms: 2000
        transition: linear
        values:
          par-1.red: 0
          par-1.green: 0
          par-1.blue: 255
          par-2.red: 0
          par-2.green: 255
          par-2.blue: 0
```

## Tips

- **Validate before a show:** `sparkd --project my-show.spark.yaml --validate`
- **Test without hardware:** Use `backend: dummy` and `mode: create-virtual`
- **Find your MIDI device name:** `spark-midi list`
- **Multiple fixtures of the same type:** Define the first with `channels`, then use `copy-from` for the rest
- **Keep it organized:** Use includes for projects with many fixtures/scenes
- **DMX address conflicts:** sparkd does not check for address overlaps — plan your patch carefully
