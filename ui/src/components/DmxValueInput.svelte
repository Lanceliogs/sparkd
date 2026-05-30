<script lang="ts">
  interface Props {
    value: number;
    onchange?: () => void;
  }

  let {value = $bindable(), onchange }: Props = $props();

  function set(v: number) {
    value = v;
    onchange?.();
  }
</script>

<!-- The idea: [0][━━━━━━━O━━━━━━━][ 128 ][255] -->
<button class="dmx-btn" onclick={() => set(0)}>0</button>
<input type="range" min="0" max="255" bind:value={value} oninput={onchange} class="dmx-slider" />
<input type="number" min="0" max="255" bind:value={value} oninput={onchange} class="dmx-input" />
<button class="dmx-btn" onclick={() => set(255)}>255</button>

<style>
  .dmx-btn {
    font-size: 0.65rem;
    padding: 0.15rem 0.35rem;
    border-radius: 3px;
    background: var(--bg-pad);
    border: 1px solid rgba(255, 255, 255, 0.1);
    color: var(--fg);
    cursor: pointer;
    min-width: 1.8rem;
    text-align: center;
  }
  .dmx-btn:hover {
    background: var(--bg-card);
    border-color: var(--accent);
  }
  .dmx-slider {
    -webkit-appearance: none;
    appearance: none;
    flex: 1;
    height: 6px;
    background: var(--bg);
    border-radius: 3px;
    cursor: pointer;
    margin: 0;
    padding: 0;
  }
  /* Track - WebKit (Chrome, Edge, Safari) */
  .dmx-slider::-webkit-slider-runnable-track {
    height: 6px;
    background: var(--bg);
    border-radius: 3px;
  }
  /* Track - Firefox */
  .dmx-slider::-moz-range-track {
    height: 6px;
    background: var(--bg);
    border-radius: 3px;
    border: none;
  }
  /* Thumb - WebKit */
  .dmx-slider::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 14px;
    height: 14px;
    background: var(--accent);
    border-radius: 50%;
    margin-top: -4px;
    border: none;
  }
  /* Thumb - Firefox */
  .dmx-slider::-moz-range-thumb {
    width: 14px;
    height: 14px;
    background: var(--accent);
    border-radius: 50%;
    border: none;
  }
  .dmx-input {
    width: 3.2rem;
    background: var(--bg);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 3px;
    color: var(--fg);
    text-align: center;
    font-size: 0.75rem;
    padding: 0.15rem;
  }
</style>