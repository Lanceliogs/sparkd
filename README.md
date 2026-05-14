# sparkd

A show-safe MIDI-to-DMX lighting engine.

`sparkd` is a C daemon that receives MIDI, maps triggers to lighting scenes, and outputs DMX through ENTTEC-compatible interfaces. It embeds an HTTP API and serves a web UI for configuration and monitoring.

## Build

```bash
make
```

## Run

```bash
./build/sparkd --project projects/example --log-level debug
```

## Dependencies

All vendored -- no system packages required beyond a C compiler and GNU Make.

| Component | Location |
|-----------|----------|
| portmidi | `vendor/portmidi/` |
| mongoose | `vendor/mongoose/` |
| libyaml | `vendor/libyaml/` |

## Project Layout

```
daemon/     C daemon source
tools/      sparkctl CLI
vendor/     vendored dependencies
ui/         Svelte web UI
reaper/     REAPER integration scripts
projects/   example project files
specs/      specification
```
