# Spark Project YAML Format

This document describes the `.spark.yaml` file format used by `sparkd` to define
a project: fixtures, scenes, and their relationships.

## File Extension

Project files use the `.spark.yaml` extension (e.g. `my-show.spark.yaml`).

## Top-Level Structure

```yaml
format:
  name: spark-project
  version: 1

app:
  name: my-show

midi:
  mode: open-existing
  device: "My Controller"

dmx:
  backend: open
  device: /dev/ttyUSB0

fixtures: []
scenes: []
```

### Recognized sections

| Section            | Status       | Description                         |
|--------------------|--------------|-------------------------------------|
| `format`           | skipped      | Format identifier and version       |
| `app`              | skipped      | Project identity                    |
| `midi`             | **parsed**   | MIDI input configuration            |
| `dmx`              | **parsed**   | DMX output configuration            |
| `fixtures`         | **parsed**   | Physical lights and channels        |
| `scenes`           | **parsed**   | Lighting states (static + sequence) |
| `includes`         | **parsed**   | Include files (directory mode)      |
| `fixture-templates`| skipped      | (deprecated: use fixture bank files)|
| `mappings`         | skipped      | MIDI trigger to action bindings     |
| `http`             | skipped      | HTTP server config                  |
| `safety`           | skipped      | Heartbeat surveillance config       |
| `reaper`           | skipped      | REAPER export config                |
| `ui`               | skipped      | UI preferences                      |

Unknown keys at the top level produce a debug log and are skipped.

---

## Naming Conventions

- All YAML keys use **kebab-case**: `start-address`, `channel-count`, `duration-ms`.
- IDs must match `[a-zA-Z0-9_-]+`.
- User-facing MIDI channels are **1-based** (1-16).
- User-facing DMX addresses are **1-based** (1-512).
- DMX values are 0-255.

---

## MIDI

Configures how sparkd connects to a MIDI input.

```yaml
midi:
  mode: open-existing       # or create-virtual
  device: "My Controller"
```

### MIDI fields

| Field    | Required | Type   | Description                                         |
|----------|----------|--------|-----------------------------------------------------|
| `mode`   | yes      | string | `open-existing` or `create-virtual`                 |
| `device` | cond.    | string | Device to open or virtual port name to create       |

### Modes

- **open-existing**: Opens an existing MIDI device by name. `device` is the name to match.
- **create-virtual**: Creates a virtual MIDI port named `device` (default: "spark").

If the `midi` section is omitted, no MIDI input is configured.

---

## DMX

Configures the DMX output backend.

```yaml
dmx:
  backend: open             # or dummy
  device: /dev/ttyUSB0
```

### DMX fields

| Field     | Required | Type   | Description                        |
|-----------|----------|--------|------------------------------------|
| `backend` | yes      | string | `open` (Open DMX USB) or `dummy`   |
| `device`  | cond.    | string | Serial device path (open backend)  |

If the `dmx` section is omitted, defaults to the `dummy` backend.

---

## Includes (Directory Mode)

For larger projects, fixtures and scenes can be split into separate files.
Use the `includes` section to reference them relative to the project file.

```yaml
includes:
  fixtures: fixtures.yaml
  scenes: scenes.yaml
```

Include files contain a bare YAML sequence (no top-level key):

```yaml
# fixtures.yaml
- id: par1
  start-address: 1
  channel-count: 4
  channels:
    - name: dimmer
      offset: 0
```

Constraints:
- A section cannot appear both inline and in includes (error).
- Include paths are relative to the directory containing the project file.

---

## Fixtures

A fixture represents one physical light patched at a DMX start address.

### Inline channels (default)

```yaml
fixtures:
  - id: leftpar
    name: Left PAR              # optional, defaults to id
    start-address: 1            # 1-based DMX address
    channel-count: 5
    channels:
      - name: dimmer
        offset: 0
      - name: red
        offset: 1
      - name: green
        offset: 2
      - name: blue
        offset: 3
      - name: strobe
        offset: 4
```

### Copy from another fixture

Copies `channel-count` and `channels` from a fixture defined earlier in the same file:

```yaml
fixtures:
  - id: leftpar
    start-address: 1
    channel-count: 5
    channels:
      - name: dimmer
        offset: 0
      # ...

  - id: rightpar
    start-address: 6
    copy-from: leftpar           # inherits channel layout from leftpar
```

### Template from fixture bank

Copies `channel-count` and `channels` from a template in an external bank file:

```yaml
fixtures:
  - id: leftpar
    start-address: 1
    template: stairville:par-8ch   # bank-id:fixture-id
```

### Fixture fields

| Field           | Required | Type   | Description                                |
|-----------------|----------|--------|--------------------------------------------|
| `id`            | yes      | string | Unique identifier                          |
| `name`          | no       | string | Display name (defaults to `id`)            |
| `start-address` | yes      | int    | DMX start address (1-512)                  |
| `channel-count` | cond.    | int    | Number of DMX channels                     |
| `channels`      | cond.    | list   | Channel definitions                        |
| `copy-from`     | cond.    | string | ID of fixture earlier in file to copy from |
| `template`      | cond.    | string | `bank-id:fixture-id` from bank             |

