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
    type EditorStatus,
    type EditorFixture,
    type EditorBank,
    type BankFixture,
    type BrowseResult,
  } from '../lib/api';

  let status: EditorStatus = $state({
    project_loaded: false,
    project_path: '',
    dirty: false,
    fixture_count: 0,
    bank_count: 0,
  });

  let fixtures: EditorFixture[] = $state([]);
  let banks: EditorBank[] = $state([]);
  let activeTab: 'project' | 'banks' = $state('project');

  /* Fixture editing */
  let editingFixture: EditorFixture | null = $state(null);
  let isNew = $state(false);

  /* Bank fixture editing */
  let editingBankIdx: number = $state(-1);
  let editingBankFixture: BankFixture | null = $state(null);

  /* Collapsible banks */
  let collapsedBanks: Set<number> = $state(new Set());

  /* New bank form */
  let bankDirs: string[] = $state([]);
  let newBankId = $state('');
  let newBankDir = $state('');

  /* File browser */
  let showBrowser = $state(false);
  let browseResult: BrowseResult | null = $state(null);
  let openPath = $state('');

  async function refresh() {
    status = await editorStatus();
    if (status.project_loaded) {
      fixtures = await editorGetFixtures();
    }
    banks = await editorGetBanks();
    bankDirs = await editorGetBankDirs();
    if (bankDirs.length > 0 && !newBankDir) newBankDir = bankDirs[0];
  }

  onMount(() => { refresh(); });

  /* Project lifecycle */
  async function handleOpen() {
    if (!openPath) return;
    await editorOpen(openPath);
    showBrowser = false;
    await refresh();
  }

  async function handleClose() {
    await editorClose();
    fixtures = [];
    await refresh();
  }

  async function handleSave() {
    await editorSave();
    await refresh();
  }

  /* File browser */
  async function openBrowser() {
    showBrowser = true;
    browseResult = await editorBrowse('');
  }

  async function browseTo(path: string) {
    browseResult = await editorBrowse(path);
  }

  function browseSelect(name: string) {
    if (!browseResult) return;
    const full = browseResult.path + '/' + name;
    openPath = full;
    showBrowser = false;
    editorOpen(full).then(() => refresh());
  }

  function browseNav(name: string) {
    if (!browseResult) return;
    browseTo(browseResult.path + '/' + name);
  }

  function browseUp() {
    if (!browseResult) return;
    const parts = browseResult.path.replace(/\\/g, '/').split('/');
    parts.pop();
    const parent = parts.join('/') || '/';
    browseTo(parent);
  }

  /* Fixture CRUD */
  function startAddFixture() {
    editingFixture = {
      id: '', name: '', start_address: 1, channel_count: 0,
      template: '', copy_from: '', channels: [],
    };
    isNew = true;
  }

  function startEditFixture(f: EditorFixture) {
    editingFixture = { ...f, channels: f.channels.map(c => ({ ...c })) };
    isNew = false;
  }

  async function saveFixture() {
    if (!editingFixture) return;
    const data = { ...editingFixture };
    delete (data as any).index;
    if (isNew) {
      await editorAddFixture(data);
    } else {
      await editorUpdateFixture(editingFixture.index!, data);
    }
    editingFixture = null;
    await refresh();
  }

  async function deleteFixture(index: number) {
    await editorDeleteFixture(index);
    await refresh();
  }

  function addChannel() {
    if (!editingFixture) return;
    editingFixture.channels = [...editingFixture.channels, { name: '', offset: editingFixture.channels.length }];
    editingFixture.channel_count = editingFixture.channels.length;
  }

  function removeChannel(i: number) {
    if (!editingFixture) return;
    editingFixture.channels = editingFixture.channels.filter((_, idx) => idx !== i);
    editingFixture.channel_count = editingFixture.channels.length;
  }

  /* Bank collapse */
  function toggleBank(idx: number) {
    if (collapsedBanks.has(idx)) {
      collapsedBanks = new Set([...collapsedBanks].filter(i => i !== idx));
    } else {
      collapsedBanks = new Set([...collapsedBanks, idx]);
    }
  }

  /* New bank */
  async function handleCreateBank() {
    if (!newBankId || !newBankDir) return;
    await editorCreateBank(newBankId, newBankDir);
    newBankId = '';
    await refresh();
  }

  /* Bank fixture editing */
  function startAddBankFixture(bankIdx: number) {
    editingBankIdx = bankIdx;
    editingBankFixture = { id: '', name: '', channel_count: 0, channels: [] };
    isNew = true;
  }

  function startEditBankFixture(bankIdx: number, f: BankFixture) {
    editingBankIdx = bankIdx;
    editingBankFixture = { ...f, channels: f.channels.map(c => ({ ...c })) };
    isNew = false;
  }

  async function saveBankFixture() {
    if (!editingBankFixture || editingBankIdx < 0) return;
    const data = { ...editingBankFixture };
    delete (data as any).index;
    if (isNew) {
      await editorBankAddFixture(editingBankIdx, data);
    } else {
      await editorBankUpdateFixture(editingBankIdx, editingBankFixture.index!, data);
    }
    editingBankFixture = null;
    editingBankIdx = -1;
    await refresh();
  }

  async function deleteBankFixture(bankIdx: number, fixIdx: number) {
    await editorBankDeleteFixture(bankIdx, fixIdx);
    await refresh();
  }

  async function handleBankSave(bankIdx: number) {
    await editorBankSave(bankIdx);
    await refresh();
  }

  function addBankChannel() {
    if (!editingBankFixture) return;
    editingBankFixture.channels = [...editingBankFixture.channels, { name: '', offset: editingBankFixture.channels.length }];
    editingBankFixture.channel_count = editingBankFixture.channels.length;
  }

  function removeBankChannel(i: number) {
    if (!editingBankFixture) return;
    editingBankFixture.channels = editingBankFixture.channels.filter((_, idx) => idx !== i);
    editingBankFixture.channel_count = editingBankFixture.channels.length;
  }
