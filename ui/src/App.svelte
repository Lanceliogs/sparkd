<script lang="ts">
  import './app.css';
  import Live from './pages/Live.svelte';
  import Editor from './pages/Editor.svelte';
  import Toasts from './components/Toasts.svelte';
  import { initAuth, resolveRole, setToken, getToken, onAuthLostCallback, type Role } from './lib/auth';
  import ShareModal from './components/ShareModal.svelte';

  let currentTab: 'live' | 'editor' = $state('live');
  let authenticated = $state(false);
  let role: Role = $state(null);
  let tokenInput = $state('');
  let shareOpen = $state(false);
  let shareToken = $state('');

  async function resolveAuth() {
    const result = initAuth();
    if (result.token) {
      authenticated = true;
      role = result.role;
      if (!role) role = await resolveRole();
      if (!role) { authenticated = false; return; }
    } else {
      authenticated = false;
      role = null;
    }
  }

  onAuthLostCallback(() => {
    authenticated = false;
    role = null;
  });

  async function submitToken() {
    if (!tokenInput.trim()) return;
    setToken(tokenInput.trim());
    role = await resolveRole();
    if (role) {
      authenticated = true;
    } else {
      authenticated = false;
    }
  }

  resolveAuth();

  async function openShare() {
    try {
      const token = getToken();
      const res = await fetch('/api/auth/tokens', {
        headers: token ? { Authorization: `Bearer ${token}` } : {}
      });
      if (res.ok) {
        const data = await res.json();
        shareToken = data.share_token;
      }
    } catch { /* ignore */ }
    shareOpen = true;
  }
</script>

{#if !authenticated}
  <div class="auth-gate">
    <div class="auth-box">
      <h2>spark</h2>
      <p>Enter access code to continue</p>
      <form onsubmit={(e) => { e.preventDefault(); submitToken(); }}>
        <input
          type="text"
          bind:value={tokenInput}
          placeholder="Access code"
          class="auth-input"
          autofocus
        />
        <button type="submit" class="auth-submit">Connect</button>
      </form>
    </div>
  </div>
{:else}
  <nav class="tab-bar">
    <button class="tab" class:active={currentTab === 'live'} onclick={() => currentTab = 'live'}>
      Live
    </button>
    {#if role === 'admin'}
      <button class="tab" class:active={currentTab === 'editor'} onclick={() => currentTab = 'editor'}>
        Editor
      </button>
      <button class="tab share-btn" onclick={openShare}>
        Share
      </button>
    {/if}
  </nav>

  <main>
    {#if currentTab === 'live'}
      <Live />
    {:else if role === 'admin'}
      <Editor />
    {/if}
  </main>
{/if}

{#if role === 'admin'}
  <ShareModal bind:open={shareOpen} token={shareToken} />
{/if}

<Toasts />

<style>
  .auth-gate {
    display: flex;
    align-items: center;
    justify-content: center;
    min-height: 100vh;
  }

  .auth-box {
    text-align: center;
    padding: 2rem;
    background: var(--bg-surface);
    border-radius: 12px;
    box-shadow: 0 4px 24px rgba(0,0,0,0.5);
    min-width: 300px;
  }

  .auth-box h2 {
    margin: 0 0 0.5rem;
    color: var(--accent);
    font-size: 1.5rem;
    letter-spacing: 0.05em;
  }

  .auth-box p {
    margin: 0 0 1.5rem;
    color: var(--text-muted);
    font-size: 0.85rem;
  }

  .auth-box form {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .auth-input {
    padding: 0.6rem 0.8rem;
    border-radius: 6px;
    border: 1px solid rgba(255,255,255,0.1);
    background: var(--bg);
    color: var(--text);
    font-size: 0.9rem;
    text-align: center;
    outline: none;
  }

  .auth-input:focus {
    border-color: var(--accent);
    box-shadow: 0 0 0 2px var(--accent-glow);
  }

  .auth-submit {
    padding: 0.6rem;
    border-radius: 6px;
    border: none;
    background: var(--accent);
    color: #000;
    font-weight: 700;
    font-size: 0.85rem;
    cursor: pointer;
    transition: opacity 0.12s;
  }

  .auth-submit:hover {
    opacity: 0.85;
  }

  .tab-bar {
    display: flex;
    gap: 0;
    background: var(--bg-surface);
    border-bottom: 1px solid rgba(255,255,255,0.04);
    padding: 0 1rem;
    box-shadow: 0 1px 4px rgba(0,0,0,0.4);
  }

  .tab {
    background: none;
    border: none;
    border-radius: 0;
    padding: 0.75rem 1.8rem;
    font-size: 0.75rem;
    font-weight: 700;
    color: var(--text-muted);
    text-transform: uppercase;
    letter-spacing: 0.08em;
    border-bottom: 2px solid transparent;
    margin-bottom: -1px;
    transition: color 0.12s, border-color 0.12s, box-shadow 0.12s;
  }

  .tab:hover {
    color: var(--text);
    background: none;
    box-shadow: none;
  }

  .tab:active {
    transform: none;
  }

  .tab.active {
    color: var(--accent);
    border-bottom-color: var(--accent);
    box-shadow: 0 2px 8px var(--accent-glow);
  }

  .share-btn {
    margin-left: auto;
    color: var(--text-muted);
    border-bottom-color: transparent;
  }

  .share-btn:hover {
    color: var(--accent);
  }

  main {
    max-width: 1200px;
    margin: 0 auto;
    padding: 1rem;
  }
</style>
