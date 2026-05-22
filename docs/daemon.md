# sparkd - Daemon

`sparkd` is the main daemon process. It loads a project file, manages MIDI input, scene state, and DMX output, and exposes an HTTP API for control.

## Usage

```
sparkd [OPTIONS]
```

## Options

| Option | Argument | Default | Description |
|--------|----------|---------|-------------|
| `--project` | PATH | none | Project file to load (`.spark.yaml`) |
| `--auto` | | off | Auto-start the engine after loading the project |
| `--http` | HOST:PORT | `127.0.0.1:7600` | HTTP listen address |
| `--log-level` | LEVEL | `info` | Log verbosity: `debug`, `info`, `warn`, `error` |
| `--validate` | | | Validate project file and exit (requires `--project`) |
| `--version` | | | Print version and exit |
| `--help` | | | Print usage and exit |

MIDI and DMX configuration is defined in the project YAML file (not CLI args).

## Environment Variables

Environment variables are read before CLI arguments. CLI arguments override them.
Variables can be defined in a `.spark.env` file (searched in CWD, then `~/.spark/.spark.env`).
Real env vars always take priority over file values.

| Variable | Description | Equivalent CLI |
|----------|-------------|----------------|
| `SPARK_PROJECT_PATH` | Default project file path | `--project` |
| `SPARK_HTTP_ADDR` | HTTP listen address (`host:port`) | `--http` |
| `SPARK_FIXTURE_BANK_PATH` | Semicolon-separated fixture bank directories | — |

## Lifecycle Modes

### Idle mode (default)

```bash
sparkd --project my-show.spark.yaml
```

The daemon loads the project, starts HTTP, and waits. The engine is not running.
Use `sparkctl start` to bring up MIDI + DMX.

### Auto-start mode

```bash
sparkd --project my-show.spark.yaml --auto
```

The daemon loads the project and immediately starts the engine (MIDI + DMX).
Equivalent to idle mode followed by `sparkctl start`.

### Validate mode

```bash
sparkd --project my-show.spark.yaml --validate
```

Parses and resolves the project file, then exits. Returns exit code 0 if valid,
non-zero with error details if not.

## Engine Lifecycle

```
init -> load_project -> [start -> stop]* -> destroy
```

The engine can be started and stopped multiple times. A project reload
(`sparkctl reload`) requires the engine to be stopped first.

Typical flow:

```bash
sparkctl stop                     # stop MIDI + DMX
sparkctl reload                   # reload last project (after editing YAML)
sparkctl start                    # resume MIDI + DMX
```

## HTTP API

The daemon listens on the configured HTTP address (default `127.0.0.1:7600`).

### Endpoints

| Method | Path | Description |
|--------|------|-------------|
| GET | `/healthz` | Health check (version, pid, uptime) |
| GET | `/api/engine/state` | Engine status (running, blackout, project, active scenes) |
| POST | `/api/engine/start` | Start the engine (uses config from loaded project) |
| POST | `/api/engine/stop` | Stop the engine |
| GET | `/api/engine/blackout` | Get blackout state |
| POST | `/api/engine/blackout` | Set blackout `{"enabled": true/false}` |
| POST | `/api/engine/midi/reconnect` | Reconnect MIDI device |
| POST | `/api/project/reload` | Reload project (engine must be stopped) |
| POST | `/api/fixtures/bank/reload` | Reload fixture bank from search paths |
| GET | `/api/scenes` | List scene definitions |
| POST | `/api/scenes/{id}/activate` | Activate a scene |
| POST | `/api/scenes/{id}/release` | Release a scene |
| — | `/ws` | WebSocket for live events |

### POST /api/engine/start

No request body needed. Uses MIDI and DMX configuration from the loaded project.

### POST /api/project/reload

Request body (optional):

```json
{"path": "/absolute/or/relative/path.spark.yaml"}
```

If `path` is omitted, reloads the last loaded project file.

## Signals

| Signal | Behavior |
|--------|----------|
| `SIGINT` (Ctrl+C) | Graceful shutdown |
| `SIGTERM` | Graceful shutdown |

## Exit Codes

| Code | Meaning |
|------|---------|
| 0 | Clean exit / validation passed |
| 1 | Argument error |
| non-zero | Startup or validation failure |

## Examples

```bash
# Development with debug logging
sparkd --project projects/example/project.spark.yaml --auto --log-level debug

# Production with env vars
export SPARK_PROJECT_PATH=/opt/show/main.spark.yaml
export SPARK_HTTP_ADDR=0.0.0.0:8080
sparkd --auto

# Validate before deploying
sparkd --project new-show.spark.yaml --validate && echo "OK"
```
