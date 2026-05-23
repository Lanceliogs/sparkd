<script lang="ts">
  import type { MidiStatus, DmxStatus } from '../lib/api';

  interface Props {
    midiStatus: MidiStatus;
    dmxStatus: DmxStatus;
  }

  let { midiStatus, dmxStatus }: Props = $props();
</script>

<footer class="status-footer">
  <div class="status-group">
    <span class="status-label">MIDI</span>
    {#if midiStatus.port_count === 0}
      <span class="status-value muted">no ports</span>
    {:else}
      {#each midiStatus.ports as port}
        <span class="status-value">
          <span class="dot-sm" class:green={port.connected} class:red={!port.connected}></span>
          {port.pattern}
        </span>
      {/each}
    {/if}
  </div>
  <div class="status-group">
    <span class="status-label">DMX</span>
    <span class="status-value">
      <span class="dot-sm" class:green={dmxStatus.state === 'connected'} class:yellow={dmxStatus.state === 'connecting'} class:red={dmxStatus.state === 'disconnected' || dmxStatus.state === 'error'}></span>
      {dmxStatus.backend}
    </span>
    <span class="status-value muted">{dmxStatus.stats.frames_sent} frames</span>
    {#if dmxStatus.stats.write_errors > 0}
      <span class="status-value error">{dmxStatus.stats.write_errors} errors</span>
    {/if}
  </div>
</footer>

<style>
  .status-footer {
    position: fixed;
    bottom: 0;
    left: 0;
    right: 0;
    display: flex;
    gap: 2rem;
    padding: 0.5rem 1rem;
    background: var(--bg-surface);
    border-top: 1px solid rgba(233, 69, 96, 0.15);
    font-size: 0.75rem;
    z-index: 50;
    box-shadow: 0 -2px 8px rgba(0, 0, 0, 0.3);
  }

  .status-group {
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }

  .status-label {
    font-weight: 700;
    text-transform: uppercase;
    color: var(--text-muted);
    font-size: 0.6rem;
    letter-spacing: 0.08em;
  }

  .status-value {
    display: flex;
    align-items: center;
    gap: 0.3rem;
    font-family: monospace;
    font-size: 0.7rem;
  }

  .status-value.muted { color: var(--text-muted); }
  .status-value.error { color: var(--red); }

  .dot-sm {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    display: inline-block;
  }

  .dot-sm.green { background: var(--green); box-shadow: 0 0 4px var(--green-glow); }
  .dot-sm.yellow { background: var(--yellow); box-shadow: 0 0 4px rgba(247, 211, 84, 0.4); }
  .dot-sm.red { background: var(--red); box-shadow: 0 0 4px var(--accent-glow); }
</style>
