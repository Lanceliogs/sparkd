# sparkd

A show-safe MIDI-to-DMX lighting engine.

`sparkd` is a C daemon that receives MIDI, maps triggers to lighting scenes, and outputs DMX through ENTTEC-compatible interfaces. It embeds an HTTP API for runtime control and can serve a web UI for configuration and monitoring.

## Quick Start

```bash
# Build everything
make

# Run with a project file (daemon starts idle, use sparkctl to start engine)
./build/sparkd --project projects/example/project.spark.yaml

# Or auto-start the engine immediately
./build/sparkd --project projects/example/project.spark.yaml --auto

# Control the running daemon
./build/tools/sparkctl status
./build/tools/sparkctl start
./build/tools/sparkctl stop
./build/tools/sparkctl reload
```

## Build

Requires a C compiler (gcc or clang) and GNU Make. No other system dependencies.

```bash
make            # builds sparkd + sparkctl + tools
make test       # runs the test suite
make clean      # removes build artifacts
```

On Linux, ALSA development headers are needed for MIDI (`libasound2-dev` on Debian/Ubuntu).

### Build Targets

| Target | Output | Description |
|--------|--------|-------------|
| `make` | `build/sparkd` | Daemon binary |
| `make tools` | `build/tools/sparkctl`, `build/tools/spark-midi` | CLI tools |
| `make test` | `build/tests/test_*` | Unit test binaries |
| `make clean` | | Remove build directory |

## Project Layout

```
daemon/             C daemon source and Makefile
  src/              daemon modules
  src/project/      YAML project loading
  tests/            unit tests
  tools/            sparkctl and MIDI tools source
vendor/             vendored dependencies (no install needed)
  portmidi/         cross-platform MIDI
  mongoose/         embedded HTTP server
  libyaml/          YAML parser
projects/           example project files
specs/              design specs and format docs
docs/               user documentation
```

## Dependencies

All vendored -- zero system packages required beyond a C compiler and Make:

| Library | Purpose | Location |
|---------|---------|----------|
| portmidi | Cross-platform MIDI (ALSA on Linux, WinMM on Windows) | `vendor/portmidi/` |
| mongoose | Embedded HTTP/WebSocket server | `vendor/mongoose/` |
| libyaml | YAML parser (v0.2.5) | `vendor/libyaml/` |

## Documentation

- [Daemon usage](docs/daemon.md) -- CLI arguments, environment variables, lifecycle
- [sparkctl usage](docs/sparkctl.md) -- CLI commands and options
- [spark-midi usage](docs/tools.md) -- MIDI debugging tool
- [YAML format](specs/yaml-format.md) -- project file format reference
- [Full specification](specs/sparkd.md) -- complete system design

## License

See [LICENSE](LICENSE).
