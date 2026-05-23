<script lang="ts">
  import { onMount } from 'svelte';
  import { editorBrowse, type BrowseResult } from '../lib/api';
  import { ICON_FOLDER, ICON_FILE, ICON_CHEVRON } from '../lib/icons';

  interface Props {
    mode: 'open' | 'save';
    initialPath?: string;
    initialFilename?: string;
    onconfirm: (fullPath: string) => void;
    oncancel: () => void;
  }

  let { mode, initialPath = '', initialFilename = '', onconfirm, oncancel }: Props = $props();

  let browseResult: BrowseResult | null = $state(null);
  let selectedFile: string | null = $state(null);
  let filename = $state('');

  onMount(() => {
    filename = initialFilename;
    navigate(initialPath);
  });

  async function navigate(path: string) {
    browseResult = await editorBrowse(path);
    selectedFile = null;
    if (mode === 'open') filename = '';
  }

  function normalizePath(p: string): string {
    return p.replace(/\\/g, '/');
  }

  function getBreadcrumbs(path: string): { label: string; path: string }[] {
    const normalized = normalizePath(path);
    const parts = normalized.split('/').filter(p => p !== '');
    const crumbs: { label: string; path: string }[] = [];

    for (let i = 0; i < parts.length; i++) {
      const segment = parts[i];
      let fullPath: string;
      if (i === 0 && segment.match(/^[A-Za-z]:$/)) {
        fullPath = segment + '/';
      } else {
        fullPath = parts.slice(0, i + 1).join('/');
        if (parts[0].match(/^[A-Za-z]:$/)) {
          fullPath = parts[0] + '/' + parts.slice(1, i + 1).join('/');
        }
      }
      crumbs.push({ label: segment, path: fullPath });
    }
    return crumbs;
  }

  function joinPath(base: string, name: string): string {
    const normalized = normalizePath(base);
    const sep = normalized.endsWith('/') ? '' : '/';
    return normalized + sep + name;
  }

  function handleEntryClick(entry: { name: string; type: 'file' | 'dir' }) {
    if (entry.type === 'dir') {
      if (!browseResult) return;
      navigate(joinPath(browseResult.path, entry.name));
    } else {
      selectedFile = entry.name;
      filename = entry.name;
    }
  }

  function handleEntryDblClick(entry: { name: string; type: 'file' | 'dir' }) {
    if (entry.type === 'file' && mode === 'open') {
      confirm();
    }
  }

  function confirm() {
    if (!browseResult || !filename) return;
    let finalName = filename;
    if (mode === 'save' && !finalName.endsWith('.yaml') && !finalName.endsWith('.yml')) {
      finalName += '.yaml';
    }
    const fullPath = joinPath(browseResult.path, finalName);
    onconfirm(fullPath);
  }

  function handleKeydown(e: KeyboardEvent) {
    if (e.key === 'Escape') oncancel();
    if (e.key === 'Enter' && filename) confirm();
  }

</script>

