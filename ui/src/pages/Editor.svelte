<script lang="ts">
  import { onMount } from 'svelte';
  import FileBrowser from '../components/FileBrowser.svelte';
  import PadGrid from '../components/PadGrid.svelte';
  import type { PadItem } from '../components/PadGrid.svelte';
  import HardwareForm from '../components/HardwareForm.svelte';
  import FixtureForm from '../components/FixtureForm.svelte';
  import SceneForm from '../components/SceneForm.svelte';
  import { validateId } from '../lib/validate';
  import { showError, showWarning, showSuccess } from '../lib/toast';
  import { validateProject, type Problem } from '../lib/project-validate';
  import Modal from '../components/Modal.svelte';
  import ConfirmModal from '../components/ConfirmModal.svelte';
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
    type HardwareConfig,
    type EditorScene,
    type SceneValue,
    type SceneStep,
    type Channel,
    ApiError,
  } from '../lib/api';

  /* ---- Data state ---- */
  let status: EditorStatus = $state({ project_loaded: false, project_path: '', dirty: false, fixture_count: 0, bank_count: 0 });
  let fixtures: EditorFixture[] = $state([]);
  let banks: EditorBank[] = $state([]);
  let scenes: EditorScene[] = $state([]);
  let bankDirs: string[] = $state([]);
  let hwConfig: HardwareConfig = $state({ midi_device: '', midi_mode: '', dmx_device: '', dmx_backend: '', dmx_refresh_hz: 0 });
  let hwDirty = $state(false);

  /* ---- Selection / navigation ---- */
  type Selection =
    | { kind: 'project_fixture'; index: number }
    | { kind: 'bank_fixture'; bankIdx: number; index: number }
    | { kind: 'scene'; index: number }
    | null;

  let selectionTab: 'banks' | 'fixtures' | 'hardware' | 'scenes' = $state('banks');
  let selection: Selection = $state(null);
  let collapsedBanks: Set<number> = $state(new Set());

  /* ---- Problems modal ---- */
  let showProblemsModal = $state(false);

  /* ---- Edit card state ---- */
  let editDirty = $state(false);
  let showDiscardModal = $state(false);
  let showDeleteModal = $state(false);
  let pendingDelete: (() => Promise<void>) | null = $state(null);
  let pendingSelection: Selection = $state(null);
  let pendingAction: (() => void) | null = $state(null);

  let editFixture: EditorFixture | null = $state(null);
  let editBankFixture: BankFixture | null = $state(null);
  let editScene: EditorScene | null = $state(null);
  let editIsNew = $state(false);

  let editIdValid = $derived.by(() => {
    const idx = editIsNew ? undefined : selection?.index;
    if (editScene) {
      return validateId(editScene.id, scenes.map(s => s.id), idx) === 'valid';
    }
    if (editFixture) {
      return validateId(editFixture.id, fixtures.map(f => f.id), idx) === 'valid';
    }
    if (editBankFixture && selection?.kind === 'bank_fixture') {
      const bank = banks[selection.bankIdx];
      const ids = bank ? bank.fixtures.map(f => f.id) : [];
      return validateId(editBankFixture.id, ids, idx) === 'valid';
    }
    return true;
  });

  /* ---- Project validation ---- */
  let problems: Problem[] = $derived(validateProject(fixtures, scenes, banks));
  let fixtureProblems = $derived(problems.filter(p => p.tab === 'fixtures'));
  let sceneProblems = $derived(problems.filter(p => p.tab === 'scenes'));
  let warnFixtureIndices = $derived(new Set(fixtureProblems.map(p => p.index)));
  let warnSceneIndices = $derived(new Set(sceneProblems.map(p => p.index)));

  /* ---- New bank form ---- */
  let newBankId = $state('');
  let newBankDir = $state('');

  /* ---- File browser ---- */
  let browserMode: 'open' | 'save' | null = $state(null);

  /* ---- Data loading ---- */
  let loading = $state(false);

  async function refresh() {
    loading = true;
    try {
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
    } finally {
      loading = false;
    }
  }

  onMount(() => { refresh(); });

  function handleBeforeUnload(ev: BeforeUnloadEvent) {
    ev.preventDefault();
  }

  $effect(() => {
    if (editDirty || status.dirty || hwDirty) {
      window.addEventListener('beforeunload', handleBeforeUnload);
    } else {
      window.removeEventListener('beforeunload', handleBeforeUnload);
    }
    return () => window.removeEventListener('beforeunload', handleBeforeUnload);
  });

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
      const f = fixtures.find(fx => fx.index === sel.index);
      if (f) {
        editFixture = { ...f, channels: f.channels.map(c => ({ ...c })) };
        editBankFixture = null;
        editIsNew = false;
      }
    } else if (sel.kind === 'bank_fixture') {
      const bank = banks.find(b => b.index === sel.bankIdx);
      const f = bank?.fixtures.find(fx => fx.index === sel.index);
      if (f) {
        editBankFixture = { ...f, channels: f.channels.map(c => ({ ...c })) };
        editFixture = null;
        editIsNew = false;
      }
    } else if (sel.kind === 'scene') {
      const sc = scenes[sel.index];
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
    if (pendingAction) {
      const action = pendingAction;
      pendingAction = null;
      pendingSelection = null;
      action();
    } else if (pendingSelection !== undefined) {
      applySelection(pendingSelection);
    }
  }

  function cancelDiscard() {
    showDiscardModal = false;
    pendingSelection = null;
    pendingAction = null;
  }

  function closeEdit() {
    trySelect(null);
  }

  function markDirty() {
    editDirty = true;
  }

  /* ---- Project lifecycle ---- */
  function handleClose() {
    if (status.dirty || editDirty) {
      pendingAction = doClose;
      showDiscardModal = true;
      return;
    }
    doClose();
  }

  async function doClose() {
    await editorClose();
    selection = null; editFixture = null; editBankFixture = null; editScene = null; editDirty = false;
    await refresh();
  }

  async function handleSave() {
    try {
      await editorSave();
      await refresh();
    } catch (e: any) { showError(e.message || 'Save failed'); }
  }

  /* ---- File browser callbacks ---- */
  async function handleBrowserConfirm(fullPath: string) {
    if (browserMode === 'open' && (status.dirty || editDirty)) {
      browserMode = null;
      pendingAction = async () => {
        try {
          await editorOpen(fullPath);
          selection = null; editFixture = null; editBankFixture = null; editScene = null; editDirty = false;
          await refresh();
          notifyOpenResult();
        } catch (e: any) { showOpenError(e); }
      };
      showDiscardModal = true;
      return;
    }
    try {
      if (browserMode === 'open') {
        await editorOpen(fullPath);
        browserMode = null;
        await refresh();
        notifyOpenResult();
      } else if (browserMode === 'save') {
        await editorSaveAs(fullPath);
        browserMode = null;
        await refresh();
      }
    } catch (e: any) {
      if (browserMode === 'open' || browserMode === null) showOpenError(e);
      else showError(e.message || 'Save failed');
    }
  }

  function notifyOpenResult() {
    if (problems.length > 0) {
      showWarning(`Opened with ${problems.length} problem${problems.length > 1 ? 's' : ''}`, () => { showProblemsModal = true; });
    } else if (status.project_loaded) {
      showSuccess('Project opened');
    }
  }

  let openErrorDetail = $state('');
  let showOpenErrorModal = $state(false);

  function showOpenError(e: unknown) {
    if (e instanceof ApiError) {
      const line = e.detail.line as number | undefined;
      const col = e.detail.column as number | undefined;
      const loc = line ? `Line ${line}, column ${col || 1}` : '';
      openErrorDetail = [e.message, loc].filter(Boolean).join('\n');
      showError('Failed to open project', () => { showOpenErrorModal = true; });
    } else {
      showError((e as any)?.message || 'Open failed');
    }
  }

  function getProjectBasename(): string {
    if (!status.project_path) return '';
    const normalized = status.project_path.replace(/\\/g, '/');
    const parts = normalized.split('/');
    return parts[parts.length - 1] || '';
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
    selection = { kind: 'project_fixture', index: -1 };
  }

  function startAddBankFixture(bankIdx: number) {
    editBankFixture = { id: '', name: '', channel_count: 0, channels: [] };
    editFixture = null;
    editIsNew = true;
    editDirty = false;
    selection = { kind: 'bank_fixture', bankIdx, index: -1 };
  }

  /* ---- Save edit card ---- */
  async function saveEditFixture() {
    if (!editFixture) return;
    const data = { ...editFixture };
    delete (data as any).index;
    try {
      if (editIsNew) {
        await editorAddFixture(data);
      } else if (selection?.kind === 'project_fixture') {
        await editorUpdateFixture(selection.index, data);
      }
      editDirty = false;
      editIsNew = false;
      await refresh();
    } catch (e: any) { showError(e.message || 'Save failed'); }
  }

  async function saveEditBankFixture() {
    if (!editBankFixture || !selection || selection.kind !== 'bank_fixture') return;
    const data = { ...editBankFixture };
    delete (data as any).index;
    try {
      if (editIsNew) {
        await editorBankAddFixture(selection.bankIdx, data);
      } else {
        await editorBankUpdateFixture(selection.bankIdx, selection.index, data);
      }
      editDirty = false;
      editIsNew = false;
      await refresh();
    } catch (e: any) { showError(e.message || 'Save failed'); }
  }

  async function deleteEditFixture() {
    if (!selection) return;
    try {
      if (selection.kind === 'project_fixture' && selection.index >= 0) {
        await editorDeleteFixture(selection.index);
      } else if (selection.kind === 'bank_fixture' && selection.index >= 0) {
        await editorBankDeleteFixture(selection.bankIdx, selection.index);
      }
      editDirty = false;
      selection = null; editFixture = null; editBankFixture = null;
      await refresh();
    } catch (e: any) { showError(e.message || 'Delete failed'); }
  }

  /* ---- Scene editing ---- */
  function startAddScene() {
    editScene = { id: '', name: '', type: 'static', trigger_mode: 'gate', channel: 1, note: 60, enabled: true, loop: false, values: [], steps: [] };
    editFixture = null;
    editBankFixture = null;
    editIsNew = true;
    editDirty = false;
    selection = { kind: 'scene', index: -1 };
  }

  async function saveEditScene() {
    if (!editScene) return;
    const data = { ...editScene };
    delete (data as any).index;
    try {
      if (editIsNew) {
        await editorAddScene(data);
      } else if (selection?.kind === 'scene') {
        await editorUpdateScene(selection.index, data);
      }
      editDirty = false;
      editIsNew = false;
      await refresh();
    } catch (e: any) { showError(e.message || 'Save failed'); }
  }

  async function deleteEditScene() {
    if (!selection || selection.kind !== 'scene' || selection.index < 0) return;
    try {
      await editorDeleteScene(selection.index);
      editDirty = false;
      selection = null; editScene = null;
      await refresh();
    } catch (e: any) { showError(e.message || 'Delete failed'); }
  }

  function confirmDeleteScene() { pendingDelete = deleteEditScene; showDeleteModal = true; }
  function confirmDeleteFixture() { pendingDelete = deleteEditFixture; showDeleteModal = true; }
  async function executeDelete() {
    showDeleteModal = false;
    if (pendingDelete) { await pendingDelete(); pendingDelete = null; }
  }
  function cancelDelete() { showDeleteModal = false; pendingDelete = null; }

  /* ---- Hardware editing ---- */
  async function saveHardware() {
    try {
      await editorUpdateHardware(hwConfig);
      hwDirty = false;
      await refresh();
    } catch (e: any) { showError(e.message || 'Save hardware failed'); }
  }

  /* ---- Fixtures sort ---- */
  async function handleFixturesSort() {
    try {
      await editorFixturesSort();
      await refresh();
    } catch (e: any) { showError(e.message || 'Sort failed'); }
  }



  /* ---- Helpers ---- */
  function isSelected(sel: Selection): boolean {
    if (!selection || !sel) return false;
    if (selection.kind !== sel.kind) return false;
    if (selection.kind === 'bank_fixture' && sel.kind === 'bank_fixture') {
      return selection.bankIdx === sel.bankIdx && selection.index === sel.index;
    }
    return selection.index === sel.index;
  }
