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

### Prerequisites

- C compiler (gcc or clang) and GNU Make
- `libasound2-dev` (Debian/Ubuntu) for MIDI support on Linux
- Node.js and npm (optional — only needed to rebuild the web UI from source)

If you don't have Node.js, you can download pre-built UI assets instead:

```bash
make ui-fetch       # downloads pre-built UI for the current version
```

### Quick Build

```bash
make                # builds sparkd + all tools
make test           # runs the test suite
sudo make install   # installs to /opt/sparkd, symlinks to /usr/local/bin
```

### Build Targets

| Target | Description |
|--------|-------------|
| `make` | Build daemon + all tools |
| `make tools` | Build CLI tools (auto-builds UI if missing) |
| `make test` | Run the test suite |
| `make install` | Install to `/opt/sparkd`, symlink binaries to `/usr/local/bin` |
| `make uninstall` | Remove installation and symlinks |
| `make clean` | Remove C build artifacts |
| `make ui-rebuild` | Force-rebuild the web UI (requires npm) |
| `make ui-fetch` | Download pre-built UI assets from GitHub release |
| `make ui-clean` | Remove `ui/dist/` |
| `make dist` | Create Linux tarball |
| `make deb` | Create `.deb` package |
| `make installer` | Create Windows installer (cross-compile) |

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

## Pre-built Releases

Pre-built binaries are available on the [Releases](https://github.com/music-music/sparkd/releases) page.

### Windows

Download `sparkd_vX.X.X_win64_setup.exe` and run it. Installs to `%LOCALAPPDATA%\sparkd` (no admin required), adds to PATH, and creates a Start Menu shortcut. An uninstaller is included.

### Linux

**Tarball:** Extract `sparkd-vX.X.X-linux-x64.tar.gz` and run `./install.sh` (installs to `/opt/sparkd`, symlinks to `/usr/local/bin`).

**Debian/Ubuntu:** `sudo dpkg -i sparkd_X.X.X_amd64.deb`

### System Requirements

- **Windows:** 64-bit Windows 10 or later.
- **Linux:** x86_64 with glibc 2.34+ (Ubuntu 22.04+, Debian 12+, Fedora 35+). Requires `libasound2` for MIDI support.

Older Linux distributions are not supported by the pre-built binaries. Build from source if you need to target an older system.

## License

See [LICENSE](LICENSE).