Channel source fields (`channels`, `copy-from`, `template`) are mutually exclusive.
Exactly one must be present. When using `copy-from` or `template`, `channel-count`
is inherited automatically.

### Channel fields

| Field    | Required | Type   | Description                       |
|----------|----------|--------|-----------------------------------|
| `name`   | yes      | string | Channel name (used in targets)    |
| `offset` | yes      | int    | Zero-based offset within fixture  |

### Target syntax

Scenes reference fixture channels using dot notation:

```
fixture-id.channel-name
```

Resolution: `dmx_index = (start-address - 1) + channel.offset`

---

## Fixture Bank Files

Bank files define reusable fixture templates that can be referenced by any project.
They live in directories listed in `SPARK_FIXTURE_BANK_PATH` (or `~/.spark/fixtures/`).

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

  - id: par-4ch
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
```

### Bank file fields

| Field          | Required | Type   | Description                              |
|----------------|----------|--------|------------------------------------------|
| `bank.id`      | yes      | string | Unique bank identifier                   |
| `bank.version` | no       | int    | Bank version (informational)             |
| `fixtures`     | yes      | list   | Fixture templates (no `start-address`)   |

### Loading rules

- `SPARK_FIXTURE_BANK_PATH` uses `;` as separator (e.g. `/path/a;/path/b`)
- All `.yaml`/`.yml` files in each directory are scanned
- Files without `bank.id` are skipped (warning)
- Duplicate `bank.id` across files: second file is skipped (warning with path)
- `bank:` section must appear before `fixtures:` in the file
- Templates are referenced as `bank-id:fixture-id` in project files

---

## Scenes

A scene is a renderable lighting state activated by a MIDI trigger.

### Scene types

- **static** - fixed DMX values applied while active
- **sequence** - timed steps with transitions, optionally looping

### Static scene

```yaml
scenes:
  - id: red-wash
    name: Red Wash              # optional, defaults to id
    type: static
    trigger:
      channel: 1                # MIDI channel (1-16)
      note: 60                  # MIDI note (0-127)
      mode: gate                # gate or toggle
    values:
      leftpar.dimmer: 220
      leftpar.red: 255
      leftpar.green: 0
      leftpar.blue: 0
```

### Sequence scene

```yaml
scenes:
  - id: color-cycle
    name: Color Cycle           # optional, defaults to id
    type: sequence
    trigger:
      channel: 1
      note: 61
      mode: gate
    loop: true
    steps:
      - duration-ms: 1000
        transition: linear      # hold or linear
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

### Scene fields

| Field     | Required | Type    | Description                              |
|-----------|----------|---------|------------------------------------------|
| `id`      | yes      | string  | Unique identifier                        |
| `name`    | no       | string  | Display name (defaults to `id`)          |
| `type`    | yes      | string  | `static` or `sequence`                   |
| `trigger` | yes      | mapping | MIDI trigger definition                  |
| `values`  | static   | mapping | Target/value pairs (static scenes only)  |
| `loop`    | no       | bool    | Loop sequence (default: false)           |
| `steps`   | sequence | list    | Step definitions (sequence scenes only)  |

### Trigger fields

| Field     | Required | Type   | Description                    |
|-----------|----------|--------|--------------------------------|
| `channel` | yes      | int    | MIDI channel (1-16)            |
| `note`    | yes      | int    | MIDI note number (0-127)       |
| `mode`    | yes      | string | `gate` or `toggle`             |

### Step fields (sequence scenes)

| Field         | Required | Type    | Description                    |
|---------------|----------|---------|--------------------------------|
| `duration-ms` | yes      | int     | Step duration in milliseconds  |
| `transition`  | yes      | string  | `hold` or `linear`            |
| `values`      | yes      | mapping | Target/value pairs for step    |

### Values syntax

Values are a mapping of target to DMX value:

```yaml
values:
  fixture-id.channel-name: 0-255
```

---

## Minimal Complete Example

```yaml
format:
  name: spark-project
  version: 1

app:
  name: my-show

midi:
  mode: create-virtual
  device: spark

dmx:
  backend: dummy

fixtures:
  - id: par1
    start-address: 1
    channel-count: 4
    channels:
      - name: dimmer
        offset: 0
      - name: red
        offset: 1
      - name: green
        offset: 2
      - name: blue
        offset: 3

scenes:
  - id: warm
    type: static
    trigger:
      channel: 1
      note: 60
      mode: gate
    values:
      par1.dimmer: 200
      par1.red: 255
      par1.green: 120
      par1.blue: 0
```

## Directory Mode Example

```
my-show/
  project.yaml       # main manifest with includes
  fixtures.yaml      # fixture definitions
  scenes.yaml        # scene definitions
```

```yaml
# project.yaml
format:
  name: spark-project
  version: 1

midi:
  mode: open-existing
  device: "Launchpad Mini"

dmx:
  backend: open
  device: /dev/ttyUSB0

includes:
  fixtures: fixtures.yaml
  scenes: scenes.yaml
```
