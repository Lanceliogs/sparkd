<script lang="ts">
  import type { HardwareConfig } from '../lib/api';

  interface Props {
    config: HardwareConfig;
    onsave: () => void;
    ondirty?: () => void;
  }

  let { config = $bindable(), onsave, ondirty }: Props = $props();

  let dirty = $state(false);

  function markDirty() { dirty = true; ondirty?.(); }

  function handleSave() {
    onsave();
    dirty = false;
  }
</script>

<div class="hw-form">
  <h3 class="section-title">MIDI</h3>
  <p class="help-text">
    Device name is pattern-based: sparkd matches any MIDI input port whose name contains this string (case-insensitive). Leave empty to disable MIDI input.
  </p>
  <div class="form-grid">
    <label for="hw-midi-dev">Device</label>
    <input id="hw-midi-dev" type="text" placeholder="e.g. loopMIDI, UM-ONE" bind:value={config.midi_device} oninput={markDirty} />
    <label for="hw-midi-mode">Mode</label>
    <select id="hw-midi-mode" bind:value={config.midi_mode} onchange={markDirty}>
      <option value="">none</option>
      <option value="open-existing">open-existing</option>
      <option value="create-virtual">create-virtual</option>
    </select>
  </div>
  <p class="help-text help-detail">
    <strong>open-existing</strong> — connect to a physical or virtual MIDI port already present on the system.<br/>
    <strong>create-virtual</strong> — create a new virtual MIDI port (macOS/Linux only, useful for routing from a DAW).
  </p>

  <h3 class="section-title">DMX</h3>
  <p class="help-text">
    Device is the serial port for your DMX interface. On Windows this is a COM port (e.g. COM3), on Linux a /dev/tty path. Leave empty to use the dummy backend (no output).
  </p>
  <div class="form-grid">
    <label for="hw-dmx-dev">Device</label>
    <input id="hw-dmx-dev" type="text" placeholder="e.g. COM3, /dev/ttyUSB0" bind:value={config.dmx_device} oninput={markDirty} />
    <label for="hw-dmx-backend">Backend</label>
    <select id="hw-dmx-backend" bind:value={config.dmx_backend} onchange={markDirty}>
      <option value="">none</option>
      <option value="open">open</option>
      <option value="pro">pro</option>
      <option value="dummy">dummy</option>
    </select>
    <label for="hw-refresh">Refresh Hz</label>
    <input id="hw-refresh" type="number" min="1" max="44" placeholder="25" bind:value={config.dmx_refresh_hz} oninput={markDirty} />
  </div>
  <p class="help-text help-detail">
    <strong>open</strong> — Open DMX protocol (break + raw frames). Used by many USB-DMX interfaces.<br/>
    <strong>pro</strong> — Enttec Pro protocol (packetized serial). Used by Enttec Pro and compatible interfaces.<br/>
    <strong>dummy</strong> — No DMX output. Useful for testing without hardware.<br/>
    Not sure which one? Try both — only the correct one will work with your interface.<br/>
    Refresh rate: 1–44 Hz. Default <strong>25 Hz</strong> is a good balance between responsiveness and bus stability.
  </p>

  {#if dirty}
    <button class="btn-sm btn-save" onclick={handleSave}>Save Hardware</button>
  {/if}
</div>

<style>
  .hw-form { padding: 0.75rem; }
  .section-title {
    font-size: 0.75rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text);
    margin-top: 0.8rem;
    margin-bottom: 0.3rem;
  }
  .section-title:first-child { margin-top: 0; }
  .help-text {
    font-size: 0.65rem;
    color: var(--text-muted);
    line-height: 1.5;
    margin-bottom: 0.4rem;
  }
  .help-detail {
    margin-top: 0.3rem;
    padding-left: 0.3rem;
    border-left: 2px solid rgba(255, 255, 255, 0.06);
  }
  .help-text strong {
    color: var(--text);
  }
  .form-grid {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 0.5rem 0.75rem;
    align-items: center;
  }
  .form-grid label {
    font-size: 0.7rem;
    color: var(--text-muted);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .form-grid input, .form-grid select {
    background: var(--bg);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.3rem 0.5rem;
    font-size: 0.75rem;
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.3);
  }
  .form-grid input:focus, .form-grid select:focus {
    outline: none;
    border-color: rgba(233, 69, 96, 0.4);
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.3), 0 0 4px var(--accent-glow);
  }
  .btn-sm {
    font-size: 0.7rem;
    padding: 0.3rem 0.7rem;
    border-radius: 4px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    cursor: pointer;
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .btn-save {
    background: var(--green);
    color: #111;
    border-color: rgba(78, 205, 196, 0.3);
    margin-top: 0.75rem;
  }
  .btn-save:hover { box-shadow: 0 0 6px var(--green-glow); }
</style>
