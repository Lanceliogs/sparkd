<script lang="ts">
  import './app.css';
  import Live from './pages/Live.svelte';
  import Editor from './pages/Editor.svelte';

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

<style>
  .tab-bar {
    display: flex;
    gap: 0;
    background: var(--bg-surface);
    border-bottom: 2px solid var(--bg-card);
    padding: 0 1rem;
  }

  .tab {
    background: none;
    border: none;
    border-radius: 0;
    padding: 0.7rem 1.5rem;
    font-size: 0.85rem;
    font-weight: 600;
    color: var(--text-muted);
    border-bottom: 2px solid transparent;
    margin-bottom: -2px;
    transition: color 0.15s, border-color 0.15s;
  }

  .tab:hover {
    color: var(--text);
  }

  .tab:active {
    transform: none;
  }

  .tab.active {
    color: var(--accent);
    border-bottom-color: var(--accent);
  }

  main {
    max-width: 1200px;
    margin: 0 auto;
    padding: 1rem;
  }
</style>
