<script lang="ts">
  import { onMount } from 'svelte';
  import {
    editorStatus,
    editorOpen,
    editorClose,
    editorSave,
    editorGetFixtures,
    editorAddFixture,
    editorUpdateFixture,
    editorDeleteFixture,
    editorGetBanks,
    editorBankAddFixture,
    editorBankUpdateFixture,
    editorBankDeleteFixture,
    editorBankSave,
    editorGetBankDirs,
    editorCreateBank,
    editorBrowse,
    getScenes,
    type EditorStatus,
    type EditorFixture,
    type EditorBank,
    type BankFixture,
    type BrowseResult,
    type SceneDef,
    type Channel,
  } from '../lib/api';

  /* ---- Data state ---- */
  let status: EditorStatus = $state({ project_loaded: false, project_path: '', dirty: false, fixture_count: 0, bank_count: 0 });
  let fixtures: EditorFixture[] = $state([]);
  let banks: EditorBank[] = $state([]);
  let scenes: SceneDef[] = $state([]);
  let bankDirs: string[] = $state([]);

  /* ---- Selection / navigation ---- */
  type SelectionKind = 'bank_fixture' | 'project_fixture' | 'scene';
  type Selection = { kind: SelectionKind; bankIdx?: number; itemIdx: number } | null;

  let selectionTab: 'banks' | 'fixtures' | 'hardware' | 'scenes' = $state('banks');
  let selection: Selection = $state(null);
  let collapsedBanks: Set<number> = $state(new Set());

  /* ---- Edit card state ---- */
  let editDirty = $state(false);
  let showDiscardModal = $state(false);
  let pendingSelection: Selection = $state(null);

  let editFixture: EditorFixture | null = $state(null);
  let editBankFixture: BankFixture | null = $state(null);
  let editIsNew = $state(false);

  /* ---- New bank form ---- */
  let newBankId = $state('');
  let newBankDir = $state('');

  /* ---- File browser ---- */
  let showBrowser = $state(false);
  let browseResult: BrowseResult | null = $state(null);
  let openPath = $state('');

  /* ---- Data loading ---- */
  async function refresh() {
    status = await editorStatus();
    if (status.project_loaded) {
      fixtures = await editorGetFixtures();
      try { scenes = await getScenes(); } catch { scenes = []; }
    } else {
      fixtures = [];
      scenes = [];
    }
    banks = await editorGetBanks();
    bankDirs = await editorGetBankDirs();
    if (bankDirs.length > 0 && !newBankDir) newBankDir = bankDirs[0];
  }

  onMount(() => { refresh(); });

  /* ---- Selection logic ---- */
  function trySelect(sel: Selection) {
    if (editDirty) {
      pendingSelection = sel;
      showDiscardModal = true;
      return;
    }
    applySelection(sel);
  }

  function applySelection(sel: Selection) {
    selection = sel;
    editDirty = false;
    showDiscardModal = false;
    pendingSelection = null;

    if (!sel) {
      editFixture = null;
      editBankFixture = null;
      return;
    }

    if (sel.kind === 'project_fixture') {
      const f = fixtures.find(fx => fx.index === sel.itemIdx);
      if (f) {
        editFixture = { ...f, channels: f.channels.map(c => ({ ...c })) };
        editBankFixture = null;
        editIsNew = false;
      }
    } else if (sel.kind === 'bank_fixture') {
      const bank = banks.find(b => b.index === sel.bankIdx);
      const f = bank?.fixtures.find(fx => fx.index === sel.itemIdx);
      if (f) {
        editBankFixture = { ...f, channels: f.channels.map(c => ({ ...c })) };
        editFixture = null;
        editIsNew = false;
      }
    } else if (sel.kind === 'scene') {
      editFixture = null;
      editBankFixture = null;
    }
  }

  function confirmDiscard() {
    editDirty = false;
    showDiscardModal = false;
    if (pendingSelection !== undefined) applySelection(pendingSelection);
  }

  function cancelDiscard() {
    showDiscardModal = false;
    pendingSelection = null;
  }

  function closeEdit() {
    trySelect(null);
  }

  function markDirty() {
    editDirty = true;
  }

  /* ---- Project lifecycle ---- */
  async function handleOpen() {
    if (!openPath) return;
    await editorOpen(openPath);
    showBrowser = false;
    await refresh();
  }

  async function handleClose() {
    await editorClose();
    selection = null; editFixture = null; editBankFixture = null; editDirty = false;
    await refresh();
  }

  async function handleSave() {
    await editorSave();
    await refresh();
  }

  /* ---- File browser ---- */
  async function openBrowser() {
    showBrowser = true;
    browseResult = await editorBrowse('');
  }

  async function browseTo(path: string) {
    browseResult = await editorBrowse(path);
  }

  function joinPath(base: string, name: string): string {
    const sep = base.endsWith('/') || base.endsWith('\\') ? '' : '/';
    return base + sep + name;
  }

  function browseSelect(name: string) {
    if (!browseResult) return;
    const full = joinPath(browseResult.path, name);
    openPath = full;
    showBrowser = false;
    editorOpen(full).then(() => refresh());
  }

  function browseNav(name: string) {
    if (!browseResult) return;
    browseTo(joinPath(browseResult.path, name));
  }

  function browseUp() {
    if (!browseResult) return;
    const normalized = browseResult.path.replace(/\\/g, '/');
    const parts = normalized.split('/').filter(p => p !== '');
    if (parts.length <= 1) return; /* Already at root (e.g. "E:") */
    parts.pop();
    const parent = parts.join('/');
    /* Preserve drive letter on Windows (e.g. E:/Projects -> E:/) */
    const result = parent.match(/^[A-Za-z]:$/) ? parent + '/' : parent;
    browseTo(result);
  }

  /* ---- Bank actions ---- */
  function toggleBank(idx: number) {
    if (collapsedBanks.has(idx)) {
      collapsedBanks = new Set([...collapsedBanks].filter(i => i !== idx));
    } else {
      collapsedBanks = new Set([...collapsedBanks, idx]);
    }
  }

  async function handleCreateBank() {
    if (!newBankId || !newBankDir) return;
    await editorCreateBank(newBankId, newBankDir);
    newBankId = '';
    await refresh();
  }

  async function handleBankSave(bankIdx: number) {
    await editorBankSave(bankIdx);
    await refresh();
  }

  /* ---- New fixture (project or bank) ---- */
  function startAddProjectFixture() {
    editFixture = { id: '', name: '', start_address: 1, channel_count: 0, template: '', copy_from: '', channels: [] };
    editBankFixture = null;
    editIsNew = true;
    editDirty = false;
    selection = { kind: 'project_fixture', itemIdx: -1 };
  }

  function startAddBankFixture(bankIdx: number) {
    editBankFixture = { id: '', name: '', channel_count: 0, channels: [] };
    editFixture = null;
    editIsNew = true;
    editDirty = false;
    selection = { kind: 'bank_fixture', bankIdx, itemIdx: -1 };
  }

  /* ---- Save edit card ---- */
  async function saveEditFixture() {
    if (!editFixture) return;
    const data = { ...editFixture };
    delete (data as any).index;
    if (editIsNew) {
      await editorAddFixture(data);
    } else if (selection?.kind === 'project_fixture') {
      await editorUpdateFixture(selection.itemIdx, data);
    }
    editDirty = false;
    selection = null; editFixture = null;
    await refresh();
  }

  async function saveEditBankFixture() {
    if (!editBankFixture || !selection || selection.kind !== 'bank_fixture') return;
    const data = { ...editBankFixture };
    delete (data as any).index;
    if (editIsNew) {
      await editorBankAddFixture(selection.bankIdx!, data);
    } else {
      await editorBankUpdateFixture(selection.bankIdx!, selection.itemIdx, data);
    }
    editDirty = false;
    selection = null; editBankFixture = null;
    await refresh();
  }

  async function deleteEditFixture() {
    if (!selection) return;
    if (selection.kind === 'project_fixture' && selection.itemIdx >= 0) {
      await editorDeleteFixture(selection.itemIdx);
    } else if (selection.kind === 'bank_fixture' && selection.itemIdx >= 0) {
      await editorBankDeleteFixture(selection.bankIdx!, selection.itemIdx);
    }
    editDirty = false;
    selection = null; editFixture = null; editBankFixture = null;
    await refresh();
  }

  /* ---- Channel helpers ---- */
  function addChannel() {
    if (editFixture) {
      editFixture.channels = [...editFixture.channels, { name: '', offset: editFixture.channels.length }];
      editFixture.channel_count = editFixture.channels.length;
      markDirty();
    } else if (editBankFixture) {
      editBankFixture.channels = [...editBankFixture.channels, { name: '', offset: editBankFixture.channels.length }];
      editBankFixture.channel_count = editBankFixture.channels.length;
      markDirty();
    }
  }

  function removeChannel(i: number) {
    if (editFixture) {
      editFixture.channels = editFixture.channels.filter((_, idx) => idx !== i);
      editFixture.channel_count = editFixture.channels.length;
      markDirty();
    } else if (editBankFixture) {
      editBankFixture.channels = editBankFixture.channels.filter((_, idx) => idx !== i);
      editBankFixture.channel_count = editBankFixture.channels.length;
      markDirty();
    }
  }

  /* ---- Helpers ---- */
  function isSelected(kind: SelectionKind, bankIdx: number | undefined, itemIdx: number): boolean {
    if (!selection) return false;
    return selection.kind === kind && selection.bankIdx === bankIdx && selection.itemIdx === itemIdx;
  }
