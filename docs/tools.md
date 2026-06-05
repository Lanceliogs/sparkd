# CLI Tools

sparkd ships with several command-line tools for debugging hardware and integrating with external software.

## spark-midi — MIDI Debugging

A standalone tool for listing and monitoring MIDI devices. Use it to find your device name and verify MIDI input before configuring sparkd.

### Commands

#### list

Lists all available MIDI input devices detected by PortMidi.

```bash
$ spark-midi list
MIDI input devices (2):
  [1] Arturia KeyStep 32
  [3] loopMIDI Port
```

Use the device name (or a substring) in your project's `midi.device` field.

#### listen

Opens one or more MIDI input devices and prints incoming messages in real-time.

```bash
$ spark-midi listen "KeyStep"
Listening on: [1] Arturia KeyStep 32
--- Ctrl+C to stop ---
  NOTE_ON   ch= 1  note= 60  vel=100
  NOTE_OFF  ch= 1  note= 60  vel=  0
  CC        ch= 1  cc=  1  val= 64
```

Multiple patterns can be given to listen on several devices at once:

```bash
$ spark-midi listen "KeyStep" "loopMIDI"
Listening on: [1] Arturia KeyStep 32
Listening on: [3] loopMIDI Port
```

The pattern is a substring match against device names (case-insensitive).

### Typical workflow

```bash
# 1. Find your device
spark-midi list

# 2. Verify it sends the expected events (press keys, twist knobs)
spark-midi listen "My Controller"

# 3. Note the channel and note numbers, use them in your project's scenes
```

---

## spark-serial — Serial/DMX Debugging

A standalone tool for listing USB-serial devices and testing DMX auto-detection. Use it to find your DMX interface's port name and verify it's recognized.

### Commands

#### list

Lists all USB-serial devices with VID:PID, description, and serial number. Known DMX devices are flagged.

```bash
$ spark-serial list
USB-serial devices (2):

  [0] COM3
      VID:PID  0403:6001 (known DMX device)
      Desc     FTDI FT232R
      Serial   A50285BI

  [1] COM7
      VID:PID  1a86:7523
      Desc     USB-SERIAL CH340
```

#### find

Auto-detects a DMX interface using the same logic as `auto` in the project file. Optionally filter by manufacturer tag.

```bash
$ spark-serial find
Searching for DMX device...

Found DMX device:
  Port:    COM3
  VID:PID: 0403:6001
  Desc:    FTDI FT232R
  Serial:  A50285BI

Use in project YAML:
  dmx:
    device: COM3
    backend: open   # or 'pro' for DMX USB Pro
```

Filter by manufacturer:

```bash
$ spark-serial find enttec
Searching for DMX device (filter: enttec)...

Found DMX device:
  Port:    COM3
  VID:PID: 0403:6001
```

Available tags: `ftdi`, `enttec`, `eurolite`

### Typical workflow

```bash
# 1. Plug in your USB-DMX interface
# 2. Find it
spark-serial list

# 3. Test auto-detection
spark-serial find

# 4. Use the port in your project, or use "auto"
```

### Troubleshooting

| Problem | Solution |
|---------|----------|
| Device not listed | Check USB connection, try a different cable/port |
| Listed but not "known" | Use the port path directly instead of `auto` |
| `find` returns nothing | Device VID:PID not in the known list — specify port manually |
| Wrong backend | Try both `open` and `pro` — only one will work |

---

## spark-reaper — REAPER Integration

Generates note-name files for REAPER's MIDI editor from a sparkd project. This lets you see scene names directly on the MIDI piano roll when composing triggers.

### Commands

#### note-names

Parses a project file and generates `.txt` note-name files (one per MIDI channel that has scenes assigned).

```bash
$ spark-reaper note-names project.spark.yaml
Generated: ch01.txt (4 entries)
Generated: ch02.txt (2 entries)
```

Output files use REAPER's note-name format:

```
60 Red Wash
61 Color Cycle
62 White Strobe
```

Import these in REAPER via: *MIDI Editor > File > Note/CC Names > Load note names from file...*

### Options

| Option | Description |
|--------|-------------|
| `-o DIR` | Output directory (default: current directory) |
| `--all` | Include disabled scenes (suffixed with " [disabled]") |

### Examples

```bash
# Generate to default location
spark-reaper note-names my-show.spark.yaml

# Generate to a specific directory
spark-reaper note-names -o reaper/note-names/ my-show.spark.yaml

# Include disabled scenes
spark-reaper note-names --all my-show.spark.yaml
```

### Typical workflow

```bash
# 1. Design your scenes in sparkd (assign MIDI triggers)
# 2. Generate note-name files
spark-reaper note-names -o ~/reaper-projects/my-show/ project.spark.yaml

# 3. In REAPER, load the note names for the relevant MIDI track
# 4. Now your piano roll shows scene names instead of note numbers
```

---

## Common Options

All tools support:

| Option | Description |
|--------|-------------|
| `--version` | Print version and exit |
| `--help`, `-h` | Print usage and exit |
