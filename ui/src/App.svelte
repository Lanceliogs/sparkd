<script lang="ts">
  import './app.css';
  import Live from './pages/Live.svelte';
  import Editor from './pages/Editor.svelte';
  import Toasts from './components/Toasts.svelte';

  let currentTab: 'live' | 'editor' = $state('live');
</script>

<nav class="tab-bar">
  <button class="tab" class:active={currentTab === 'live'} onclick={() => currentTab = 'live'}>
    Live
  </button>
  <button class="tab" class:active={currentTab === 'editor'} onclick={() => currentTab = 'editor'}>
    Editor
  </button>
</nav>

<main>
  {#if currentTab === 'live'}
    <Live />
  {:else}
    <Editor />
  {/if}
</main>

<Toasts />

<style>
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

  main {
    max-width: 1200px;
    margin: 0 auto;
    padding: 1rem;
  }
</style>
