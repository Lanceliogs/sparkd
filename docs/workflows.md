# Common Workflows

Real-world usage patterns for sparkd.

## Windows Quick Start

1. Install sparkd (run the setup exe)
2. Double-click **Sparkd** in the Start Menu
3. Browser opens with the UI — you're ready to go

Everything runs in the background. No terminal needed.

## Linux Quick Start

```bash
sparkctl daemon up
sparkctl ui up
sparkctl ui open
```

To auto-load a project on startup:

```bash
sparkctl daemon up --project ~/shows/my-show.spark.yaml --auto
sparkctl ui up
sparkctl ui open
```

## Live Show

A typical performance workflow:

1. **Prepare:** Load your project in the Editor, verify fixtures and scenes, save
2. **Start:** Switch to Live, click **Start** (or use `sparkctl start`)
3. **Perform:** Trigger scenes via MIDI controller or click pads in the UI
4. **Emergency:** Hit **Blackout** to cut all output instantly
5. **End:** Click **Stop** or `sparkctl stop`

### Using a phone as a remote

1. Set `SPARK_UI_HTTP_ADDR=0.0.0.0:7601` in `.spark.env`
2. Restart spark-ui (`sparkctl ui down && sparkctl ui up`)
3. In the UI, click **SHARE**
4. Scan the QR code with your phone (must be on the same network)
5. Your phone gets a Live view with scene pads, start/stop, and blackout

## Edit-Reload Cycle

When tweaking a show without restarting:

### Via UI

1. Click **Stop** in Live view
2. Switch to **Editor**, make changes (fixtures, scenes, hardware)
3. Click **SAVE**
4. Switch back to **Live**, click **Reload**, then **Start**

### Via terminal

```bash
sparkctl stop
# edit the YAML file with your editor of choice
sparkctl reload
sparkctl start
```

### Via terminal (different project file)

```bash
sparkctl stop
sparkctl reload ~/shows/other-project.spark.yaml
sparkctl start
```

## Remote Control (Headless)

Run sparkd on a dedicated machine (e.g. a Raspberry Pi) and control it from another device:

### On the sparkd machine

```bash
# .spark.env
SPARK_UI_HTTP_ADDR=0.0.0.0:7601
SPARK_PROJECT_PATH=/home/pi/show.spark.yaml
SPARK_AUTH_TOKEN=my-secure-token
```

```bash
sparkctl daemon up --auto
sparkctl ui up
```

spark-ui proxies API requests to the local daemon, so usually only spark-ui needs to be network-exposed.

### From another machine

Open `http://<sparkd-ip>:7601` in a browser for the full UI. Everything goes through spark-ui.

## CLI-Only Workflow

For automation and scripting, use `sparkctl` directly. It handles auth tokens automatically (reads `SPARK_AUTH_TOKEN` from the environment or `.spark.env`), so you don't need to worry about Bearer headers. Direct HTTP API calls are still possible if you prefer, but sparkctl is the easier path.

```bash
# Start daemon with a project
sparkctl daemon up --project my-show.spark.yaml --auto

# Check status
sparkctl status

# Control the engine
sparkctl start
sparkctl stop
sparkctl set-blackout
sparkctl clear-blackout

# Reload after editing
sparkctl stop
sparkctl reload
sparkctl start

# Shut down
sparkctl daemon down
```

## Testing Without Hardware

Use the `dummy` DMX backend to test scenes without a physical DMX interface:

```yaml
dmx:
  backend: dummy
```

Or in `.spark.env`:

```bash
# No DMX device needed
```

Simply leave the DMX device field empty in the Hardware tab. The engine runs normally — you just won't see any light output.

For MIDI, you can use `create-virtual` mode to create a virtual MIDI port, then send notes from a DAW or virtual keyboard:

```yaml
midi:
  mode: create-virtual
  device: sparkd
```

## Validate Before Deploying

Before a show, validate your project file without starting the engine:

```bash
sparkd --project my-show.spark.yaml --validate
```

Returns exit code 0 if valid, or prints detailed errors if not.

## Multiple Shows

Keep each show in its own directory with its own project file:

```
shows/
  club-night/
    project.spark.yaml
    fixtures.yaml
    scenes.yaml
  corporate/
    project.spark.yaml
  rehearsal/
    project.spark.yaml
```

Use `SPARK_PROJECT_ROOTS` for quick access in the file browser:

```bash
SPARK_PROJECT_ROOTS=Shows@/home/user/shows
```

Switch between shows by opening different project files in the Editor, or via CLI:

```bash
sparkctl stop
sparkctl reload ~/shows/corporate/project.spark.yaml
sparkctl start
```
