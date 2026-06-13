# Project Setup

This guide walks you through creating a project from scratch using the web UI.

## Opening / Creating a Project

1. Launch sparkd and open the UI (see [Getting Started](getting-started.md))
2. Switch to the **EDITOR** tab
3. Click **NEW** to create a blank project, or open an existing `.spark.yaml` file

The file browser lets you navigate to any location on your system:

![File browser](images/file_browser.png)

Once opened, you'll see the project path in the toolbar:

![Project opened](images/project_opened.png)

## Step 1: Fixture Banks

Before adding fixtures, check that your fixture banks are loaded. The **BANKS** tab shows available template libraries:

![Banks tab](images/editor_banks.png)

Banks provide reusable fixture definitions (channel layouts) so you don't have to define channels manually every time. sparkd loads banks from:

- `~/.spark/fixtures/` (default)
- Directories listed in `SPARK_FIXTURE_BANK_PATH`

If you don't have banks yet, you can still add fixtures manually (see below).

## Step 2: Add Fixtures

Switch to the **FIXTURES** tab and click **+ ADD**:

![Fixtures tab](images/editor_fixtures.png)

You have three options:

### From a template (recommended)

Select a bank and fixture template. The channel layout is filled automatically:

![Add fixture from template](images/fixture_form_template.png)

You only need to provide:
- **ID** — unique identifier (e.g. `par-left`)
- **Start address** — DMX address where this fixture begins (1-512)

### Manual

Define each channel by hand:

![Add fixture manually](images/fixture_form_manual.png)

Provide:
- **ID** and **Start address**
- **Channel count** — how many DMX channels this fixture uses
- **Channels** — name and offset for each channel

### Copy from existing

Duplicate the channel layout from a fixture already in your project:

![Copy from existing](images/fixture_form_copy_from.png)

Useful when you have multiple identical lights at different addresses.

## Step 3: Configure Hardware

Switch to the **HARDWARE** tab to set up your MIDI and DMX devices:

![Hardware tab](images/editor_hardware.png)

### MIDI

- **Device** — the name (or part of the name) of your MIDI controller. sparkd does a case-insensitive substring match, so `"KeyStep"` matches `"Arturia KeyStep 32"`. Leave empty to disable MIDI input entirely.
- **Mode**:
  - `open-existing` — connect to a physical or virtual MIDI port already present on the system
  - `create-virtual` — create a new virtual MIDI port (Linux/macOS only). Useful for routing MIDI from a DAW like REAPER or Ableton.

![MIDI modes](images/editor_hardware_midi_mode.png)

#### Troubleshooting MIDI

