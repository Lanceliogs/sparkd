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
  <div class="form-grid">
    <label for="hw-midi-dev">MIDI Device</label>
    <input id="hw-midi-dev" type="text" bind:value={config.midi_device} oninput={markDirty} />
    <label for="hw-midi-mode">MIDI Mode</label>
    <select id="hw-midi-mode" bind:value={config.midi_mode} onchange={markDirty}>
      <option value="">none</option>
      <option value="open-existing">open-existing</option>
      <option value="create-virtual">create-virtual</option>
    </select>
    <label for="hw-dmx-dev">DMX Device</label>
    <input id="hw-dmx-dev" type="text" bind:value={config.dmx_device} oninput={markDirty} />
    <label for="hw-dmx-backend">DMX Backend</label>
    <select id="hw-dmx-backend" bind:value={config.dmx_backend} onchange={markDirty}>
      <option value="">none</option>
      <option value="open">open</option>
      <option value="pro">pro</option>
      <option value="dummy">dummy</option>
    </select>
    <label for="hw-refresh">Refresh Hz</label>
    <input id="hw-refresh" type="number" min="1" max="44" bind:value={config.dmx_refresh_hz} oninput={markDirty} />
  </div>
  {#if dirty}
    <button class="btn-sm btn-save" onclick={handleSave}>Save Hardware</button>
  {/if}
</div>

<style>
  .hw-form { padding: 0.75rem; }
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
