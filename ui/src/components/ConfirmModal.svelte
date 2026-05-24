<script lang="ts">
  interface Props {
    message: string;
    confirmLabel?: string;
    onconfirm: () => void;
    oncancel: () => void;
  }

  let { message, confirmLabel = 'Confirm', onconfirm, oncancel }: Props = $props();

  function handleKeydown(ev: KeyboardEvent) {
    if (ev.key === 'Escape') oncancel();
  }
</script>

<svelte:window onkeydown={handleKeydown} />

<div class="modal-overlay" onclick={oncancel} role="presentation">
  <div class="confirm-modal" onclick={(e) => e.stopPropagation()} onkeydown={() => {}} role="alertdialog" aria-modal="true" tabindex="-1">
    <p class="confirm-msg">{message}</p>
    <div class="confirm-actions">
      <button class="btn-sm btn-danger" onclick={onconfirm}>{confirmLabel}</button>
      <button class="btn-sm btn-muted" onclick={oncancel}>Cancel</button>
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

  .confirm-modal {
    background: var(--bg-surface);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 8px;
    padding: 1.2rem 1.5rem;
    min-width: 280px;
    max-width: 400px;
    box-shadow: 0 8px 32px rgba(0, 0, 0, 0.6);
  }

  .confirm-msg {
    margin: 0 0 1rem 0;
    font-size: 0.8rem;
    color: var(--text);
  }

  .confirm-actions {
    display: flex;
    gap: 0.5rem;
    justify-content: flex-end;
  }

  .btn-sm.btn-danger {
    background: var(--red);
    color: white;
    border-color: rgba(233, 69, 96, 0.3);
  }
  
  .btn-sm.btn-danger:hover {
    box-shadow: 0 0 8px var(--accent-glow);
  }

  .btn-sm.btn-muted {
    background: var(--bg-pad);
    color: var(--text-muted);
    border-color: rgba(255,255,255,0.08);
  }

  .btn-sm.btn-muted:hover {
    box-shadow: 0 0 6px rgba(255,255,255,0.05);
  }
</style>