</script>

<!-- Project bar -->
<header class="project-bar">
  {#if loading}<span class="loading-dot"></span>{/if}
  {#if status.project_loaded}
    <span class="project-path">{status.project_path}</span>
    {#if status.dirty}<span class="dirty-badge">unsaved</span>{/if}
    <div class="project-actions">
      <button class="btn-sm" onclick={handleSave}>Save</button>
      <button class="btn-sm btn-muted" onclick={() => browserMode = 'save'}>Save As</button>
      <button class="btn-sm btn-muted" onclick={handleClose}>Close</button>
    </div>
  {:else}
    <button class="btn-sm" onclick={() => browserMode = 'open'}>Open Project</button>
  {/if}
</header>

{#if browserMode}
  <FileBrowser
    mode={browserMode}
    initialFilename={browserMode === 'save' ? getProjectBasename() : ''}
    onconfirm={handleBrowserConfirm}
    oncancel={() => browserMode = null}
  />
{/if}

<!-- Main editor layout -->
<div class="editor-layout">
  <!-- Selection Card -->
  <div class="selection-card">
    <div class="sel-tabs">
      <button class="sel-tab" class:active={selectionTab === 'banks'} onclick={() => selectionTab = 'banks'}>Banks</button>
      <button class="sel-tab" class:active={selectionTab === 'fixtures'} onclick={() => selectionTab = 'fixtures'}>Fixtures{#if fixtureProblems.length > 0}<span class="problem-badge">{fixtureProblems.length}</span>{/if}</button>
      <button class="sel-tab" class:active={selectionTab === 'hardware'} onclick={() => selectionTab = 'hardware'}>Hardware</button>
      <button class="sel-tab" class:active={selectionTab === 'scenes'} onclick={() => selectionTab = 'scenes'}>Scenes{#if sceneProblems.length > 0}<span class="problem-badge">{sceneProblems.length}</span>{/if}</button>
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
              <PadGrid
                items={bank.fixtures.map(f => ({ id: String(f.index), label: f.id }))}
                selected={selection?.kind === 'bank_fixture' && selection.bankIdx === bank.index ? String(selection.index) : undefined}
                onselect={(id) => trySelect({ kind: 'bank_fixture', bankIdx: bank.index, index: Number(id) })}
              />
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
          <PadGrid
            items={fixtures.map((f, i) => ({ id: String(f.index), label: f.id, sublabel: '@' + f.start_address, warn: warnFixtureIndices.has(i) }))}
            selected={selection?.kind === 'project_fixture' ? String(selection.index) : undefined}
            onselect={(id) => trySelect({ kind: 'project_fixture', index: Number(id) })}
          />
        {:else}
          <p class="empty-msg">Open a project first.</p>
        {/if}

      {:else if selectionTab === 'hardware'}
        {#if status.project_loaded}
          <HardwareForm bind:config={hwConfig} onsave={saveHardware} ondirty={() => { hwDirty = true; }} />
        {:else}
          <p class="empty-msg">Open a project first.</p>
        {/if}

      {:else if selectionTab === 'scenes'}
        {#if status.project_loaded}
          <div class="sel-toolbar">
            <button class="btn-xs btn-add" onclick={startAddScene}>+ Add Scene</button>
          </div>
          <PadGrid
            items={scenes.map((s, i) => {
              return { id: String(i), label: s.id, sublabel: s.type, warn: warnSceneIndices.has(i), dim: !s.enabled };
            })}
            selected={selection?.kind === 'scene' ? String(selection.index) : undefined}
            onselect={(id) => trySelect({ kind: 'scene', index: Number(id) })}
          />
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
            <button class="btn-sm btn-save" onclick={saveEditScene} disabled={!editIdValid}>Save</button>
            {#if !editIsNew}
              <button class="btn-sm btn-danger" onclick={confirmDeleteScene}>Delete</button>
            {/if}
          {:else}
            <button class="btn-sm btn-save" onclick={() => { if (editFixture) saveEditFixture(); else saveEditBankFixture(); }} disabled={!editIdValid}>Save</button>
            {#if !editIsNew}
              <button class="btn-sm btn-danger" onclick={confirmDeleteFixture}>Delete</button>
            {/if}
          {/if}
          <button class="btn-sm btn-muted" onclick={closeEdit}>Close</button>
        </div>
      </div>

      <div class="edit-body">
        {#if selection.kind === 'project_fixture' && editFixture}
          <FixtureForm bind:fixture={editFixture} isProject={true}
            existingIds={fixtures.map(f => f.id)}
            currentIndex={editIsNew ? undefined : selection.index}
            ondirty={markDirty} />

        {:else if selection.kind === 'bank_fixture' && editBankFixture}
          {@const bank = banks[selection.bankIdx]}
          <FixtureForm bind:fixture={editBankFixture} isProject={false}
            existingIds={bank ? bank.fixtures.map(f => f.id) : []}
            currentIndex={editIsNew ? undefined : selection.index}
            ondirty={markDirty} />

        {:else if selection.kind === 'scene' && editScene}
          <SceneForm bind:scene={editScene} {fixtures} {banks}
            existingIds={scenes.map(s => s.id)}
            currentIndex={editIsNew ? undefined : selection.index}
            ondirty={markDirty} />
        {/if}
      </div>
    </div>
  {/if}
</div>

{#if showDiscardModal}
  <ConfirmModal message="You have unsaved changes. Discard them?" confirmLabel="Discard" onconfirm={confirmDiscard} oncancel={cancelDiscard} />
{/if}

{#if showDeleteModal}
  <ConfirmModal message="Delete this item? This cannot be undone." confirmLabel="Delete" onconfirm={executeDelete} oncancel={cancelDelete} />
{/if}

{#if showProblemsModal}
  <Modal type="warning" title="Project Problems" onclose={() => showProblemsModal = false}>
    <ul class="problem-list">
      {#each problems as p}
        <li class="problem-item">
          <span class="problem-tab">{p.tab}</span>
          <span class="problem-msg">{p.message}</span>
        </li>
      {/each}
    </ul>
  </Modal>
{/if}

{#if showOpenErrorModal}
  <Modal type="error" title="Parse Error" onclose={() => showOpenErrorModal = false}>
    <pre class="error-detail">{openErrorDetail}</pre>
  </Modal>
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
  .problem-badge {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    margin-left: 0.3rem;
    min-width: 1rem;
    height: 1rem;
    padding: 0 0.25rem;
    border-radius: 9px;
    background: rgba(255, 180, 0, 0.25);
    color: rgb(255, 180, 0);
    font-size: 0.55rem;
    font-weight: 800;
  }

  .sel-content {
    flex: 1;
    overflow-y: auto;
    padding: 0.6rem;
  }

  .sel-toolbar { margin-bottom: 0.5rem; }

  /* Pad grid */

  /* Bank groups */
  .bank-group { margin-bottom: 0.6rem; display: flex; flex-direction: column; gap: 0.35rem; }
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
  .btn-xs.btn-add {
    background: var(--green);
    color: #111;
    border-color: rgba(78, 205, 196, 0.3);
  }
  .btn-xs.btn-add:hover { box-shadow: 0 0 6px var(--green-glow); }
  .empty-msg { color: var(--text-muted); font-size: 0.75rem; padding: 1.5rem; text-align: center; letter-spacing: 0.02em; }

  .loading-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    background: var(--accent);
    animation: pulse 0.8s ease-in-out infinite;
  }
  @keyframes pulse {
    0%, 100% { opacity: 0.3; }
    50% { opacity: 1; }
  }

  .problem-list {
    list-style: none;
    margin: 0;
    padding: 0;
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
  }
  .problem-item {
    display: flex;
    gap: 0.5rem;
    align-items: baseline;
    font-size: 0.7rem;
    padding: 0.3rem 0.5rem;
    background: rgba(255, 255, 255, 0.02);
    border-radius: 4px;
  }
  .problem-tab {
    font-weight: 700;
    text-transform: uppercase;
    font-size: 0.55rem;
    color: rgb(255, 180, 0);
    min-width: 4rem;
  }
  .problem-msg { color: var(--text-muted); }
  .error-detail {
    margin: 0;
    font-family: monospace;
    font-size: 0.75rem;
    color: var(--red);
    white-space: pre-wrap;
    word-break: break-word;
  }
</style>