</script>

<!-- Project bar -->
<header class="project-bar">
  {#if status.project_loaded}
    <span class="project-path">{status.project_path}</span>
    {#if status.dirty}
      <span class="dirty-badge">unsaved</span>
    {/if}
    <div class="project-actions">
      <button class="btn-sm" onclick={handleSave}>Save</button>
      <button class="btn-sm btn-muted" onclick={handleClose}>Close</button>
    </div>
  {:else}
    <input
      class="open-input"
      type="text"
      placeholder="Paste path or browse..."
      bind:value={openPath}
      onkeydown={(e) => { if (e.key === 'Enter') handleOpen(); }}
    />
    <button class="btn-sm" onclick={handleOpen}>Open</button>
    <button class="btn-sm btn-muted" onclick={openBrowser}>Browse</button>
  {/if}
</header>

<!-- File browser panel -->
{#if showBrowser && browseResult}
  <section class="browser-panel">
    <div class="browser-header">
      <button class="btn-xs" onclick={browseUp}>..</button>
      <span class="browser-path">{browseResult.path}</span>
      <button class="btn-xs btn-muted" onclick={() => showBrowser = false}>Close</button>
    </div>
    <div class="browser-list">
      {#each browseResult.entries.filter(e => e.type === 'dir').sort((a, b) => a.name.localeCompare(b.name)) as entry}
        <button class="browser-entry dir" onclick={() => browseNav(entry.name)}>
          {entry.name}/
        </button>
      {/each}
      {#each browseResult.entries.filter(e => e.type === 'file').sort((a, b) => a.name.localeCompare(b.name)) as entry}
        <button class="browser-entry file" onclick={() => browseSelect(entry.name)}>
          {entry.name}
        </button>
      {/each}
      {#if browseResult.entries.length === 0}
        <span class="empty-msg">Empty directory</span>
      {/if}
    </div>
  </section>
{/if}

<!-- Tab switcher -->
<div class="editor-tabs">
  <button class="editor-tab" class:active={activeTab === 'project'} onclick={() => activeTab = 'project'}>
    Fixtures ({fixtures.length})
  </button>
  <button class="editor-tab" class:active={activeTab === 'banks'} onclick={() => activeTab = 'banks'}>
    Banks ({banks.length})
  </button>
</div>

{#if activeTab === 'project'}
  {#if status.project_loaded}
    <section class="fixture-list">
      <div class="list-header">
        <h3>Project Fixtures</h3>
        <button class="btn-sm btn-add" onclick={startAddFixture}>+ Add</button>
      </div>
      {#each fixtures as fix}
        <div class="fixture-row">
          <div class="fixture-info">
            <span class="fixture-id">{fix.id}</span>
            <span class="fixture-name">{fix.name}</span>
            <span class="fixture-addr">@{fix.start_address}</span>
            {#if fix.template}
              <span class="fixture-ref">tpl:{fix.template}</span>
            {:else if fix.copy_from}
              <span class="fixture-ref">copy:{fix.copy_from}</span>
            {:else}
              <span class="fixture-channels">{fix.channel_count}ch</span>
            {/if}
          </div>
          <div class="fixture-actions">
            <button class="btn-xs" onclick={() => startEditFixture(fix)}>Edit</button>
            <button class="btn-xs btn-danger" onclick={() => deleteFixture(fix.index!)}>Del</button>
          </div>
        </div>
      {/each}
    </section>
  {:else}
    <p class="empty-msg">Open a project to edit fixtures.</p>
  {/if}

  {#if editingFixture}
    <section class="edit-form">
      <h3>{isNew ? 'Add Fixture' : 'Edit Fixture'}</h3>
      <div class="form-grid">
        <label for="fix-id">ID</label>
        <input id="fix-id" type="text" bind:value={editingFixture.id} />
        <label for="fix-name">Name</label>
        <input id="fix-name" type="text" bind:value={editingFixture.name} />
        <label for="fix-addr">Start Address</label>
        <input id="fix-addr" type="number" min="1" max="512" bind:value={editingFixture.start_address} />
        <label for="fix-tpl">Template</label>
        <input id="fix-tpl" type="text" placeholder="bank-id:fixture-id" bind:value={editingFixture.template} />
        <label for="fix-copy">Copy From</label>
        <input id="fix-copy" type="text" placeholder="fixture-id" bind:value={editingFixture.copy_from} />
      </div>

      {#if !editingFixture.template && !editingFixture.copy_from}
        <div class="channel-section">
          <div class="list-header">
            <h4>Channels</h4>
            <button class="btn-xs" onclick={addChannel}>+ Channel</button>
          </div>
          {#each editingFixture.channels as ch, i}
            <div class="channel-row">
              <input type="text" placeholder="name" bind:value={ch.name} class="ch-name" />
              <input type="number" min="0" max="255" bind:value={ch.offset} class="ch-offset" />
              <button class="btn-xs btn-danger" onclick={() => removeChannel(i)}>x</button>
            </div>
          {/each}
        </div>
      {/if}

      <div class="form-actions">
        <button class="btn-sm btn-save" onclick={saveFixture}>
          {isNew ? 'Add' : 'Save'}
        </button>
        <button class="btn-sm btn-muted" onclick={() => editingFixture = null}>Cancel</button>
      </div>
    </section>
  {/if}

{:else}
  <!-- New bank form -->
  {#if bankDirs.length > 0}
    <div class="new-bank-form">
      <label for="new-bank-id">New Bank:</label>
      <input id="new-bank-id" type="text" placeholder="bank-id" bind:value={newBankId} />
      <select bind:value={newBankDir}>
        {#each bankDirs as dir}
          <option value={dir}>{dir}</option>
        {/each}
      </select>
      <button class="btn-sm btn-add" onclick={handleCreateBank}>Create</button>
    </div>
  {/if}

  <!-- Bank cards -->
  {#each banks as bank}
    <section class="bank-card">
      <div class="bank-header" onclick={() => toggleBank(bank.index)} role="button" tabindex="0" onkeydown={(e) => { if (e.key === 'Enter') toggleBank(bank.index); }}>
        <span class="bank-collapse">{collapsedBanks.has(bank.index) ? '\u25B6' : '\u25BC'}</span>
        <h3>{bank.id}</h3>
        <span class="bank-path">{bank.path}</span>
        {#if bank.dirty}
          <span class="dirty-badge">unsaved</span>
        {/if}
        <span class="bank-count">{bank.fixtures.length} fixtures</span>
      </div>
      <div class="bank-actions-bar">
        <button class="btn-sm" onclick={() => handleBankSave(bank.index)}>Save</button>
        <button class="btn-sm btn-add" onclick={() => startAddBankFixture(bank.index)}>+ Add Fixture</button>
      </div>

      {#if !collapsedBanks.has(bank.index)}
        <div class="bank-fixtures">
          {#each bank.fixtures as fix}
            <div class="fixture-row">
              <div class="fixture-info">
                <span class="fixture-id">{bank.id}:{fix.id}</span>
                <span class="fixture-name">{fix.name}</span>
                <span class="fixture-channels">{fix.channel_count}ch</span>
              </div>
              <div class="fixture-actions">
                <button class="btn-xs" onclick={() => startEditBankFixture(bank.index, fix)}>Edit</button>
                <button class="btn-xs btn-danger" onclick={() => deleteBankFixture(bank.index, fix.index!)}>Del</button>
              </div>
            </div>
          {/each}
        </div>
      {/if}
    </section>
  {/each}

  {#if banks.length === 0}
    <p class="empty-msg">No fixture banks found. Set SPARK_FIXTURE_BANK_PATH in .spark.env.</p>
  {/if}

  {#if editingBankFixture}
    <section class="edit-form">
      <h3>{isNew ? 'Add Bank Fixture' : 'Edit Bank Fixture'}</h3>
      <div class="form-grid">
        <label for="bfix-id">ID</label>
        <input id="bfix-id" type="text" bind:value={editingBankFixture.id} />
        <label for="bfix-name">Name</label>
        <input id="bfix-name" type="text" bind:value={editingBankFixture.name} />
      </div>

      <div class="channel-section">
        <div class="list-header">
          <h4>Channels</h4>
          <button class="btn-xs" onclick={addBankChannel}>+ Channel</button>
        </div>
        {#each editingBankFixture.channels as ch, i}
          <div class="channel-row">
            <input type="text" placeholder="name" bind:value={ch.name} class="ch-name" />
            <input type="number" min="0" max="255" bind:value={ch.offset} class="ch-offset" />
            <button class="btn-xs btn-danger" onclick={() => removeBankChannel(i)}>x</button>
          </div>
        {/each}
      </div>

      <div class="form-actions">
        <button class="btn-sm btn-save" onclick={saveBankFixture}>
          {isNew ? 'Add' : 'Save'}
        </button>
        <button class="btn-sm btn-muted" onclick={() => { editingBankFixture = null; editingBankIdx = -1; }}>Cancel</button>
      </div>
    </section>
  {/if}
{/if}

<style>
  .project-bar {
    display: flex;
    align-items: center;
    gap: 0.8rem;
    padding: 0.6rem 1rem;
    background: var(--bg-surface);
    border-radius: var(--radius);
    margin-bottom: 1rem;
  }

  .project-path {
    font-size: 0.8rem;
    color: var(--text-muted);
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
    max-width: 400px;
  }

  .dirty-badge {
    font-size: 0.7rem;
    font-weight: 700;
    color: var(--yellow);
    text-transform: uppercase;
  }

  .project-actions {
    margin-left: auto;
    display: flex;
    gap: 0.4rem;
  }

  .open-input {
    flex: 1;
    background: var(--bg-card);
    border: 1px solid var(--text-muted);
    border-radius: var(--radius);
    color: var(--text);
    padding: 0.4rem 0.7rem;
    font-size: 0.85rem;
  }

  .open-input::placeholder {
    color: var(--text-muted);
  }

  /* File browser */
  .browser-panel {
    background: var(--bg-card);
    border-radius: var(--radius);
    padding: 0.8rem;
    margin-bottom: 1rem;
    max-height: 300px;
    display: flex;
    flex-direction: column;
  }

  .browser-header {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    margin-bottom: 0.5rem;
    padding-bottom: 0.5rem;
    border-bottom: 1px solid var(--bg-surface);
  }

  .browser-path {
    flex: 1;
    font-size: 0.75rem;
    color: var(--text-muted);
    font-family: monospace;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .browser-list {
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 0.15rem;
  }

  .browser-entry {
    text-align: left;
    padding: 0.3rem 0.6rem;
    font-size: 0.8rem;
    border-radius: 4px;
    background: none;
    color: var(--text);
    font-weight: 400;
  }

  .browser-entry:hover {
    background: var(--bg-surface);
  }

  .browser-entry.dir {
    color: var(--green);
    font-weight: 600;
  }

  .browser-entry.file {
    color: var(--text);
  }

  /* Editor tabs */
  .editor-tabs {
    display: flex;
    gap: 0;
    margin-bottom: 1rem;
  }

  .editor-tab {
    background: var(--bg-surface);
    color: var(--text-muted);
    border: none;
    border-radius: var(--radius) var(--radius) 0 0;
    padding: 0.5rem 1.2rem;
    font-size: 0.8rem;
    font-weight: 600;
  }

  .editor-tab.active {
    background: var(--bg-card);
    color: var(--text);
  }

  /* Fixture list */
  .fixture-list {
    margin-bottom: 1.5rem;
  }

  .list-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 0.5rem;
  }

  .list-header h3, .list-header h4 {
    font-size: 0.9rem;
    font-weight: 600;
    color: var(--text);
  }

  .fixture-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0.5rem 0.8rem;
    background: var(--bg-surface);
    border-radius: var(--radius);
    margin-bottom: 0.3rem;
  }

  .fixture-info {
    display: flex;
    align-items: center;
    gap: 0.8rem;
    font-size: 0.8rem;
  }

  .fixture-id {
    font-weight: 700;
    color: var(--accent);
  }

  .fixture-name {
    color: var(--text);
  }

  .fixture-addr {
    color: var(--text-muted);
    font-family: monospace;
    font-size: 0.75rem;
  }

  .fixture-ref {
    color: var(--green);
    font-size: 0.75rem;
    font-family: monospace;
  }

  .fixture-channels {
    color: var(--text-muted);
    font-size: 0.75rem;
  }

  .fixture-actions {
    display: flex;
    gap: 0.3rem;
  }

  /* Bank cards */
  .new-bank-form {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    padding: 0.6rem 1rem;
    background: var(--bg-surface);
    border-radius: var(--radius);
    margin-bottom: 1rem;
    font-size: 0.8rem;
  }

  .new-bank-form label {
    font-weight: 600;
    color: var(--text-muted);
    white-space: nowrap;
  }

  .new-bank-form input {
    background: var(--bg-card);
    border: 1px solid var(--text-muted);
    border-radius: 4px;
    color: var(--text);
    padding: 0.35rem 0.6rem;
    font-size: 0.8rem;
    width: 120px;
  }

  .new-bank-form select {
    background: var(--bg-card);
    border: 1px solid var(--text-muted);
    border-radius: 4px;
    color: var(--text);
    padding: 0.35rem 0.6rem;
    font-size: 0.75rem;
    flex: 1;
    min-width: 0;
  }

  .bank-card {
    background: var(--bg-card);
    border-radius: var(--radius);
    padding: 0.8rem 1rem;
    margin-bottom: 0.8rem;
  }

  .bank-header {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    cursor: pointer;
    user-select: none;
  }

  .bank-collapse {
    font-size: 0.7rem;
    color: var(--text-muted);
    width: 1rem;
  }

  .bank-header h3 {
    font-size: 0.9rem;
    font-weight: 700;
    color: var(--accent);
    margin: 0;
  }

  .bank-path {
    font-size: 0.7rem;
    color: var(--text-muted);
    flex: 1;
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }

  .bank-count {
    font-size: 0.7rem;
    color: var(--text-muted);
  }

  .bank-actions-bar {
    display: flex;
    gap: 0.4rem;
    margin-top: 0.5rem;
    margin-bottom: 0.5rem;
    padding-left: 1.6rem;
  }

  .bank-fixtures {
    padding-left: 1rem;
  }

  /* Edit form */
  .edit-form {
    background: var(--bg-card);
    padding: 1rem;
    border-radius: var(--radius);
    margin-top: 1rem;
  }

  .edit-form h3 {
    font-size: 0.9rem;
    margin-bottom: 0.8rem;
  }

  .form-grid {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 0.4rem 0.8rem;
    align-items: center;
    margin-bottom: 0.8rem;
  }

  .form-grid label {
    font-size: 0.8rem;
    color: var(--text-muted);
    font-weight: 600;
  }

  .form-grid input {
    background: var(--bg-surface);
    border: 1px solid var(--text-muted);
    border-radius: 4px;
    color: var(--text);
    padding: 0.35rem 0.6rem;
    font-size: 0.8rem;
  }

  .channel-section {
    margin-top: 0.8rem;
  }

  .channel-row {
    display: flex;
    gap: 0.4rem;
    align-items: center;
    margin-bottom: 0.3rem;
  }

  .ch-name {
    flex: 1;
    background: var(--bg-surface);
    border: 1px solid var(--text-muted);
    border-radius: 4px;
    color: var(--text);
    padding: 0.3rem 0.5rem;
    font-size: 0.8rem;
  }

  .ch-offset {
    width: 60px;
    background: var(--bg-surface);
    border: 1px solid var(--text-muted);
    border-radius: 4px;
    color: var(--text);
    padding: 0.3rem 0.5rem;
    font-size: 0.8rem;
    text-align: center;
  }

  .form-actions {
    display: flex;
    gap: 0.5rem;
    margin-top: 1rem;
  }

  /* Buttons */
  .btn-sm {
    padding: 0.35rem 0.8rem;
    font-size: 0.75rem;
    border-radius: 4px;
    background: var(--accent);
    color: white;
  }

  .btn-sm.btn-muted {
    background: var(--bg-surface);
    color: var(--text-muted);
  }

  .btn-sm.btn-add, .btn-sm.btn-save {
    background: var(--green);
    color: #111;
  }

  .btn-xs {
    padding: 0.2rem 0.5rem;
    font-size: 0.7rem;
    border-radius: 4px;
    background: var(--bg-card);
    color: var(--text);
  }

  .btn-xs.btn-danger {
    background: var(--red);
    color: white;
  }

  .btn-xs.btn-muted {
    background: var(--bg-surface);
    color: var(--text-muted);
  }

  .empty-msg {
    color: var(--text-muted);
    font-size: 0.85rem;
    padding: 2rem;
    text-align: center;
  }
</style>
