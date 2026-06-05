# Web UI Guide

The sparkd web interface runs at `http://127.0.0.1:7601` and has three main sections accessible via the top navigation: **LIVE**, **EDITOR**, and **SHARE**.

## Live View

The Live view is your performance interface. It shows the engine status and provides real-time control.

### Not Running

When no project is loaded or the engine is stopped:

![Live — not running](images/live_not_running.png)

### Project Loaded

After loading a project, the Live view shows scene pads and control buttons:

![Live — loaded](images/live_loaded.png)

### Engine Running

Click **Start** to activate the engine. The status indicator turns green and the bottom bar shows MIDI and DMX connection status:

![Live — running](images/live_running.png)

The status bar at the bottom shows:
- **MIDI** — device name and connection status (green dot = connected)
- **DMX** — device, backend type, and frame count

### Scene Pads

Each scene from your project appears as a clickable pad. Click to activate:

![Live — activate scene](images/live_activate_scene.png)

Pads show the scene name and type (STATIC or SEQUENCE). Active scenes are highlighted.

### Blackout

Click **Blackout** to instantly cut all DMX output to zero (emergency stop):

![Live — blackout](images/live_blackout.png)

Click again to restore output.

### Reload

Click **Reload** to reload the project file from disk. The engine must be stopped first. Useful after editing the YAML manually.

## Editor View

The Editor is where you build and modify your project. It has four sub-tabs:

### Banks

Manage fixture template banks. Banks provide reusable fixture definitions (channel layouts) that you can assign to fixtures.

![Editor — Banks](images/editor_banks.png)

Banks are loaded from directories configured in `SPARK_FIXTURE_BANK_PATH` or the default `~/.spark/fixtures/` path.

### Fixtures

Add and manage the physical lights in your project. Each fixture has an ID, a DMX start address, and a channel layout.

![Editor — Fixtures](images/editor_fixtures.png)

Click **+ ADD** to create a new fixture. Three methods are available:

**From template** — pick from a fixture bank:

![Fixture form — template](images/fixture_form_template.png)

**Manual** — define channels by hand:

![Fixture form — manual](images/fixture_form_manual.png)

**Copy from** — duplicate the layout of an existing fixture:

![Fixture form — copy from](images/fixture_form_copy_from.png)

### Hardware

Configure your MIDI input and DMX output devices:

![Editor — Hardware](images/editor_hardware.png)

**MIDI** settings:
- **Device** — name (or substring) of your MIDI controller
- **Mode** — `open-existing` (connect to a real device) or `create-virtual` (create a virtual port)

![MIDI modes](images/editor_hardware_midi_mode.png)

**DMX** settings:
- **Device** — serial port (e.g. `COM3` on Windows, `/dev/ttyUSB0` on Linux)
- **Backend** — protocol to use (`open`, `pro`, or `dummy`)
- **Refresh Hz** — DMX update rate (default: 25 Hz)

![DMX backends](images/editor_hardware_dmx_backend.png)

### Scenes

Create lighting scenes triggered by MIDI notes:

![Editor — Scenes](images/editor_scenes.png)

Click **+ ADD SCENE** to create a new scene.

**Static scene** — fixed DMX values applied while the scene is active:

![Scene form — static](images/scene_form_static.png)

**Sequence scene** — timed steps with transitions, optionally looping:

![Scene form — sequence](images/scene_form_sequence.png)

### Saving

The toolbar shows your project path and action buttons:

- **SAVE** — write changes to the current file
- **SAVE AS** — save to a new file
- **NEW** — create a blank project
- **CLOSE** — close the current project

An unsaved changes indicator appears when you have pending modifications:

![Unsaved changes](images/editor_project_unsaved.png)

### Error Handling

If a project file has syntax errors, sparkd shows a detailed error:

![Parse error](images/parse_error.png)

Runtime errors appear as toast notifications:

![Error toast](images/error_toast.png)

## File Browser

Click **NEW** or use the open dialog to browse your filesystem:

![File browser](images/file_browser.png)

The left sidebar shows:
- **Projects** — custom directories from `SPARK_PROJECT_ROOTS` (if configured)
- **Places** — common locations (Home, Documents, Desktop)
- **Drives** — mounted drives (Windows) or mount points (Linux)

Navigate using the breadcrumb path at the top, or click folders to drill down.

## Share

The Share tab generates a QR code for live access from other devices on the same network (e.g. your phone):

![Share — QR code](images/share_qrcode_live_access.png)

Scan the QR code or copy the URL. The linked device gets access to the Live view (start, stop, blackout, scene pads) with a token for authentication.

This requires sparkd to be listening on `0.0.0.0` instead of `127.0.0.1`. See [Configuration](configuration.md) for details.