<div class="fb-overlay" onkeydown={handleKeydown} role="dialog" aria-modal="true" tabindex="-1">
  <div class="fb-backdrop"></div>
  <div class="fb-modal">
    <header class="fb-header">
      <h3>{mode === 'open' ? 'Open Project' : 'Save Project As'}</h3>
      <button class="fb-cancel-btn" onclick={oncancel}>Cancel</button>
    </header>

    {#if browseResult}
      <nav class="fb-breadcrumbs">
        {#each getBreadcrumbs(browseResult.path) as crumb, i}
          {#if i > 0}
            <svg class="fb-chevron" viewBox="0 0 20 20" fill="currentColor">
              <path d={ICON_CHEVRON} />
            </svg>
          {/if}
          <button class="fb-crumb" onclick={() => navigate(crumb.path)}>{crumb.label}</button>
        {/each}
      </nav>

      <div class="fb-list">
        {#each browseResult.entries.filter(e => e.type === 'dir').sort((a, b) => a.name.localeCompare(b.name)) as entry (entry.name)}
          <button
            class="fb-entry"
            onclick={() => handleEntryClick(entry)}
            ondblclick={() => handleEntryDblClick(entry)}
          >
            <svg class="fb-icon fb-icon-folder" viewBox="0 0 20 20" fill="currentColor">
              <path d={ICON_FOLDER} />
            </svg>
            <span>{entry.name}</span>
          </button>
        {/each}
        {#each browseResult.entries.filter(e => e.type === 'file').sort((a, b) => a.name.localeCompare(b.name)) as entry (entry.name)}
          <button
            class="fb-entry"
            class:selected={selectedFile === entry.name}
            onclick={() => handleEntryClick(entry)}
            ondblclick={() => handleEntryDblClick(entry)}
          >
            <svg class="fb-icon fb-icon-file" viewBox="0 0 20 20" fill="currentColor">
              <path d={ICON_FILE} />
            </svg>
            <span>{entry.name}</span>
          </button>
        {/each}
        {#if browseResult.entries.length === 0}
          <span class="fb-empty">Empty directory</span>
        {/if}
      </div>
    {:else}
      <div class="fb-list">
        <span class="fb-empty">Loading...</span>
      </div>
    {/if}

    <footer class="fb-footer">
      <label class="fb-filename-label" for="fb-filename-input">Filename:</label>
      {#if mode === 'open'}
        <input id="fb-filename-input" class="fb-filename" type="text" readonly value={filename} />
      {:else}
        <input id="fb-filename-input" class="fb-filename" type="text" bind:value={filename} placeholder="project-name.yaml" />
      {/if}
      <button class="fb-confirm" disabled={!filename} onclick={confirm}>
        {mode === 'open' ? 'Open' : 'Save'}
      </button>
    </footer>
  </div>
</div>

<style>
  .fb-overlay {
    position: fixed;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 200;
  }
  .fb-backdrop {
    position: absolute;
    inset: 0;
    background: rgba(0, 0, 0, 0.75);
    backdrop-filter: blur(3px);
  }
  .fb-modal {
    position: relative;
    width: 50vw;
    height: 50vh;
    min-width: 400px;
    min-height: 300px;
    background: var(--bg-surface);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: var(--radius);
    display: flex;
    flex-direction: column;
    overflow: hidden;
    box-shadow: 0 12px 48px rgba(0, 0, 0, 0.7), 0 0 1px rgba(233, 69, 96, 0.3);
  }

  .fb-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0.75rem 1rem;
    border-bottom: 1px solid rgba(255, 255, 255, 0.06);
    background: var(--bg-card);
  }
  .fb-header h3 {
    margin: 0;
    font-size: 0.85rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--text);
  }
  .fb-cancel-btn {
    background: var(--bg-pad);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 4px;
    color: var(--text-muted);
    font-size: 0.7rem;
    padding: 0.3rem 0.7rem;
    cursor: pointer;
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .fb-cancel-btn:hover {
    color: var(--text);
    border-color: rgba(255, 255, 255, 0.15);
  }

  .fb-breadcrumbs {
    display: flex;
    align-items: center;
    gap: 0.1rem;
    padding: 0.5rem 1rem;
    border-bottom: 1px solid rgba(255, 255, 255, 0.04);
    background: var(--bg);
    overflow-x: auto;
    white-space: nowrap;
  }
  .fb-crumb {
    background: none;
    border: none;
    color: var(--accent);
    font-size: 0.72rem;
    font-family: monospace;
    padding: 0.15rem 0.35rem;
    border-radius: 3px;
    cursor: pointer;
  }
  .fb-crumb:hover {
    background: var(--bg-card);
    box-shadow: none;
  }
  .fb-chevron {
    width: 12px;
    height: 12px;
    color: var(--text-muted);
    flex-shrink: 0;
  }

  .fb-list {
    flex: 1;
    overflow-y: auto;
    padding: 0.4rem;
    display: flex;
    flex-direction: column;
    gap: 1px;
  }
  .fb-entry {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    text-align: left;
    padding: 0.4rem 0.8rem;
    font-size: 0.78rem;
    border-radius: 4px;
    background: none;
    color: var(--text);
    border: 1px solid transparent;
    cursor: pointer;
  }
  .fb-entry:hover {
    background: var(--bg-card);
    box-shadow: none;
  }
  .fb-entry.selected {
    background: rgba(233, 69, 96, 0.1);
    border-color: rgba(233, 69, 96, 0.3);
    box-shadow: 0 0 6px rgba(233, 69, 96, 0.15);
  }
  .fb-icon {
    width: 16px;
    height: 16px;
    flex-shrink: 0;
  }
  .fb-icon-folder { color: var(--accent); }
  .fb-icon-file { color: var(--text-muted); }
  .fb-empty {
    color: var(--text-muted);
    font-size: 0.75rem;
    padding: 2rem;
    text-align: center;
  }

  .fb-footer {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    padding: 0.6rem 1rem;
    border-top: 1px solid rgba(255, 255, 255, 0.06);
    background: var(--bg-card);
  }
  .fb-filename-label {
    font-size: 0.7rem;
    color: var(--text-muted);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    white-space: nowrap;
  }
  .fb-filename {
    flex: 1;
    background: var(--bg);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.35rem 0.6rem;
    font-size: 0.75rem;
    font-family: monospace;
    box-shadow: inset 0 1px 3px rgba(0, 0, 0, 0.4);
  }
  .fb-filename:focus {
    outline: none;
    border-color: rgba(233, 69, 96, 0.4);
    box-shadow: inset 0 1px 3px rgba(0, 0, 0, 0.4), 0 0 4px var(--accent-glow);
  }
  .fb-filename[readonly] {
    opacity: 0.7;
    cursor: default;
  }
  .fb-confirm {
    background: var(--accent);
    color: white;
    border: none;
    border-radius: 4px;
    padding: 0.4rem 1rem;
    font-size: 0.72rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    cursor: pointer;
  }
  .fb-confirm:hover {
    background: var(--accent-hover);
    box-shadow: 0 0 8px var(--accent-glow);
  }
  .fb-confirm:disabled {
    opacity: 0.4;
    cursor: not-allowed;
    box-shadow: none;
  }
</style>
