<script lang="ts">
  export interface PadItem {
    id: string;
    label: string;
    sublabel?: string;
    active?: boolean;
  }

  interface Props {
    items: PadItem[];
    selected?: string;
    columns?: string;
    onselect?: (id: string) => void;
    onactivate?: (id: string, ev: PointerEvent) => void;
    onrelease?: (id: string, ev: PointerEvent) => void;
  }

  let { items, selected, columns, onselect, onactivate, onrelease }: Props = $props();

  function handleClick(id: string) {
    onselect?.(id);
  }

  function handlePointerDown(id: string, ev: PointerEvent) {
    onactivate?.(id, ev);
  }

  function handlePointerUp(id: string, ev: PointerEvent) {
    onrelease?.(id, ev);
  }
</script>

<div class="pad-grid" style:grid-template-columns={columns}>
  {#each items as item (item.id)}
    <button
      class="pad"
      class:active={item.active}
      class:selected={selected === item.id}
      onclick={() => handleClick(item.id)}
      onpointerdown={(ev) => handlePointerDown(item.id, ev)}
      onpointerup={(ev) => handlePointerUp(item.id, ev)}
    >
      <span class="pad-label">{item.label}</span>
      {#if item.sublabel}
        <span class="pad-sub">{item.sublabel}</span>
      {/if}
    </button>
  {/each}
</div>

<style>
  .pad-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(70px, 90px));
    gap: 0.4rem;
  }

  .pad {
    aspect-ratio: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 0.15rem;
    background: var(--bg-pad);
    border: 1.5px solid rgba(255, 255, 255, 0.06);
    border-radius: var(--pad-radius);
    color: var(--text);
    font-size: 0.65rem;
    font-weight: 600;
    padding: 0.4rem;
    user-select: none;
    touch-action: none;
    word-break: break-all;
    text-align: center;
    min-width: 0;
    box-shadow:
      inset 0 2px 4px rgba(0, 0, 0, 0.4),
      inset 0 -1px 2px rgba(255, 255, 255, 0.03);
    transition: box-shadow 0.12s, background 0.12s, transform 0.08s, border-color 0.12s;
  }

  .pad:hover {
    border-color: rgba(233, 69, 96, 0.4);
    box-shadow:
      inset 0 2px 4px rgba(0, 0, 0, 0.4),
      0 0 8px var(--accent-glow);
  }

  .pad:active {
    transform: translateY(1px);
    box-shadow: inset 0 3px 6px rgba(0, 0, 0, 0.6);
  }

  .pad.active,
  .pad.selected {
    background: var(--accent);
    color: white;
    border-color: var(--accent);
    box-shadow:
      0 0 14px var(--accent-glow),
      0 0 4px var(--accent-glow),
      inset 0 1px 2px rgba(255, 255, 255, 0.15);
  }

  .pad-label {
    font-size: 0.65rem;
    font-weight: 700;
    letter-spacing: 0.02em;
  }

  .pad-sub {
    font-size: 0.5rem;
    color: var(--text-muted);
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }

  .pad.active .pad-sub,
  .pad.selected .pad-sub {
    color: rgba(255, 255, 255, 0.7);
  }
</style>
