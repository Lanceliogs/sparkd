# Getting Started

This guide walks you through installing sparkd and launching the UI for the first time.

## Installation

### Windows

Download `sparkd_vX.X.X_win64_setup.exe` from the [Releases](https://github.com/music-music/sparkd/releases) page and run it. The installer:

- Extracts to `%LOCALAPPDATA%\sparkd` (no admin required)
- Adds the `bin/` directory to your user PATH
- Creates a **Sparkd** shortcut in the Start Menu

### Linux (tarball)

```bash
tar xzf sparkd-vX.X.X-linux-x64.tar.gz
cd sparkd-vX.X.X-linux-x64
sudo ./install.sh
```

This installs to `/opt/sparkd` and symlinks binaries to `/usr/local/bin`.

### Linux (deb)

```bash
sudo dpkg -i sparkd_X.X.X_amd64.deb
```

### Build from source

```bash
git clone https://github.com/music-music/sparkd.git
cd sparkd
make
sudo make install
```

Requires: GCC/Clang, GNU Make, `libasound2-dev` (Linux).

## First Launch

### Windows

Double-click **Sparkd** in the Start Menu. This starts the daemon and the UI server, then opens your browser automatically.

If you prefer the terminal:

```
sparkctl daemon up
sparkctl ui up
sparkctl ui open
```

### Linux

```bash
sparkctl daemon up
sparkctl ui up
sparkctl ui open
```

## Verify It Works

Your browser should open to `http://127.0.0.1:7601`. You'll see the Live view:

![Live view — not running](images/live_not_running.png)

The status shows "not running" because no project is loaded yet. Head to [Project Setup](project-setup.md) to create your first project.

## Stopping

### Windows

Close the browser tab. The services keep running in the background. To fully stop:

```
sparkctl ui down
sparkctl daemon down
```

### Linux

```bash
sparkctl ui down
sparkctl daemon down
```

## Next Steps

- [Project Setup](project-setup.md) — Create a project with fixtures and scenes
- [Web UI Guide](ui-guide.md) — Learn how to navigate the interface
- [Configuration](configuration.md) — Customize paths, banks, and environment