</script>

<!-- Project bar -->
<header class="project-bar">
  {#if status.project_loaded}
    <span class="project-path">{status.project_path}</span>
    {#if status.dirty}<span class="dirty-badge">unsaved</span>{/if}
    <div class="project-actions">
      <button class="btn-sm" onclick={handleSave}>Save</button>
      <button class="btn-sm btn-muted" onclick={handleClose}>Close</button>
    </div>
  {:else}
    <input class="open-input" type="text" placeholder="Paste path or browse..." bind:value={openPath} onkeydown={(e) => { if (e.key === 'Enter') handleOpen(); }} />
    <button class="btn-sm" onclick={handleOpen}>Open</button>
    <button class="btn-sm btn-muted" onclick={openBrowser}>Browse</button>
  {/if}
</header>

<!-- File browser -->
{#if showBrowser && browseResult}
  <section class="browser-panel">
    <div class="browser-header">
      <button class="btn-xs" onclick={browseUp}>..</button>
      <span class="browser-path">{browseResult.path}</span>
      <button class="btn-xs btn-muted" onclick={() => showBrowser = false}>Close</button>
    </div>
    <div class="browser-list">
      {#each browseResult.entries.filter(e => e.type === 'dir').sort((a, b) => a.name.localeCompare(b.name)) as entry}
        <button class="browser-entry dir" onclick={() => browseNav(entry.name)}>{entry.name}/</button>
      {/each}
      {#each browseResult.entries.filter(e => e.type === 'file').sort((a, b) => a.name.localeCompare(b.name)) as entry}
        <button class="browser-entry file" onclick={() => browseSelect(entry.name)}>{entry.name}</button>
      {/each}
      {#if browseResult.entries.length === 0}
        <span class="empty-msg">Empty directory</span>
      {/if}
    </div>
  </section>
{/if}

<!-- Main editor layout -->
<div class="editor-layout">
  <!-- Selection Card -->
  <div class="selection-card">
    <div class="sel-tabs">
      <button class="sel-tab" class:active={selectionTab === 'banks'} onclick={() => selectionTab = 'banks'}>Banks</button>
      <button class="sel-tab" class:active={selectionTab === 'fixtures'} onclick={() => selectionTab = 'fixtures'}>Fixtures</button>
      <button class="sel-tab" class:active={selectionTab === 'hardware'} onclick={() => selectionTab = 'hardware'}>Hardware</button>
      <button class="sel-tab" class:active={selectionTab === 'scenes'} onclick={() => selectionTab = 'scenes'}>Scenes</button>
    </div>

    <div class="sel-content">
      {#if selectionTab === 'banks'}
        <!-- New bank form -->
        {#if bankDirs.length > 0}
          <div class="new-bank-row">
            <input type="text" placeholder="new bank id" bind:value={newBankId} class="input-sm" />
            <select bind:value={newBankDir} class="input-sm">
              {#each bankDirs as dir}
                <option value={dir}>{dir}</option>
              {/each}
            </select>
            <button class="btn-xs btn-add" onclick={handleCreateBank}>Create</button>
          </div>
        {/if}

        {#each banks as bank (bank.id)}
          <div class="bank-group">
            <div class="bank-group-header" onclick={() => toggleBank(bank.index)} role="button" tabindex="0" onkeydown={(e) => { if (e.key === 'Enter') toggleBank(bank.index); }}>
              <span class="collapse-icon">{collapsedBanks.has(bank.index) ? '\u25B6' : '\u25BC'}</span>
              <span class="bank-id">{bank.id}</span>
              {#if bank.dirty}<span class="dirty-badge">*</span>{/if}
              <span class="bank-count">{bank.fixtures.length}</span>
              <button class="btn-xs" onclick={(e) => { e.stopPropagation(); handleBankSave(bank.index); }}>Save</button>
              <button class="btn-xs btn-add" onclick={(e) => { e.stopPropagation(); startAddBankFixture(bank.index); }}>+</button>
            </div>
            {#if !collapsedBanks.has(bank.index)}
              <div class="pad-grid">
                {#each bank.fixtures as fix (fix.index)}
                  <button
                    class="pad-btn"
                    class:selected={isSelected('bank_fixture', bank.index, fix.index!)}
                    onclick={() => trySelect({ kind: 'bank_fixture', bankIdx: bank.index, itemIdx: fix.index! })}
                  >
                    {fix.id}
                  </button>
                {/each}
              </div>
            {/if}
          </div>
        {/each}

        {#if banks.length === 0}
          <p class="empty-msg">No banks. Set SPARK_FIXTURE_BANK_PATH.</p>
        {/if}

      {:else if selectionTab === 'fixtures'}
        {#if status.project_loaded}
          <div class="sel-toolbar">
            <button class="btn-xs btn-add" onclick={startAddProjectFixture}>+ Add Fixture</button>
          </div>
          <div class="pad-grid">
            {#each fixtures as fix (fix.index)}
              <button
                class="pad-btn"
                class:selected={isSelected('project_fixture', undefined, fix.index!)}
                onclick={() => trySelect({ kind: 'project_fixture', itemIdx: fix.index! })}
              >
                <span class="pad-id">{fix.id}</span>
                <span class="pad-sub">@{fix.start_address}</span>
              </button>
            {/each}
          </div>
        {:else}
          <p class="empty-msg">Open a project first.</p>
        {/if}

      {:else if selectionTab === 'hardware'}
        {#if status.project_loaded}
          <div class="hw-display">
            <p class="hw-note">Read-only. Edit in project YAML.</p>
            <div class="hw-grid">
              <span class="hw-label">Project</span>
              <span class="hw-value">{status.project_path}</span>
            </div>
          </div>
        {:else}
          <p class="empty-msg">Open a project first.</p>
        {/if}

      {:else if selectionTab === 'scenes'}
        {#if status.project_loaded}
          <div class="pad-grid">
            {#each scenes as scene, i}
              <button
                class="pad-btn"
                class:selected={isSelected('scene', undefined, i)}
                onclick={() => trySelect({ kind: 'scene', itemIdx: i })}
              >
                <span class="pad-id">{scene.id}</span>
                <span class="pad-sub">{scene.type}</span>
              </button>
            {/each}
          </div>
          {#if scenes.length === 0}
            <p class="empty-msg">No scenes loaded.</p>
          {/if}
        {:else}
          <p class="empty-msg">Open a project first.</p>
        {/if}
      {/if}
    </div>
  </div>

  <!-- Edit Card -->
  {#if selection}
    <div class="edit-card">
      <div class="edit-header">
        <span class="edit-title">
          {#if selection.kind === 'project_fixture'}
            {editIsNew ? 'New Fixture' : `Fixture: ${editFixture?.id ?? ''}`}
          {:else if selection.kind === 'bank_fixture'}
            {editIsNew ? 'New Bank Fixture' : `Bank Fixture: ${editBankFixture?.id ?? ''}`}
          {:else if selection.kind === 'scene'}
            Scene: {scenes[selection.itemIdx]?.id ?? ''}
          {/if}
        </span>
        {#if editDirty}<span class="dirty-badge">modified</span>{/if}
        <div class="edit-actions">
          {#if selection.kind !== 'scene'}
            <button class="btn-sm btn-save" onclick={() => { if (editFixture) saveEditFixture(); else saveEditBankFixture(); }}>Save</button>
            {#if !editIsNew}
              <button class="btn-sm btn-danger" onclick={deleteEditFixture}>Delete</button>
            {/if}
          {/if}
          <button class="btn-sm btn-muted" onclick={closeEdit}>Close</button>
        </div>
      </div>

      <div class="edit-body">
        <!-- Project fixture form -->
        {#if selection.kind === 'project_fixture' && editFixture}
          <div class="form-grid">
            <label for="ef-id">ID</label>
            <input id="ef-id" type="text" bind:value={editFixture.id} oninput={markDirty} />
            <label for="ef-name">Name</label>
            <input id="ef-name" type="text" bind:value={editFixture.name} oninput={markDirty} />
            <label for="ef-addr">Start Addr</label>
            <input id="ef-addr" type="number" min="1" max="512" bind:value={editFixture.start_address} oninput={markDirty} />
            <label for="ef-tpl">Template</label>
            <input id="ef-tpl" type="text" placeholder="bank:fixture" bind:value={editFixture.template} oninput={markDirty} />
            <label for="ef-copy">Copy From</label>
            <input id="ef-copy" type="text" placeholder="fixture-id" bind:value={editFixture.copy_from} oninput={markDirty} />
          </div>

          {#if !editFixture.template && !editFixture.copy_from}
            <div class="channel-section">
              <div class="ch-header">
                <span>Channels ({editFixture.channel_count})</span>
                <button class="btn-xs btn-add" onclick={addChannel}>+</button>
              </div>
              {#each editFixture.channels as ch, i}
                <div class="channel-row">
                  <input type="text" placeholder="name" bind:value={ch.name} oninput={markDirty} class="ch-name" />
                  <input type="number" min="0" max="255" bind:value={ch.offset} oninput={markDirty} class="ch-offset" />
                  <button class="btn-xs btn-danger" onclick={() => removeChannel(i)}>x</button>
                </div>
              {/each}
            </div>
          {/if}

        <!-- Bank fixture form -->
        {:else if selection.kind === 'bank_fixture' && editBankFixture}
          <div class="form-grid">
            <label for="bf-id">ID</label>
            <input id="bf-id" type="text" bind:value={editBankFixture.id} oninput={markDirty} />
            <label for="bf-name">Name</label>
            <input id="bf-name" type="text" bind:value={editBankFixture.name} oninput={markDirty} />
          </div>

          <div class="channel-section">
            <div class="ch-header">
              <span>Channels ({editBankFixture.channel_count})</span>
              <button class="btn-xs btn-add" onclick={addChannel}>+</button>
            </div>
            {#each editBankFixture.channels as ch, i}
              <div class="channel-row">
                <input type="text" placeholder="name" bind:value={ch.name} oninput={markDirty} class="ch-name" />
                <input type="number" min="0" max="255" bind:value={ch.offset} oninput={markDirty} class="ch-offset" />
                <button class="btn-xs btn-danger" onclick={() => removeChannel(i)}>x</button>
              </div>
            {/each}
          </div>

        <!-- Scene read-only view -->
        {:else if selection.kind === 'scene'}
          {@const scene = scenes[selection.itemIdx]}
          {#if scene}
            <div class="scene-view">
              <div class="form-grid">
                <span class="hw-label">ID</span><span class="hw-value">{scene.id}</span>
                <span class="hw-label">Name</span><span class="hw-value">{scene.name}</span>
                <span class="hw-label">Type</span><span class="hw-value">{scene.type}</span>
                <span class="hw-label">Trigger</span><span class="hw-value">{scene.trigger_mode} ch{scene.channel} note{scene.note}</span>
                <span class="hw-label">Enabled</span><span class="hw-value">{scene.enabled ? 'yes' : 'no'}</span>
              </div>
              <p class="hw-note">Scene editing coming soon.</p>
            </div>
          {/if}
        {/if}
      </div>
    </div>
  {/if}
</div>

<!-- Discard confirmation modal -->
{#if showDiscardModal}
  <div class="modal-overlay" onclick={cancelDiscard} role="presentation">
    <div class="modal" onclick={(e) => e.stopPropagation()} onkeydown={() => {}} role="dialog" aria-modal="true" tabindex="-1">
      <p>You have unsaved changes. Discard them?</p>
      <div class="modal-actions">
        <button class="btn-sm btn-danger" onclick={confirmDiscard}>Discard</button>
        <button class="btn-sm btn-muted" onclick={cancelDiscard}>Cancel</button>
      </div>
    </div>
  </div>
{/if}

<style>
  /* Project bar */
  .project-bar {
    display: flex;
    align-items: center;
    gap: 0.8rem;
    padding: 0.5rem 1rem;
    background: var(--bg-surface);
    border-radius: var(--radius);
    margin-bottom: 0.8rem;
  }
  .project-path { font-size: 0.8rem; color: var(--text-muted); overflow: hidden; text-overflow: ellipsis; white-space: nowrap; max-width: 400px; }
  .dirty-badge { font-size: 0.65rem; font-weight: 700; color: var(--yellow); text-transform: uppercase; }
  .project-actions { margin-left: auto; display: flex; gap: 0.4rem; }
  .open-input { flex: 1; background: var(--bg-card); border: 1px solid var(--text-muted); border-radius: var(--radius); color: var(--text); padding: 0.35rem 0.6rem; font-size: 0.8rem; }
  .open-input::placeholder { color: var(--text-muted); }

  /* File browser */
  .browser-panel { background: var(--bg-card); border-radius: var(--radius); padding: 0.6rem; margin-bottom: 0.8rem; max-height: 250px; display: flex; flex-direction: column; }
  .browser-header { display: flex; align-items: center; gap: 0.4rem; margin-bottom: 0.4rem; padding-bottom: 0.4rem; border-bottom: 1px solid var(--bg-surface); }
  .browser-path { flex: 1; font-size: 0.7rem; color: var(--text-muted); font-family: monospace; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
  .browser-list { overflow-y: auto; display: flex; flex-direction: column; gap: 0.1rem; }
  .browser-entry { text-align: left; padding: 0.25rem 0.5rem; font-size: 0.75rem; border-radius: 4px; background: none; color: var(--text); font-weight: 400; }
  .browser-entry:hover { background: var(--bg-surface); }
  .browser-entry.dir { color: var(--green); font-weight: 600; }

  /* Main layout */
  .editor-layout {
    display: flex;
    flex-direction: row;
    gap: 0.8rem;
    height: calc(100vh - 140px);
    min-height: 300px;
  }

  @media (max-width: 800px) {
    .editor-layout { flex-direction: column; height: auto; }
  }

  /* Selection card */
  .selection-card {
    flex: 2;
    display: flex;
    flex-direction: column;
    background: var(--bg-surface);
    border-radius: var(--radius);
    overflow: hidden;
    min-width: 0;
  }

  .sel-tabs {
    display: flex;
    border-bottom: 1px solid var(--bg-card);
    flex-shrink: 0;
  }
  .sel-tab {
    flex: 1;
    padding: 0.5rem 0.3rem;
    font-size: 0.7rem;
    font-weight: 600;
    background: none;
    color: var(--text-muted);
    border: none;
    border-radius: 0;
    border-bottom: 2px solid transparent;
    text-transform: uppercase;
    letter-spacing: 0.03em;
  }
  .sel-tab:active { transform: none; }
  .sel-tab.active { color: var(--accent); border-bottom-color: var(--accent); }

  .sel-content {
    flex: 1;
    overflow-y: auto;
    padding: 0.6rem;
  }

  .sel-toolbar { margin-bottom: 0.5rem; }

  /* Pad grid */
  .pad-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(70px, 1fr));
    gap: 0.35rem;
  }

  .pad-btn {
    aspect-ratio: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 0.15rem;
    background: var(--bg-card);
    border: 2px solid transparent;
    border-radius: var(--radius);
    color: var(--text);
    font-size: 0.65rem;
    font-weight: 600;
    padding: 0.3rem;
    user-select: none;
    word-break: break-all;
    text-align: center;
    min-width: 0;
  }
  .pad-btn:hover { border-color: var(--accent); }
  .pad-btn.selected { background: var(--accent); color: white; border-color: var(--accent-hover); }
  .pad-id { font-size: 0.7rem; }
  .pad-sub { font-size: 0.55rem; color: var(--text-muted); }
  .pad-btn.selected .pad-sub { color: rgba(255,255,255,0.7); }

  /* Bank groups */
  .bank-group { margin-bottom: 0.6rem; }
  .bank-group-header {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    padding: 0.3rem 0.4rem;
    background: var(--bg-card);
    border-radius: var(--radius);
    cursor: pointer;
    user-select: none;
    font-size: 0.75rem;
  }
  .collapse-icon { width: 0.8rem; color: var(--text-muted); font-size: 0.6rem; }
  .bank-id { font-weight: 700; color: var(--accent); }
  .bank-count { color: var(--text-muted); margin-left: auto; font-size: 0.65rem; }

  .new-bank-row {
    display: flex;
    gap: 0.4rem;
    align-items: center;
    margin-bottom: 0.6rem;
  }
  .input-sm {
    background: var(--bg-card);
    border: 1px solid var(--text-muted);
    border-radius: 4px;
    color: var(--text);
    padding: 0.25rem 0.5rem;
    font-size: 0.7rem;
    min-width: 0;
  }
  .new-bank-row select { flex: 1; }
  .new-bank-row input { width: 90px; }

  /* Edit card */
  .edit-card {
    flex: 3;
    display: flex;
    flex-direction: column;
    background: var(--bg-card);
    border-radius: var(--radius);
    overflow: hidden;
    min-width: 0;
  }

  .edit-header {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    padding: 0.5rem 0.8rem;
    border-bottom: 1px solid var(--bg-surface);
    flex-shrink: 0;
  }
  .edit-title { font-size: 0.8rem; font-weight: 700; color: var(--text); }
  .edit-actions { margin-left: auto; display: flex; gap: 0.3rem; }

  .edit-body {
    flex: 1;
    overflow-y: auto;
    padding: 0.8rem;
  }

  /* Forms */
  .form-grid {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 0.35rem 0.6rem;
    align-items: center;
    margin-bottom: 0.6rem;
  }
  .form-grid label, .hw-label { font-size: 0.75rem; color: var(--text-muted); font-weight: 600; }
  .form-grid input, .form-grid select {
    background: var(--bg-surface);
    border: 1px solid var(--text-muted);
    border-radius: 4px;
    color: var(--text);
    padding: 0.3rem 0.5rem;
    font-size: 0.75rem;
  }
  .hw-value { font-size: 0.75rem; color: var(--text); }
  .hw-note { font-size: 0.7rem; color: var(--text-muted); font-style: italic; margin-top: 0.5rem; }
  .hw-display { padding: 0.3rem; }
  .hw-grid { display: grid; grid-template-columns: auto 1fr; gap: 0.3rem 0.6rem; align-items: center; }

  /* Channels */
  .channel-section { margin-top: 0.6rem; }
  .ch-header { display: flex; align-items: center; justify-content: space-between; margin-bottom: 0.3rem; font-size: 0.75rem; color: var(--text-muted); font-weight: 600; }
  .channel-row { display: flex; gap: 0.3rem; align-items: center; margin-bottom: 0.2rem; }
  .ch-name { flex: 1; background: var(--bg-surface); border: 1px solid var(--text-muted); border-radius: 4px; color: var(--text); padding: 0.25rem 0.4rem; font-size: 0.75rem; }
  .ch-offset { width: 50px; background: var(--bg-surface); border: 1px solid var(--text-muted); border-radius: 4px; color: var(--text); padding: 0.25rem 0.4rem; font-size: 0.75rem; text-align: center; }

  /* Scene view */
  .scene-view { padding: 0.2rem; }

  /* Buttons */
  .btn-sm { padding: 0.3rem 0.7rem; font-size: 0.7rem; border-radius: 4px; background: var(--accent); color: white; }
  .btn-sm.btn-muted { background: var(--bg-surface); color: var(--text-muted); }
  .btn-sm.btn-save { background: var(--green); color: #111; }
  .btn-sm.btn-danger { background: var(--red); color: white; }
  .btn-xs { padding: 0.2rem 0.4rem; font-size: 0.6rem; border-radius: 4px; background: var(--bg-card); color: var(--text); }
  .btn-xs.btn-danger { background: var(--red); color: white; }
  .btn-xs.btn-add { background: var(--green); color: #111; }
  .btn-xs.btn-muted { background: var(--bg-surface); color: var(--text-muted); }

  .empty-msg { color: var(--text-muted); font-size: 0.8rem; padding: 1.5rem; text-align: center; }

  /* Modal */
  .modal-overlay {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.6);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 1000;
  }
  .modal {
    background: var(--bg-card);
    border-radius: var(--radius);
    padding: 1.5rem;
    max-width: 320px;
    width: 90%;
  }
  .modal p { font-size: 0.85rem; margin-bottom: 1rem; color: var(--text); }
  .modal-actions { display: flex; gap: 0.5rem; justify-content: flex-end; }
</style>
