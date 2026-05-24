<script lang="ts">
  import { onMount } from 'svelte';
  import { removeToast, subscribeToasts, type Toast } from '../lib/toast';

  let toasts: Toast[] = $state([]);

  onMount(() => subscribeToasts((t) => { toasts = t; }));

  function handleDetail(toast: Toast) {
    removeToast(toast.id);
    toast.ondetail?.();
  }
</script>

{#if toasts.length > 0}
  <div class="toast-container">
    {#each toasts as toast (toast.id)}
      <button class="toast toast-{toast.type}" onclick={() => removeToast(toast.id)}>
        <span class="toast-msg">{toast.message}</span>
        {#if toast.ondetail}
          <span class="toast-detail" role="button" tabindex="0" onclick={(e) => { e.stopPropagation(); handleDetail(toast); }} onkeydown={(e) => { if (e.key === 'Enter') { e.stopPropagation(); handleDetail(toast); } }}>Details</span>
        {/if}
        <span class="toast-close">&times;</span>
      </button>
    {/each}
  </div>
{/if}

<style>
  .toast-container {
    position: fixed;
    bottom: 1rem;
    right: 1rem;
    z-index: 9999;
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    max-width: 360px;
  }

  .toast {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.6rem 0.8rem;
    border-radius: 6px;
    font-size: 0.75rem;
    font-weight: 500;
    box-shadow: 0 4px 12px rgba(0, 0, 0, 0.5);
    cursor: pointer;
    animation: slide-in 0.2s ease-out;
  }

  @keyframes slide-in {
    from { transform: translateX(100%); opacity: 0; }
    to { transform: translateX(0); opacity: 1; }
  }

  .toast-success {
    background: rgba(78, 205, 196, 0.15);
    border: 1px solid rgba(78, 205, 196, 0.4);
    color: #4ecdc4;
  }

  .toast-error {
    background: rgba(233, 69, 96, 0.15);
    border: 1px solid rgba(233, 69, 96, 0.4);
    color: #e94560;
  }

  .toast-warning {
    background: rgba(255, 180, 0, 0.15);
    border: 1px solid rgba(255, 180, 0, 0.4);
    color: rgb(255, 180, 0);
  }

  .toast-msg { flex: 1; }

  .toast-detail {
    font-size: 0.65rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.03em;
    opacity: 0.8;
    text-decoration: underline;
    cursor: pointer;
  }
  .toast-detail:hover { opacity: 1; }

  .toast-close {
    background: none;
    border: none;
    color: inherit;
    font-size: 1.1rem;
    cursor: pointer;
    opacity: 0.6;
    padding: 0 0.2rem;
  }
  .toast-close:hover { opacity: 1; }
</style>
