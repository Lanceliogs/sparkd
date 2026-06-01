# sparkctl - CLI Control Tool

`sparkctl` is a command-line tool for controlling `sparkd` and `spark-ui` — managing their lifecycle and controlling the engine via HTTP.

## Usage

```
sparkctl [--http ADDR] <command> [args...]
```

## Lifecycle Commands

| Command | Description |
|---------|-------------|
| `daemon up [OPTS]` | Start sparkd in background (OPTS passed through to sparkd) |
| `daemon down` | Stop sparkd gracefully |
| `daemon status` | Check if sparkd is running (pid, uptime) |
| `ui up [OPTS]` | Start spark-ui in background (OPTS passed through to spark-ui) |
| `ui down` | Stop spark-ui gracefully |
| `ui status` | Check if spark-ui is running (pid, uptime) |

## Engine Commands

| Command | Argument | Description |
|---------|----------|-------------|
| `status` | | Get engine state (running, blackout, project) |
| `start` | | Start the engine (MIDI + DMX) |
| `stop` | | Stop the engine |
| `set-blackout` | | Enable blackout |
| `clear-blackout` | | Disable blackout |
| `reload` | [PATH] | Reload project file (engine must be stopped) |
| `reconnect-midi` | | Reconnect the MIDI device |
| `healthz` | | Health check (version, pid, uptime) |

## Options

| Option | Argument | Default | Description |
|--------|----------|---------|-------------|
| `--http` | ADDR | `127.0.0.1:7600` | Daemon address to connect to |
| `--help`, `-h` | | | Print usage |

## Environment Variables

| Variable | Description | Equivalent CLI |
|----------|-------------|----------------|
| `SPARK_HTTP_ADDR` | Default daemon address | `--http` |
| `SPARK_UI_HTTP_ADDR` | Default spark-ui address | `--http` (within `ui` subcommand) |

Priority: CLI argument > environment variable > `.spark.env` file > built-in default.

## Lifecycle Commands Detail

### daemon up

Starts `sparkd` as a detached background process. Polls `/healthz` to confirm it's up.

```bash
$ sparkctl daemon up
sparkd: starting...
sparkd: running (pid 1234)

# With passthrough args
$ sparkctl daemon up --project my-show.spark.yaml --auto
sparkd: starting...
sparkd: running (pid 1235)
```

If sparkd is already running, returns immediately:
```bash
$ sparkctl daemon up
sparkd: already running (pid 1234)
```

Logs are written to `~/.spark/sparkd.log`.

### daemon down

Sends a graceful shutdown request to sparkd via `POST /shutdown`.

```bash
$ sparkctl daemon down
sparkd: stopped

# Idempotent — safe to call when not running
$ sparkctl daemon down
sparkd: not running
```

### daemon status

```bash
$ sparkctl daemon status
sparkd: running (pid 1234, uptime 42s)
```

### ui up / ui down / ui status

Same pattern as `daemon`, targeting spark-ui on port 7601.

```bash
$ sparkctl ui up --open-browser
spark-ui: starting...
spark-ui: running (pid 5678)

$ sparkctl ui down
spark-ui: stopped
```

Logs are written to `~/.spark/spark-ui.log`.

---

## Engine Commands Detail

### status

Returns the current engine state.

```bash
$ sparkctl status
running:  yes
blackout: no
project:  /home/user/my-show.spark.yaml
```

### start

Starts the engine. Fails if already running.

```bash
$ sparkctl start
{"status":"started"}
```

### stop

Stops the engine. Fails if not running.

```bash
$ sparkctl stop
{"status":"stopped"}
```

### reload [PATH]

Reloads a project file. The engine must be stopped first.

```bash
# Reload a specific project
sparkctl reload projects/example/project.spark.yaml

# Reload the last loaded project (e.g. after editing the YAML)
sparkctl reload
```

If no path is given, reuses the last project path that was loaded.
Fails with an error if no previous project was loaded and no path is provided.

### reconnect-midi

Attempts to reconnect the MIDI device. Useful if the device was unplugged
and reconnected.

```bash
$ sparkctl reconnect-midi
{"status":"reconnected"}
```

### healthz

Returns daemon health information.

```bash
$ sparkctl healthz
{"version":"0.1.0","pid":12345,"uptime_ms":84200}
```

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Command succeeded (HTTP 2xx) |
| 1 | Command failed (HTTP 4xx/5xx, connection error, or usage error) |

## Common Workflows

### Full startup

```bash
sparkctl daemon up --project my-show.spark.yaml --auto
sparkctl ui up --open-browser
```

### Full shutdown

```bash
sparkctl ui down
sparkctl daemon down
```

### Edit and reload

```bash
sparkctl stop
vim my-show.spark.yaml
sparkctl reload
sparkctl start
```

### Remote daemon

```bash
sparkctl --http 192.168.1.50:7600 status
```

### Using environment variable

```bash
export SPARK_HTTP_ADDR=192.168.1.50:7600
sparkctl status
sparkctl start
```
