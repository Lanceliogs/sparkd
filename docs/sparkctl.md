# sparkctl - CLI Control Tool

`sparkctl` is a command-line tool for controlling a running `sparkd` instance via its HTTP API.

## Usage

```
sparkctl [OPTIONS] <command> [ARG]
```

## Commands

| Command | Argument | Description |
|---------|----------|-------------|
| `status` | | Get engine state (running, backend, port, MIDI device) |
| `start` | | Start the engine (MIDI + DMX) |
| `stop` | | Stop the engine |
| `reload` | [PATH] | Reload project file (engine must be stopped) |
| `reconnect-midi` | | Reconnect the MIDI device |
| `healthz` | | Health check (version, pid, uptime) |

## Options

| Option | Argument | Default | Description |
|--------|----------|---------|-------------|
| `--http` | ADDR | `http://127.0.0.1:7600` | Daemon address to connect to |
| `--help`, `-h` | | | Print usage |

## Environment Variables

| Variable | Description | Equivalent CLI |
|----------|-------------|----------------|
| `SPARK_HTTP_ADDR` | Default daemon address | `--http` |

Priority: CLI argument > environment variable > built-in default.

## Commands Detail

### status

Returns the current engine state as JSON.

```bash
$ sparkctl status
{"running":true,"dmx_backend":"open","dmx_port":"COM3","midi_device":""}
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

### Start/stop cycle

```bash
sparkctl start        # bring up MIDI + DMX
sparkctl stop         # tear down
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
sparkctl --http http://192.168.1.50:7600 status
```

### Using environment variable

```bash
export SPARK_HTTP_ADDR=http://192.168.1.50:7600
sparkctl status
sparkctl start
```
