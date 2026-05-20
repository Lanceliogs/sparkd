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

fixtures: []
scenes: []
```

### Recognized sections

| Section            | Status       | Description                         |
|--------------------|--------------|-------------------------------------|
| `format`           | parsed       | Format identifier and version       |
| `app`              | parsed       | Project identity                    |
| `fixtures`         | parsed       | Physical lights and channels        |
| `scenes`           | parsed       | Lighting states (static + sequence) |
| `fixture-templates`| skipped      | Reusable channel layout helpers     |
| `mappings`         | skipped      | MIDI trigger to action bindings     |
| `http`             | skipped      | HTTP server config                  |
| `midi`             | skipped      | MIDI input config                   |
| `dmx`              | skipped      | DMX output config                   |
| `safety`           | skipped      | Heartbeat surveillance config       |
| `reaper`           | skipped      | REAPER export config                |
| `ui`               | skipped      | UI preferences                      |

Skipped sections are read without error but their content is not used yet.
Unknown keys at the top level produce a warning and are skipped.

---

## Naming Conventions

- All YAML keys use **kebab-case**: `start-address`, `channel-count`, `duration-ms`.
- IDs must match `[a-zA-Z0-9_-]+`.
- User-facing MIDI channels are **1-based** (1-16).
- User-facing DMX addresses are **1-based** (1-512).
- DMX values are 0-255.

---

## Fixtures

A fixture represents one physical light patched at a DMX start address.

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

### Fixture fields

| Field           | Required | Type   | Description                          |
|-----------------|----------|--------|--------------------------------------|
| `id`            | yes      | string | Unique identifier                    |
| `name`          | no       | string | Display name (defaults to `id`)      |
| `start-address` | yes      | int    | DMX start address (1-512)            |
| `channel-count` | yes      | int    | Number of DMX channels               |
| `channels`      | yes      | list   | Channel definitions                  |

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
