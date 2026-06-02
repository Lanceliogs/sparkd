<script lang="ts">
  import QRCode from 'qrcode';
  import { onMount } from 'svelte';

  let { open = $bindable(), token }: {
    open: boolean;
    token: string;
  } = $props();

  let qrDataUrl: string = $state('');
  let shareUrl: string = $state('');
  let lanInfo: { lan_ip: string; port: number } | null = $state(null);
  let copied = $state(false);

  onMount(async () => {
    try {
      const res = await fetch('/api/auth/info');
      if (res.ok) lanInfo = await res.json();
    } catch { /* ignore */ }
  });

  $effect(() => {
    if (!lanInfo || !open || !token) return;
    const url = `http://${lanInfo.lan_ip}:${lanInfo.port}/?token=${token}`;
    shareUrl = url;
    QRCode.toDataURL(url, { width: 256, margin: 2, color: { dark: '#ffffff', light: '#00000000' } })
      .then(dataUrl => { qrDataUrl = dataUrl; });
  });

  function copyUrl() {
    navigator.clipboard.writeText(shareUrl);
    copied = true;
    setTimeout(() => { copied = false; }, 2000);
  }

  function close() {
    open = false;
  }
</script>

{#if open}
  <div class="modal-backdrop" onclick={close} role="presentation">
    <div class="modal" onclick={(e) => e.stopPropagation()} onkeydown={(e) => e.key === 'Escape' && close()} role="dialog" tabindex="-1">
      <div class="modal-header">
        <h3>Share Live Access</h3>
        <button class="modal-close" onclick={close}>X</button>
      </div>

      <p class="desc">Scan to get live control (start, stop, blackout, pads).</p>

      {#if qrDataUrl}
        <div class="qr-container">
          <img src={qrDataUrl} alt="QR Code" class="qr-image" />
        </div>
      {/if}

      <div class="url-row">
        <input type="text" readonly value={shareUrl} class="url-input" />
        <button class="copy-btn" onclick={copyUrl}>{copied ? 'Copied' : 'Copy'}</button>
      </div>
    </div>
  </div>
{/if}

<style>
  .modal-backdrop {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.7);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 1000;
  }

  .modal {
    background: var(--bg-surface);
    border-radius: 12px;
    padding: 1.5rem;
    min-width: 340px;
    max-width: 400px;
    box-shadow: 0 8px 32px rgba(0,0,0,0.6);
  }

  .modal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin-bottom: 1rem;
  }

  .modal-header h3 {
    margin: 0;
    font-size: 1.1rem;
  }

  .modal-close {
    background: none;
    border: none;
    color: var(--text-muted);
    font-size: 1rem;
    cursor: pointer;
    padding: 0.25rem 0.5rem;
  }

  .desc {
    color: var(--text-muted);
    font-size: 0.85rem;
    margin: 0 0 1rem;
  }

  .qr-container {
    display: flex;
    justify-content: center;
    margin-bottom: 1rem;
  }

  .qr-image {
    width: 200px;
    height: 200px;
    border-radius: 8px;
  }

  .url-row {
    display: flex;
    gap: 0.5rem;
  }

  .url-input {
    flex: 1;
    padding: 0.5rem;
    border-radius: 6px;
    border: 1px solid rgba(255,255,255,0.1);
    background: var(--bg);
    color: var(--text);
    font-size: 0.75rem;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .copy-btn {
    padding: 0.5rem 0.75rem;
    border-radius: 6px;
    border: none;
    background: var(--accent);
    color: #000;
    font-weight: 700;
    font-size: 0.75rem;
    cursor: pointer;
    white-space: nowrap;
  }
</style>
