<script lang="ts">
  import { onMount } from 'svelte';
  import {
    editorStatus,
    editorOpen,
    editorClose,
    editorSave,
    editorSaveAs,
    editorGetFixtures,
    editorAddFixture,
    editorUpdateFixture,
    editorDeleteFixture,
    editorFixturesSort,
    editorGetBanks,
    editorBankAddFixture,
    editorBankUpdateFixture,
    editorBankDeleteFixture,
    editorBankSave,
    editorGetBankDirs,
    editorCreateBank,
    editorBrowse,
    editorGetHardware,
    editorUpdateHardware,
    editorGetScenes,
    editorAddScene,
    editorUpdateScene,
    editorDeleteScene,
    type EditorStatus,
    type EditorFixture,
    type EditorBank,
    type BankFixture,
    type BrowseResult,
    type HardwareConfig,
    type EditorScene,
    type SceneValue,
    type SceneStep,
    type Channel,
  } from '../lib/api';

  /* ---- Data state ---- */
  let status: EditorStatus = $state({ project_loaded: false, project_path: '', dirty: false, fixture_count: 0, bank_count: 0 });
  let fixtures: EditorFixture[] = $state([]);
  let banks: EditorBank[] = $state([]);
  let scenes: EditorScene[] = $state([]);
  let bankDirs: string[] = $state([]);
  let hwConfig: HardwareConfig = $state({ midi_device: '', midi_mode: '', dmx_device: '', dmx_backend: '', dmx_refresh_hz: 0 });

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
  let editScene: EditorScene | null = $state(null);
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
      scenes = await editorGetScenes();
      hwConfig = await editorGetHardware();
    } else {
      fixtures = [];
      scenes = [];
      hwConfig = { midi_device: '', midi_mode: '', dmx_device: '', dmx_backend: '', dmx_refresh_hz: 0 };
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
      const sc = scenes[sel.itemIdx];
      if (sc) {
        editScene = {
          ...sc,
          values: sc.values.map(v => ({ ...v })),
          steps: sc.steps.map(s => ({ ...s, values: s.values.map(v => ({ ...v })) })),
        };
        editIsNew = false;
      }
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

  /* ---- Scene editing ---- */
  function startAddScene() {
    editScene = { id: '', name: '', type: 'static', trigger_mode: 'gate', channel: 1, note: 60, enabled: true, loop: false, values: [], steps: [] };
    editFixture = null;
    editBankFixture = null;
    editIsNew = true;
    editDirty = false;
    selection = { kind: 'scene', itemIdx: -1 };
  }

  async function saveEditScene() {
    if (!editScene) return;
    const data = { ...editScene };
    delete (data as any).index;
    if (editIsNew) {
      await editorAddScene(data);
    } else if (selection?.kind === 'scene') {
      await editorUpdateScene(selection.itemIdx, data);
    }
    editDirty = false;
    selection = null; editScene = null;
    await refresh();
  }

  async function deleteEditScene() {
    if (!selection || selection.kind !== 'scene' || selection.itemIdx < 0) return;
    await editorDeleteScene(selection.itemIdx);
    editDirty = false;
    selection = null; editScene = null;
    await refresh();
  }

  function addSceneValue() {
    if (!editScene) return;
    editScene.values = [...editScene.values, { target: '', value: 0 }];
    markDirty();
  }

  function removeSceneValue(i: number) {
    if (!editScene) return;
    editScene.values = editScene.values.filter((_, idx) => idx !== i);
    markDirty();
  }

  function addSceneStep() {
    if (!editScene) return;
    editScene.steps = [...editScene.steps, { duration_ms: 1000, transition: 'hold', values: [] }];
    markDirty();
  }

  function removeSceneStep(i: number) {
    if (!editScene) return;
    editScene.steps = editScene.steps.filter((_, idx) => idx !== i);
    markDirty();
  }

  function addStepValue(stepIdx: number) {
    if (!editScene) return;
    editScene.steps[stepIdx].values = [...editScene.steps[stepIdx].values, { target: '', value: 0 }];
    markDirty();
  }

  function removeStepValue(stepIdx: number, valIdx: number) {
    if (!editScene) return;
    editScene.steps[stepIdx].values = editScene.steps[stepIdx].values.filter((_, idx) => idx !== valIdx);
    markDirty();
  }

  /* ---- Hardware editing ---- */
  let hwDirty = $state(false);

  async function saveHardware() {
    await editorUpdateHardware(hwConfig);
    hwDirty = false;
    await refresh();
  }

  function markHwDirty() { hwDirty = true; }

  /* ---- Save As ---- */
  let showSaveAs = $state(false);
  let saveAsPath = $state('');

  async function handleSaveAs() {
    if (!saveAsPath) return;
    await editorSaveAs(saveAsPath);
    showSaveAs = false;
    saveAsPath = '';
    await refresh();
  }

  /* ---- Fixtures sort ---- */
  async function handleFixturesSort() {
    await editorFixturesSort();
    await refresh();
  }

  /* ---- Fixture/channel target helpers ---- */
  function getFixtureTargets(): { fixture: string; channel: string; addr: number }[] {
    const targets: { fixture: string; channel: string; addr: number }[] = [];
    const sorted = [...fixtures].sort((a, b) => a.start_address - b.start_address);
    for (const fix of sorted) {
      for (const ch of fix.channels) {
        targets.push({ fixture: fix.id, channel: ch.name, addr: fix.start_address });
      }
    }
    return targets;
  }

  function validateTarget(target: string): 'valid' | 'warn' {
    if (!target || !target.includes('.')) return 'warn';
    const [fixId, chName] = target.split('.', 2);
    const fix = fixtures.find(f => f.id === fixId);
    if (!fix) return 'warn';
    if (!fix.channels.some(c => c.name === chName)) return 'warn';
    return 'valid';
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
      <button class="btn-sm btn-muted" onclick={() => showSaveAs = true}>Save As</button>
      <button class="btn-sm btn-muted" onclick={handleClose}>Close</button>
    </div>
  {:else}
    <input class="open-input" type="text" placeholder="Paste path or browse..." bind:value={openPath} onkeydown={(e) => { if (e.key === 'Enter') handleOpen(); }} />
    <button class="btn-sm" onclick={handleOpen}>Open</button>
    <button class="btn-sm btn-muted" onclick={openBrowser}>Browse</button>
  {/if}
</header>

{#if showBrowser && browseResult}
<div class="modal-overlay" role="dialog" tabindex="-1" onkeydown={(e) => { if (e.key === 'Escape') showBrowser = false; }}>
  <button class="modal-backdrop" aria-label="Close" onclick={() => showBrowser = false} tabindex="-1"></button>
  <div class="modal">
    <header class="modal-header">
      <h3>Open Project</h3>
      <button class="modal-close" onclick={() => showBrowser = false}>X</button>
    </header>
    <div class="browser-path">{browseResult.path}</div>
    <div class="browser-list">
      <button class="browser-entry dir" onclick={browseUp}>..</button>
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
  </div>
</div>
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
            <button class="btn-xs btn-add" onclick={startAddProjectFixture}>+ Add</button>
            <button class="btn-xs" onclick={handleFixturesSort}>Sort</button>
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
          <div class="hw-form">
            <div class="form-grid">
              <label>MIDI Device</label>
              <input type="text" bind:value={hwConfig.midi_device} oninput={markHwDirty} />
              <label>MIDI Mode</label>
              <select bind:value={hwConfig.midi_mode} onchange={markHwDirty}>
                <option value="">none</option>
                <option value="open-existing">open-existing</option>
                <option value="create-virtual">create-virtual</option>
              </select>
              <label>DMX Device</label>
              <input type="text" bind:value={hwConfig.dmx_device} oninput={markHwDirty} />
              <label>DMX Backend</label>
              <select bind:value={hwConfig.dmx_backend} onchange={markHwDirty}>
                <option value="">none</option>
                <option value="open">open</option>
                <option value="dummy">dummy</option>
              </select>
              <label>Refresh Hz</label>
              <input type="number" min="0" max="60" bind:value={hwConfig.dmx_refresh_hz} oninput={markHwDirty} />
            </div>
            {#if hwDirty}
              <button class="btn-sm btn-save" onclick={saveHardware}>Save Hardware</button>
            {/if}
          </div>
        {:else}
          <p class="empty-msg">Open a project first.</p>
        {/if}

      {:else if selectionTab === 'scenes'}
        {#if status.project_loaded}
          <div class="sel-toolbar">
            <button class="btn-xs btn-add" onclick={startAddScene}>+ Add Scene</button>
          </div>
          <div class="pad-grid">
            {#each scenes as scene, i (i)}
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
            <p class="empty-msg">No scenes yet.</p>
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
            {editIsNew ? 'New Scene' : `Scene: ${editScene?.id ?? ''}`}
          {/if}
        </span>
        {#if editDirty}<span class="dirty-badge">modified</span>{/if}
        <div class="edit-actions">
          {#if selection.kind === 'scene'}
            <button class="btn-sm btn-save" onclick={saveEditScene}>Save</button>
            {#if !editIsNew}
              <button class="btn-sm btn-danger" onclick={deleteEditScene}>Delete</button>
            {/if}
          {:else}
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

        <!-- Scene editor -->
        {:else if selection.kind === 'scene' && editScene}
          <div class="form-grid">
            <label>ID</label>
            <input type="text" bind:value={editScene.id} oninput={markDirty} />
            <label>Name</label>
            <input type="text" bind:value={editScene.name} oninput={markDirty} />
            <label>Type</label>
            <select bind:value={editScene.type} onchange={markDirty}>
              <option value="static">static</option>
              <option value="sequence">sequence</option>
            </select>
            <label>Trigger</label>
            <select bind:value={editScene.trigger_mode} onchange={markDirty}>
              <option value="gate">gate</option>
              <option value="toggle">toggle</option>
            </select>
            <label>Channel</label>
            <input type="number" min="1" max="16" bind:value={editScene.channel} oninput={markDirty} />
            <label>Note</label>
            <input type="number" min="0" max="127" bind:value={editScene.note} oninput={markDirty} />
            <label>Enabled</label>
            <input type="checkbox" bind:checked={editScene.enabled} onchange={markDirty} />
            {#if editScene.type === 'sequence'}
              <label>Loop</label>
              <input type="checkbox" bind:checked={editScene.loop} onchange={markDirty} />
            {/if}
          </div>

          {#if editScene.type === 'static'}
            <div class="channel-section">
              <div class="ch-header">
                <span>Values ({editScene.values.length})</span>
                <button class="btn-xs btn-add" onclick={addSceneValue}>+</button>
              </div>
              {#each editScene.values as val, i}
                <div class="channel-row">
                  <input type="text" placeholder="fixture.channel" bind:value={val.target} oninput={markDirty} class="ch-name"
                    list="target-opts" />
                  <input type="number" min="0" max="255" bind:value={val.value} oninput={markDirty} class="ch-offset" />
                  {#if validateTarget(val.target) === 'warn' && val.target}
                    <span class="warn-icon" title="Target not found">!</span>
                  {/if}
                  <button class="btn-xs btn-danger" onclick={() => removeSceneValue(i)}>x</button>
                </div>
              {/each}
            </div>
          {:else}
            <div class="channel-section">
              <div class="ch-header">
                <span>Steps ({editScene.steps.length})</span>
                <button class="btn-xs btn-add" onclick={addSceneStep}>+</button>
              </div>
              {#each editScene.steps as step, si}
                <div class="step-card">
                  <div class="step-header">
                    <span class="step-label">Step {si + 1}</span>
                    <input type="number" min="0" placeholder="ms" bind:value={step.duration_ms} oninput={markDirty} class="ch-offset" />
                    <select bind:value={step.transition} onchange={markDirty} class="input-sm">
                      <option value="hold">hold</option>
                      <option value="linear">linear</option>
                    </select>
                    <button class="btn-xs btn-danger" onclick={() => removeSceneStep(si)}>x</button>
                  </div>
                  {#each step.values as val, vi}
                    <div class="channel-row">
                      <input type="text" placeholder="fixture.channel" bind:value={val.target} oninput={markDirty} class="ch-name"
                        list="target-opts" />
                      <input type="number" min="0" max="255" bind:value={val.value} oninput={markDirty} class="ch-offset" />
                      {#if validateTarget(val.target) === 'warn' && val.target}
                        <span class="warn-icon" title="Target not found">!</span>
                      {/if}
                      <button class="btn-xs btn-danger" onclick={() => removeStepValue(si, vi)}>x</button>
                    </div>
                  {/each}
                  <button class="btn-xs btn-add" onclick={() => addStepValue(si)}>+ Value</button>
                </div>
              {/each}
            </div>
          {/if}

          <datalist id="target-opts">
            {#each getFixtureTargets() as t}
              <option value="{t.fixture}.{t.channel}">{t.fixture}.{t.channel} (@{t.addr})</option>
            {/each}
          </datalist>
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

{#if showSaveAs}
  <div class="modal-overlay" onclick={() => showSaveAs = false} role="presentation">
    <div class="modal" onclick={(e) => e.stopPropagation()} onkeydown={(e) => { if (e.key === 'Enter') handleSaveAs(); }} role="dialog" aria-modal="true" tabindex="-1">
      <p>Save project as:</p>
      <input type="text" class="open-input" placeholder="Enter path..." bind:value={saveAsPath} />
      <div class="modal-actions">
        <button class="btn-sm btn-save" onclick={handleSaveAs}>Save</button>
        <button class="btn-sm btn-muted" onclick={() => showSaveAs = false}>Cancel</button>
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
    border: 1px solid rgba(255,255,255,0.04);
    border-radius: var(--radius);
    margin-bottom: 0.8rem;
    box-shadow: inset 0 1px 3px rgba(0,0,0,0.4);
  }
  .project-path {
    font-size: 0.75rem;
    color: var(--text-muted);
    font-family: monospace;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    max-width: 400px;
  }
  .dirty-badge {
    font-size: 0.6rem;
    font-weight: 700;
    color: var(--yellow);
    text-transform: uppercase;
    letter-spacing: 0.05em;
  }
  .project-actions { margin-left: auto; display: flex; gap: 0.4rem; }
  .open-input {
    flex: 1;
    background: var(--bg);
    border: 1px solid rgba(255,255,255,0.08);
    border-radius: var(--radius);
    color: var(--text);
    padding: 0.35rem 0.6rem;
    font-size: 0.75rem;
    box-shadow: inset 0 1px 3px rgba(0,0,0,0.4);
  }
  .open-input:focus {
    outline: none;
    border-color: rgba(233, 69, 96, 0.4);
    box-shadow: inset 0 1px 3px rgba(0,0,0,0.4), 0 0 4px var(--accent-glow);
  }
  .open-input::placeholder { color: var(--text-muted); }

  /* File browser modal */
  .modal-overlay {
    position: fixed;
    inset: 0;
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 100;
  }
  .modal-backdrop {
    position: absolute;
    inset: 0;
    background: rgba(0,0,0,0.7);
    backdrop-filter: blur(2px);
    border: none;
    cursor: default;
  }
  .modal {
    position: relative;
    background: var(--bg-surface);
    border: 1px solid rgba(255,255,255,0.06);
    border-radius: var(--radius);
    width: 90%;
    max-width: 500px;
    max-height: 70vh;
    display: flex;
    flex-direction: column;
    overflow: hidden;
    box-shadow: 0 8px 32px rgba(0,0,0,0.6), 0 0 1px rgba(233, 69, 96, 0.2);
  }
  .modal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0.8rem 1rem;
    border-bottom: 1px solid rgba(255,255,255,0.04);
  }
  .modal-header h3 {
    margin: 0;
    font-size: 0.85rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.05em;
  }
  .modal-close {
    background: none;
    border: none;
    color: var(--text-muted);
    font-size: 0.9rem;
    padding: 0.2rem 0.5rem;
  }
  .modal-close:hover { color: var(--accent); box-shadow: none; }
  .browser-path {
    padding: 0.5rem 1rem;
    font-size: 0.7rem;
    color: var(--text-muted);
    font-family: monospace;
    border-bottom: 1px solid rgba(255,255,255,0.04);
    word-break: break-all;
    background: var(--bg);
  }
  .browser-list {
    overflow-y: auto;
    flex: 1;
    padding: 0.4rem;
    display: flex;
    flex-direction: column;
    gap: 0.1rem;
  }
  .browser-entry {
    text-align: left;
    padding: 0.35rem 0.8rem;
    font-size: 0.8rem;
    border-radius: 4px;
    background: none;
    color: var(--text);
    font-weight: 400;
    border: none;
  }
  .browser-entry:hover { background: var(--bg-card); box-shadow: none; }
  .browser-entry.dir { color: var(--accent); font-weight: 600; }

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
    border: 1px solid rgba(255,255,255,0.04);
    border-radius: var(--radius);
    overflow: hidden;
    min-width: 0;
    box-shadow: inset 0 1px 3px rgba(0,0,0,0.4);
  }

  .sel-tabs {
    display: flex;
    border-bottom: 1px solid rgba(255,255,255,0.04);
    flex-shrink: 0;
  }
  .sel-tab {
    flex: 1;
    padding: 0.5rem 0.3rem;
    font-size: 0.65rem;
    font-weight: 700;
    background: none;
    color: var(--text-muted);
    border: none;
    border-radius: 0;
    border-bottom: 2px solid transparent;
    text-transform: uppercase;
    letter-spacing: 0.06em;
  }
  .sel-tab:active { transform: none; }
  .sel-tab:hover { color: var(--text); box-shadow: none; }
  .sel-tab.active {
    color: var(--accent);
    border-bottom-color: var(--accent);
    box-shadow: 0 2px 6px var(--accent-glow);
  }

  .sel-content {
    flex: 1;
    overflow-y: auto;
    padding: 0.6rem;
  }

  .sel-toolbar { margin-bottom: 0.5rem; }

  /* Pad grid */
  .pad-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(70px, 90px));
    gap: 0.35rem;
    margin-top: 0.4rem;
  }

  .pad-btn {
    aspect-ratio: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 0.15rem;
    background: var(--bg-pad);
    border: 1.5px solid rgba(255,255,255,0.06);
    border-radius: var(--pad-radius);
    color: var(--text);
    font-size: 0.65rem;
    font-weight: 600;
    padding: 0.3rem;
    user-select: none;
    word-break: break-all;
    text-align: center;
    min-width: 0;
    box-shadow:
      inset 0 2px 4px rgba(0,0,0,0.4),
      inset 0 -1px 2px rgba(255,255,255,0.03);
    transition: box-shadow 0.12s, background 0.12s, transform 0.08s, border-color 0.12s;
  }
  .pad-btn:hover {
    border-color: rgba(233, 69, 96, 0.4);
    box-shadow:
      inset 0 2px 4px rgba(0,0,0,0.4),
      0 0 8px var(--accent-glow);
  }
  .pad-btn:active {
    transform: translateY(1px);
    box-shadow: inset 0 3px 6px rgba(0,0,0,0.6);
  }
  .pad-btn.selected {
    background: var(--accent);
    color: white;
    border-color: var(--accent);
    box-shadow:
      0 0 14px var(--accent-glow),
      0 0 4px var(--accent-glow),
      inset 0 1px 2px rgba(255,255,255,0.15);
  }
  .pad-id { font-size: 0.65rem; font-weight: 700; letter-spacing: 0.02em; }
  .pad-sub { font-size: 0.5rem; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.04em; }
  .pad-btn.selected .pad-sub { color: rgba(255,255,255,0.7); }

  /* Bank groups */
  .bank-group { margin-bottom: 0.6rem; }
  .bank-group-header {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    padding: 0.35rem 0.5rem;
    background: var(--bg-card);
    border: 1px solid rgba(255,255,255,0.04);
    border-radius: var(--radius);
    cursor: pointer;
    user-select: none;
    font-size: 0.7rem;
  }
  .collapse-icon { width: 0.8rem; color: var(--text-muted); font-size: 0.6rem; }
  .bank-id { font-weight: 700; color: var(--accent); }
  .bank-count { color: var(--text-muted); margin-left: auto; font-size: 0.6rem; letter-spacing: 0.03em; }

  .new-bank-row {
    display: flex;
    gap: 0.4rem;
    align-items: center;
    margin-bottom: 0.6rem;
  }
  .input-sm {
    background: var(--bg);
    border: 1px solid rgba(255,255,255,0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.25rem 0.5rem;
    font-size: 0.7rem;
    min-width: 0;
    box-shadow: inset 0 1px 2px rgba(0,0,0,0.3);
  }
  .new-bank-row select { flex: 1; }
  .new-bank-row input { width: 90px; }

  /* Edit card */
  .edit-card {
    flex: 2;
    display: flex;
    flex-direction: column;
    background: var(--bg-card);
    border: 1px solid rgba(255,255,255,0.04);
    border-radius: var(--radius);
    overflow: hidden;
    min-width: 0;
    box-shadow: inset 0 1px 3px rgba(0,0,0,0.4);
  }

  .edit-header {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    padding: 0.5rem 0.8rem;
    border-bottom: 1px solid rgba(255,255,255,0.04);
    flex-shrink: 0;
  }
  .edit-title {
    font-size: 0.75rem;
    font-weight: 700;
    color: var(--text);
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
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
  .form-grid label {
    font-size: 0.7rem;
    color: var(--text-muted);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .form-grid input, .form-grid select {
    background: var(--bg);
    border: 1px solid rgba(255,255,255,0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.3rem 0.5rem;
    font-size: 0.75rem;
    box-shadow: inset 0 1px 2px rgba(0,0,0,0.3);
  }
  .form-grid input:focus, .form-grid select:focus {
    outline: none;
    border-color: rgba(233, 69, 96, 0.4);
    box-shadow: inset 0 1px 2px rgba(0,0,0,0.3), 0 0 4px var(--accent-glow);
  }
  /* Channels */
  .channel-section { margin-top: 0.6rem; }
  .ch-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    margin-bottom: 0.3rem;
    font-size: 0.7rem;
    color: var(--text-muted);
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .channel-row { display: flex; gap: 0.3rem; align-items: center; margin-bottom: 0.2rem; }
  .ch-name {
    flex: 1;
    background: var(--bg);
    border: 1px solid rgba(255,255,255,0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.25rem 0.4rem;
    font-size: 0.75rem;
    box-shadow: inset 0 1px 2px rgba(0,0,0,0.3);
  }
  .ch-offset {
    width: 50px;
    background: var(--bg);
    border: 1px solid rgba(255,255,255,0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.25rem 0.4rem;
    font-size: 0.75rem;
    text-align: center;
    box-shadow: inset 0 1px 2px rgba(0,0,0,0.3);
  }


  /* Buttons */
  .btn-sm {
    padding: 0.3rem 0.7rem;
    font-size: 0.65rem;
    font-weight: 700;
    border-radius: 4px;
    background: var(--accent);
    color: white;
    border: 1px solid rgba(233, 69, 96, 0.3);
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .btn-sm:hover { box-shadow: 0 0 8px var(--accent-glow); }
  .btn-sm.btn-muted {
    background: var(--bg-pad);
    color: var(--text-muted);
    border-color: rgba(255,255,255,0.08);
  }
  .btn-sm.btn-muted:hover { box-shadow: 0 0 6px rgba(255,255,255,0.05); }
  .btn-sm.btn-save {
    background: var(--green);
    color: #111;
    border-color: rgba(78, 205, 196, 0.3);
  }
  .btn-sm.btn-save:hover { box-shadow: 0 0 8px var(--green-glow); }
  .btn-sm.btn-danger {
    background: var(--red);
    color: white;
    border-color: rgba(233, 69, 96, 0.3);
  }
  .btn-sm.btn-danger:hover { box-shadow: 0 0 8px var(--accent-glow); }

  .btn-xs {
    padding: 0.2rem 0.5rem;
    font-size: 0.55rem;
    font-weight: 700;
    border-radius: 4px;
    background: var(--bg-pad);
    color: var(--text);
    border: 1px solid rgba(255,255,255,0.08);
    text-transform: uppercase;
    letter-spacing: 0.03em;
  }
  .btn-xs:hover { box-shadow: 0 0 6px rgba(255,255,255,0.05); }
  .btn-xs.btn-danger {
    background: var(--red);
    color: white;
    border-color: rgba(233, 69, 96, 0.3);
  }
  .btn-xs.btn-add {
    background: var(--green);
    color: #111;
    border-color: rgba(78, 205, 196, 0.3);
  }
  .btn-xs.btn-add:hover { box-shadow: 0 0 6px var(--green-glow); }
  .empty-msg { color: var(--text-muted); font-size: 0.75rem; padding: 1.5rem; text-align: center; letter-spacing: 0.02em; }

  /* Discard modal */
  .modal-overlay {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.7);
    backdrop-filter: blur(2px);
    display: flex;
    align-items: center;
    justify-content: center;
    z-index: 1000;
  }
  .modal {
    background: var(--bg-card);
    border: 1px solid rgba(255,255,255,0.06);
    border-radius: var(--radius);
    padding: 1.5rem;
    max-width: 320px;
    width: 90%;
    box-shadow: 0 8px 32px rgba(0,0,0,0.6), 0 0 1px rgba(233, 69, 96, 0.2);
  }
  .modal p { font-size: 0.8rem; margin-bottom: 1rem; color: var(--text); }
  .modal input.open-input { width: 100%; margin-bottom: 0.75rem; }
  .modal-actions { display: flex; gap: 0.5rem; justify-content: flex-end; }

  /* Hardware form */
  .hw-form { padding: 0.75rem; }
  .hw-form .form-grid { gap: 0.5rem 0.75rem; }
  .hw-form .btn-save { margin-top: 0.75rem; }

  /* Scene step cards */
  .step-card {
    background: var(--bg-surface);
    border: 1px solid rgba(255,255,255,0.04);
    border-radius: var(--radius);
    padding: 0.5rem;
    margin-bottom: 0.5rem;
  }
  .step-header {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    margin-bottom: 0.35rem;
  }
  .step-label {
    font-size: 0.7rem;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    color: var(--text-muted);
    min-width: 3rem;
  }
  .input-sm {
    font-size: 0.7rem;
    padding: 2px 4px;
    width: auto;
    min-width: 4rem;
  }
  .warn-icon {
    color: var(--yellow);
    font-weight: bold;
    font-size: 0.85rem;
    line-height: 1;
  }
</style>