Use the [`spark-midi`](tools.md#spark-midi--midi-debugging) CLI tool to debug MIDI issues:

```bash
# List all MIDI input devices detected on your system
spark-midi list

# Monitor incoming messages from a device (Ctrl+C to stop)
spark-midi listen "KeyStep"
```

If `spark-midi list` doesn't show your device, check that it's plugged in and that no other application has exclusive access to it.

### DMX

- **Device** — the serial port where your USB-DMX interface is connected. On Windows this is a COM port (e.g. `COM3`), on Linux a `/dev/tty` path (e.g. `/dev/ttyUSB0`).
- **Backend** — the DMX protocol to use:
  - `open` — Open DMX USB protocol (break + raw frames). Used by most generic USB-DMX interfaces (FTDI-based clones, Eurolite USB-DMX512, etc.).
  - `pro` — Enttec Pro protocol (packetized serial). Used by Enttec DMX USB Pro and compatible interfaces.
  - `artnet` — Art-Net protocol (ArtDmx over UDP). Used by Ethernet/WiFi Art-Net nodes. The device field is the node's IP address.
  - `dummy` — No DMX output. The engine runs normally but nothing is sent. Great for testing without hardware.
- **Refresh Hz** — DMX update rate (1-44 Hz, default 25). 25 Hz is a good balance between responsiveness and bus stability. Some cheap fixtures misbehave above 30 Hz.

![DMX backends](images/editor_hardware_dmx_backend.png)

#### Auto-detection

Instead of specifying a serial port manually, you can use `auto` as the device value. sparkd will scan your USB-serial devices and find a known DMX interface automatically.

You can also filter by manufacturer tag:

| Device value | Behavior |
|--------------|----------|
| `auto` | Find any known DMX device (FTDI, Enttec, Eurolite) |
| `auto:enttec` | Find Enttec devices only |
| `auto:eurolite` | Find Eurolite devices only |
| `auto:ftdi` | Find any FTDI-based device |
| IP address (e.g. `192.168.1.100`) | Art-Net node (used with `artnet` backend) |

Auto-detection resolves at engine start time — it finds the port and then opens it normally. For the `artnet` backend, the device field is always an IP address (no auto-detection).

#### Troubleshooting DMX

Use the [`spark-serial`](tools.md#spark-serial--serialdmx-debugging) CLI tool to debug DMX/serial issues:

```bash
# List all USB-serial devices with VID:PID info
spark-serial list

# Auto-detect a DMX device (same logic as "auto" in the project)
spark-serial find

# Filter by manufacturer
spark-serial find enttec
```

If `spark-serial list` shows your interface but `spark-serial find` doesn't match it, the device might use an unknown VID:PID. In that case, specify the port manually (e.g. `COM3` or `/dev/ttyUSB0`).

#### Art-Net (Ethernet DMX)

For Art-Net nodes (Ethernet/WiFi DMX interfaces), select the `artnet` backend and enter the node's IP address in the device field. Sparkd sends ArtDmx packets via UDP unicast to port 6454.

- Make sure the Art-Net node is on the same network (or reachable via routing)
- Use `ping <ip>` to verify connectivity before configuring
- No auto-detection is available for Art-Net — you must specify the IP manually

> **Not sure which backend to use?** If your interface is on Ethernet/WiFi, use `artnet`. For USB interfaces, try `open` first — it works with the vast majority. If you have an Enttec Pro (or compatible), use `pro`. Only the correct protocol will produce output on your fixtures.

## Step 4: Create Scenes

Switch to the **SCENES** tab and click **+ ADD SCENE**:

![Scenes tab](images/editor_scenes.png)

### Static Scene

A static scene applies fixed DMX values while active:

![Static scene form](images/scene_form_static.png)

Configure:
- **ID** — unique identifier (e.g. `red-wash`)
- **Name** — display name (shown on the Live pad)
- **Trigger** — MIDI channel (1-16), note (0-127), and mode:
  - `gate` — active while the key is held
  - `toggle` — press to activate, press again to release
- **Values** — set DMX values for each fixture channel (0-255)

### Sequence Scene

A sequence scene cycles through timed steps:

![Sequence scene form](images/scene_form_sequence.png)

Configure:
- Same trigger settings as static
- **Loop** — whether the sequence repeats
- **Steps** — each step has:
  - Duration (milliseconds)
  - Transition: `snap` (instant) or `linear` (smooth crossfade)
  - Values for that step

## Step 5: Save

Click **SAVE** in the toolbar to write your project to disk. The file is a `.spark.yaml` that you can also edit by hand (see [Project File Format](project-format.md)).

Use **SAVE AS** to save a copy to a different location.

An indicator appears when you have unsaved changes:

![Unsaved changes](images/editor_project_unsaved.png)

## Step 6: Go Live

1. Switch to the **LIVE** tab
2. Click **Start** to activate the engine
3. Your scenes appear as pads — click them or trigger via MIDI

See [Web UI Guide](ui-guide.md) for more on the Live view.

## Error Handling

If your project file has issues, sparkd shows a detailed error with the file path and line number:

![Parse error](images/parse_error.png)

Fix the issue in the Editor and save again, or edit the YAML file directly and use **Reload**.

## Next Steps

- [Web UI Guide](ui-guide.md) — Deep dive into the UI features
- [Configuration](configuration.md) — Set up fixture banks and project roots
- [Project File Format](project-format.md) — Hand-edit YAML like a pro
