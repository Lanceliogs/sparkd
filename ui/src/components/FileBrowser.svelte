<script lang="ts">
  import { onMount } from 'svelte';
  import { editorBrowse, editorBrowseRoots, type BrowseResult, type BrowseRoots } from '../lib/api';
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
  let roots: BrowseRoots = $state({ places: [], drives: [] });

  /* Navigation history */
  let history: string[] = $state([]);
  let historyIdx = $state(-1);

  onMount(async () => {
    filename = initialFilename;
    roots = await editorBrowseRoots();
    await navigatePush(initialPath);
  });

  async function navigatePush(path: string) {
    browseResult = await editorBrowse(path);
    selectedFile = null;
    if (mode === 'open') filename = '';
    if (historyIdx < history.length - 1) {
      history = history.slice(0, historyIdx + 1);
    }
    history = [...history, browseResult.path];
    historyIdx = history.length - 1;
  }

  async function navigateTo(path: string) {
    browseResult = await editorBrowse(path);
    selectedFile = null;
    if (mode === 'open') filename = '';
  }

  function canGoBack(): boolean { return historyIdx > 0; }
  function canGoForward(): boolean { return historyIdx < history.length - 1; }

  async function goBack() {
    if (!canGoBack()) return;
    historyIdx--;
    await navigateTo(history[historyIdx]);
  }

  async function goForward() {
    if (!canGoForward()) return;
    historyIdx++;
    await navigateTo(history[historyIdx]);
  }

  function getParentPath(path: string): string | null {
    const normalized = normalizePath(path);
    if (normalized.match(/^[A-Za-z]:\/$/)) return null;
    if (normalized === '/') return null;
    const parts = normalized.split('/').filter(p => p !== '');
    if (parts.length <= 1) {
      if (parts[0]?.match(/^[A-Za-z]:$/)) return null;
      return '/';
    }
    parts.pop();
    if (parts[0].match(/^[A-Za-z]:$/)) {
      return parts.length === 1 ? parts[0] + '/' : parts[0] + '/' + parts.slice(1).join('/');
    }
    return (normalized.startsWith('/') ? '/' : '') + parts.join('/');
  }

  async function goUp() {
    if (!browseResult) return;
    const parent = getParentPath(browseResult.path);
    if (parent) await navigatePush(parent);
  }

  function normalizePath(p: string): string {
    return p.replace(/\\/g, '/');
  }

  function getBreadcrumbs(path: string): { label: string; path: string }[] {
    const normalized = normalizePath(path);
    const parts = normalized.split('/').filter(p => p !== '');
    const crumbs: { label: string; path: string }[] = [];
    const isUnixAbs = normalized.startsWith('/');
    const hasDrive = parts.length > 0 && !!parts[0].match(/^[A-Za-z]:$/);

    for (let i = 0; i < parts.length; i++) {
      const segment = parts[i];
      let fullPath: string;
      if (i === 0 && hasDrive) {
        fullPath = segment + '/';
      } else if (hasDrive) {
        fullPath = parts[0] + '/' + parts.slice(1, i + 1).join('/');
      } else if (isUnixAbs) {
        fullPath = '/' + parts.slice(0, i + 1).join('/');
      } else {
        fullPath = parts.slice(0, i + 1).join('/');
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
      navigatePush(joinPath(browseResult.path, entry.name));
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

    <div class="fb-body">
      <!-- Sidebar -->
      <aside class="fb-sidebar">
        {#if roots.places.length > 0}
          <div class="fb-sidebar-section">
            <span class="fb-sidebar-title">Places</span>
            {#each roots.places as place}
              <button class="fb-sidebar-item" onclick={() => navigatePush(place.path)}>
                <svg class="fb-sidebar-icon" viewBox="0 0 20 20" fill="currentColor"><path d={ICON_FOLDER} /></svg>
                {place.label}
              </button>
            {/each}
          </div>
        {/if}
        {#if roots.drives.length > 0}
          <div class="fb-sidebar-section">
            <span class="fb-sidebar-title">Drives</span>
            {#each roots.drives as drive}
              <button class="fb-sidebar-item" onclick={() => navigatePush(drive.path)}>
                <svg class="fb-sidebar-icon" viewBox="0 0 20 20" fill="currentColor"><path d={ICON_FOLDER} /></svg>
                {drive.label}
              </button>
            {/each}
          </div>
        {/if}
      </aside>

      <!-- Main content -->
      <div class="fb-main">
        {#if browseResult}
          <div class="fb-toolbar">
            <button class="fb-nav-btn" disabled={!canGoBack()} onclick={goBack} title="Back">
              <svg viewBox="0 0 20 20" fill="currentColor"><path d="M12.707 5.293a1 1 0 010 1.414L9.414 10l3.293 3.293a1 1 0 01-1.414 1.414l-4-4a1 1 0 010-1.414l4-4a1 1 0 011.414 0z"/></svg>
            </button>
            <button class="fb-nav-btn" disabled={!canGoForward()} onclick={goForward} title="Forward">
              <svg viewBox="0 0 20 20" fill="currentColor"><path d="M7.293 14.707a1 1 0 010-1.414L10.586 10 7.293 6.707a1 1 0 011.414-1.414l4 4a1 1 0 010 1.414l-4 4a1 1 0 01-1.414 0z"/></svg>
            </button>
            <button class="fb-nav-btn" disabled={!getParentPath(browseResult.path)} onclick={goUp} title="Up">
              <svg viewBox="0 0 20 20" fill="currentColor"><path d="M14.707 12.707a1 1 0 01-1.414 0L10 9.414l-3.293 3.293a1 1 0 01-1.414-1.414l4-4a1 1 0 011.414 0l4 4a1 1 0 010 1.414z"/></svg>
            </button>
            <nav class="fb-breadcrumbs">
              {#each getBreadcrumbs(browseResult.path) as crumb, i}
                {#if i > 0}
                  <svg class="fb-chevron" viewBox="0 0 20 20" fill="currentColor">
                    <path d={ICON_CHEVRON} />
                  </svg>
                {/if}
                <button class="fb-crumb" onclick={() => navigatePush(crumb.path)}>{crumb.label}</button>
              {/each}
            </nav>
          </div>

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
      </div>
    </div>

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
    width: 55vw;
    height: 55vh;
    min-width: 480px;
    min-height: 340px;
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
    padding: 0.6rem 1rem;
    border-bottom: 1px solid rgba(255, 255, 255, 0.06);
    background: var(--bg-card);
  }
  .fb-header h3 {
    margin: 0;
    font-size: 0.8rem;
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
    font-size: 0.65rem;
    padding: 0.25rem 0.6rem;
    cursor: pointer;
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .fb-cancel-btn:hover {
    color: var(--text);
    border-color: rgba(255, 255, 255, 0.15);
  }

  /* Body: sidebar + main */
  .fb-body {
    flex: 1;
    display: flex;
    overflow: hidden;
  }

  /* Sidebar */
  .fb-sidebar {
    width: 130px;
    flex-shrink: 0;
    background: var(--bg);
    border-right: 1px solid rgba(255, 255, 255, 0.04);
    overflow-y: auto;
    padding: 0.4rem 0;
  }
  .fb-sidebar-section {
    margin-bottom: 0.5rem;
  }
  .fb-sidebar-title {
    display: block;
    font-size: 0.55rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.06em;
    color: var(--text-muted);
    padding: 0.3rem 0.6rem 0.15rem;
  }
  .fb-sidebar-item {
    display: flex;
    align-items: center;
    gap: 0.35rem;
    width: 100%;
    text-align: left;
    padding: 0.25rem 0.6rem;
    font-size: 0.68rem;
    background: none;
    border: none;
    border-radius: 0;
    color: var(--text);
    cursor: pointer;
  }
  .fb-sidebar-item:hover {
    background: var(--bg-card);
    box-shadow: none;
  }
  .fb-sidebar-icon {
    width: 12px;
    height: 12px;
    color: var(--text-muted);
    flex-shrink: 0;
  }

  /* Main content */
  .fb-main {
    flex: 1;
    display: flex;
    flex-direction: column;
    overflow: hidden;
  }

  /* Toolbar: nav buttons + breadcrumbs */
  .fb-toolbar {
    display: flex;
    align-items: center;
    gap: 0.2rem;
    padding: 0.35rem 0.6rem;
    border-bottom: 1px solid rgba(255, 255, 255, 0.04);
    background: var(--bg);
  }
  .fb-nav-btn {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 22px;
    height: 22px;
    padding: 0;
    background: none;
    border: 1px solid rgba(255, 255, 255, 0.06);
    border-radius: 3px;
    color: var(--text-muted);
    cursor: pointer;
    flex-shrink: 0;
  }
  .fb-nav-btn:hover:not(:disabled) {
    background: var(--bg-card);
    color: var(--text);
    box-shadow: none;
  }
  .fb-nav-btn:disabled {
    opacity: 0.3;
    cursor: default;
  }
  .fb-nav-btn svg {
    width: 14px;
    height: 14px;
  }

  .fb-breadcrumbs {
    display: flex;
    align-items: center;
    gap: 0.1rem;
    overflow-x: auto;
    white-space: nowrap;
    margin-left: 0.3rem;
  }
  .fb-crumb {
    background: none;
    border: none;
    color: var(--accent);
    font-size: 0.68rem;
    font-family: monospace;
    padding: 0.1rem 0.3rem;
    border-radius: 3px;
    cursor: pointer;
  }
  .fb-crumb:hover {
    background: var(--bg-card);
    box-shadow: none;
  }
  .fb-chevron {
    width: 11px;
    height: 11px;
    color: var(--text-muted);
    flex-shrink: 0;
  }

  .fb-list {
    flex: 1;
    overflow-y: auto;
    padding: 0.3rem;
    display: flex;
    flex-direction: column;
    gap: 1px;
  }
  .fb-entry {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    text-align: left;
    padding: 0.35rem 0.7rem;
    font-size: 0.75rem;
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
    width: 14px;
    height: 14px;
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
    padding: 0.5rem 1rem;
    border-top: 1px solid rgba(255, 255, 255, 0.06);
    background: var(--bg-card);
  }
  .fb-filename-label {
    font-size: 0.65rem;
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
    padding: 0.3rem 0.5rem;
    font-size: 0.72rem;
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
    padding: 0.35rem 0.9rem;
    font-size: 0.68rem;
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
