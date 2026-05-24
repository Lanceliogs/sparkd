<script lang="ts">
  import type { Snippet } from 'svelte';

  interface Props {
    type?: 'info' | 'warning' | 'error';
    title: string;
    onclose: () => void;
    children: Snippet;
  }

  let { type = 'info', title, onclose, children }: Props = $props();

  function handleKeydown(ev: KeyboardEvent) {
    if (ev.key === 'Escape') onclose();
  }
</script>

<svelte:window onkeydown={handleKeydown} />

<div class="modal-overlay" onclick={onclose} role="presentation">
  <div class="modal modal-{type}" onclick={(e) => e.stopPropagation()} onkeydown={() => {}} role="dialog" aria-modal="true" tabindex="-1">
    <header class="modal-header">
      <h3 class="modal-title">{title}</h3>
      <button class="modal-close" onclick={onclose}>&times;</button>
    </header>
    <div class="modal-body">
      {@render children()}
    </div>
  </div>
</div>

<style>
  .modal-overlay {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.6);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 9000;
    animation: fade-in 0.15s ease-out;
  }

  @keyframes fade-in {
    from { opacity: 0; }
    to { opacity: 1; }
  }

  .modal {
    background: var(--bg-surface);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 8px;
    min-width: 320px;
    max-width: 560px;
    max-height: 80vh;
    display: flex;
    flex-direction: column;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.6);
  }

  .modal-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0.75rem 1rem;
    border-bottom: 1px solid rgba(255, 255, 255, 0.06);
  }

  .modal-title {
    margin: 0;
    font-size: 0.85rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }

  .modal-info .modal-title { color: var(--green); }
  .modal-warning .modal-title { color: rgb(255, 180, 0); }
  .modal-error .modal-title { color: var(--red); }

  .modal-close {
    background: none;
    border: none;
    color: var(--text-muted);
    font-size: 1.2rem;
    cursor: pointer;
    padding: 0 0.3rem;
  }
  .modal-close:hover { color: var(--text); }

  .modal-body {
    padding: 1rem;
    overflow-y: auto;
    font-size: 0.75rem;
    color: var(--text);
    line-height: 1.5;
  }
</style>
