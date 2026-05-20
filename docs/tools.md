# spark-midi - MIDI Debugging Tool

A standalone tool for listing and monitoring MIDI devices. Useful for finding
device names and verifying MIDI input before configuring `sparkd`.

## Usage

```
spark-midi <command> [args...]
```

## Commands

### list

Lists all available MIDI input devices detected by PortMidi.

```bash
$ spark-midi list
MIDI input devices (2):
  [1] Arturia KeyStep 32
  [3] loopMIDI Port
```

Use the device name (or a substring of it) with `spark-midi listen` or `sparkd --midi`.

### listen

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
--- Ctrl+C to stop ---
```

The pattern is a substring match against device names (case-sensitive).

## Typical Workflow

```bash
# Find your device
spark-midi list

# Verify it sends the expected events
spark-midi listen "My Controller"

# Then configure sparkd
sparkd --project my-show.spark.yaml --midi "My Controller" --auto
```
