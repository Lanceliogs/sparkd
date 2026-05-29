# sparkd REAPER Integration

Control sparkd from inside REAPER using a dockable ImGui panel.

## Prerequisites

1. **REAPER** (v6.0+)
2. **ReaImGui extension** — install via ReaPack:
   - Extensions > ReaPack > Browse packages
   - Search "ReaImGui", right-click > Install
   - Restart REAPER
3. **sparkd tools on PATH** — `sparkctl` and `spark-reaper` must be accessible.
   Alternatively, configure their full paths in the script's Settings panel.

## Installation

1. Copy `scripts/sparkd_control.lua` into your REAPER Scripts folder:
   - Windows: `%APPDATA%\REAPER\Scripts\`
   - Linux: `~/.config/REAPER/Scripts/`
   - macOS: `~/Library/Application Support/REAPER/Scripts/`

2. In REAPER, go to **Actions > Show action list**

3. Click **New action... > Load ReaScript...**

4. Select `sparkd_control.lua`

5. (Optional) Assign a keyboard shortcut or toolbar button to the action

## Usage

Run the action to open the sparkd Control panel. It can be docked into any
REAPER docker by dragging the title bar.

### Features

- **Project selector** — pick which `.spark.yaml` file to use
- **Engine control** — Start / Stop / Reload the sparkd engine
- **Blackout toggle** — quickly kill all lights
- **Generate Note Names** — regenerate per-channel note-name files for REAPER's MIDI editor
- **Settings** — configure daemon address and tool paths

### Loading Note Names in REAPER

After generating note-name files:

1. Open the MIDI editor on a track
2. Right-click the piano roll > **Load note names from file...**
3. Select the generated `spark-note-names-ch<N>.txt`

Note names will appear on the piano roll, making it easy to see which notes
trigger which scenes.

## File Structure

```
reaper/
  scripts/
    sparkd_control.lua     — the ReaImGui control panel script
  generated/               — default output for note-name files
    spark-note-names-ch1.txt
  README.md                — this file
```
