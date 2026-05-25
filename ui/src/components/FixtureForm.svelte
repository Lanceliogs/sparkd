<script lang="ts">
  import type { EditorFixture, EditorBank, BankFixture, Channel } from '../lib/api';
  import { validateId, type IdStatus } from '../lib/validate';

  export interface OccupiedRange {
    id: string;
    start: number;
    end: number;
  }

  interface Props {
    fixture: EditorFixture | BankFixture;
    isProject: boolean;
    existingIds?: string[];
    currentIndex?: number;
    occupancy?: OccupiedRange[];
    banks?: EditorBank[];
    projectFixtures?: EditorFixture[];
    ondirty: () => void;
  }

  let { fixture = $bindable(), isProject, existingIds = [], currentIndex, occupancy = [], banks = [], projectFixtures = [], ondirty }: Props = $props();

  let idStatus: IdStatus = $derived(validateId(fixture.id, existingIds, currentIndex));

  let dmxConflicts = $derived.by(() => {
    if (!isEditorFixture(fixture) || !fixture.start_address || fixture.channel_count <= 0) return [];
    const myStart = fixture.start_address;
    const myEnd = myStart + fixture.channel_count - 1;
    return occupancy.filter(r => r.id !== fixture.id && r.start <= myEnd && r.end >= myStart);
  });

  let templateOptions = $derived.by(() => {
    const opts: { key: string; label: string }[] = [];
    for (const bank of banks) {
      for (const bf of bank.fixtures) {
        opts.push({ key: `${bank.id}:${bf.id}`, label: `${bank.id}:${bf.id} — ${bf.name || bf.id}` });
      }
    }
    return opts;
  });

  let copyFromOptions = $derived.by(() => {
    return projectFixtures
      .filter(f => f.id !== fixture.id)
      .map(f => ({ key: f.id, label: `${f.id} — ${f.name || f.id}` }));
  });

  let templateValid = $derived.by(() => {
    if (!isEditorFixture(fixture) || !fixture.template) return true;
    return templateOptions.some(o => o.key === fixture.template);
  });

  let copyFromValid = $derived.by(() => {
    if (!isEditorFixture(fixture) || !fixture.copy_from) return true;
    return copyFromOptions.some(o => o.key === fixture.copy_from);
  });

  function isEditorFixture(f: EditorFixture | BankFixture): f is EditorFixture {
    return 'start_address' in f;
  }

  function addChannel() {
    fixture.channels = [...fixture.channels, { name: '', offset: fixture.channels.length }];
    fixture.channel_count = fixture.channels.length;
    ondirty();
  }

  function removeChannel(i: number) {
    fixture.channels = fixture.channels.filter((_, idx) => idx !== i);
    fixture.channel_count = fixture.channels.length;
    ondirty();
  }
</script>

{#if isProject && isEditorFixture(fixture)}
  <div class="form-grid">
    <label for="ef-id">ID</label>
    <div class="id-field">
      <input id="ef-id" type="text" bind:value={fixture.id} oninput={ondirty} class:id-invalid={idStatus === 'invalid' || idStatus === 'empty'} class:id-duplicate={idStatus === 'duplicate'} />
      <span class="id-status" class:valid={idStatus === 'valid'} class:invalid={idStatus === 'invalid' || idStatus === 'empty'} class:duplicate={idStatus === 'duplicate'}>
        {#if idStatus === 'valid'}Valid{:else if idStatus === 'duplicate'}Already used{:else}Invalid{/if}
      </span>
    </div>
    <label for="ef-name">Name</label>
    <input id="ef-name" type="text" bind:value={fixture.name} oninput={ondirty} />
    <label for="ef-addr">Start Addr</label>
    <input id="ef-addr" type="number" min="1" max="512" bind:value={fixture.start_address} oninput={ondirty} class:addr-conflict={dmxConflicts.length > 0} />
    {#if dmxConflicts.length > 0}
      <span class="grid-span"></span>
      <span class="addr-warn">Overlaps: {dmxConflicts.map(c => c.id).join(', ')}</span>
    {/if}
    <label for="ef-tpl">Template</label>
    <div class="ref-field">
      <input id="ef-tpl" type="text" placeholder="bank:fixture" bind:value={fixture.template} oninput={ondirty} class:ref-warn={!templateValid} list="tpl-options" />
      <datalist id="tpl-options">
        {#each templateOptions as opt}
          <option value={opt.key}>{opt.label}</option>
        {/each}
      </datalist>
      {#if !templateValid}<span class="ref-hint">Not found</span>{/if}
    </div>
    <label for="ef-copy">Copy From</label>
    <div class="ref-field">
      <input id="ef-copy" type="text" placeholder="fixture-id" bind:value={fixture.copy_from} oninput={ondirty} class:ref-warn={!copyFromValid} list="copy-options" />
      <datalist id="copy-options">
        {#each copyFromOptions as opt}
          <option value={opt.key}>{opt.label}</option>
        {/each}
      </datalist>
      {#if !copyFromValid}<span class="ref-hint">Not found</span>{/if}
    </div>
  </div>

  {#if !fixture.template && !fixture.copy_from}
    <div class="channel-section">
      <div class="ch-header">
        <span>Channels ({fixture.channel_count})</span>
        <button class="btn-xs btn-add" onclick={addChannel}>+</button>
      </div>
      {#each fixture.channels as ch, i}
        <div class="channel-row">
          <input type="text" placeholder="name" bind:value={ch.name} oninput={ondirty} class="ch-name" />
          <input type="number" min="0" max="255" bind:value={ch.offset} oninput={ondirty} class="ch-offset" />
          <button class="btn-xs btn-danger" onclick={() => removeChannel(i)}>x</button>
        </div>
      {/each}
    </div>
  {/if}

{:else}
  <div class="form-grid">
    <label for="bf-id">ID</label>
    <div class="id-field">
      <input id="bf-id" type="text" bind:value={fixture.id} oninput={ondirty} class:id-invalid={idStatus === 'invalid' || idStatus === 'empty'} class:id-duplicate={idStatus === 'duplicate'} />
      <span class="id-status" class:valid={idStatus === 'valid'} class:invalid={idStatus === 'invalid' || idStatus === 'empty'} class:duplicate={idStatus === 'duplicate'}>
        {#if idStatus === 'valid'}Valid{:else if idStatus === 'duplicate'}Already used{:else}Invalid{/if}
      </span>
    </div>
    <label for="bf-name">Name</label>
    <input id="bf-name" type="text" bind:value={fixture.name} oninput={ondirty} />
  </div>

  <div class="channel-section">
    <div class="ch-header">
      <span>Channels ({fixture.channel_count})</span>
      <button class="btn-xs btn-add" onclick={addChannel}>+</button>
    </div>
    {#each fixture.channels as ch, i}
      <div class="channel-row">
        <input type="text" placeholder="name" bind:value={ch.name} oninput={ondirty} class="ch-name" />
        <input type="number" min="0" max="255" bind:value={ch.offset} oninput={ondirty} class="ch-offset" />
        <button class="btn-xs btn-danger" onclick={() => removeChannel(i)}>x</button>
      </div>
    {/each}
  </div>
{/if}

<style>
  .form-grid {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 0.4rem 0.6rem;
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
  .form-grid input {
    background: var(--bg);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.3rem 0.5rem;
    font-size: 0.75rem;
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.3);
  }
  .form-grid input:focus {
    outline: none;
    border-color: rgba(233, 69, 96, 0.4);
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.3), 0 0 4px var(--accent-glow);
  }

  .addr-conflict {
    border-color: rgba(255, 180, 0, 0.6) !important;
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.3), 0 0 4px rgba(255, 180, 0, 0.3) !important;
  }
  .grid-span { display: none; }
  .addr-warn {
    grid-column: 2;
    font-size: 0.6rem;
    color: rgb(255, 180, 0);
    font-weight: 600;
    margin-top: -0.2rem;
  }
  .ref-field {
    display: flex;
    align-items: center;
    gap: 0.4rem;
  }
  .ref-field input { flex: 1; }
  .ref-field input.ref-warn {
    border-color: rgba(255, 180, 0, 0.6);
  }
  .ref-hint {
    font-size: 0.6rem;
    color: rgb(255, 180, 0);
    font-weight: 700;
    white-space: nowrap;
  }

  .channel-section { margin-top: 0.6rem; }
  .ch-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    font-size: 0.7rem;
    color: var(--text-muted);
    margin-bottom: 0.3rem;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.03em;
  }
  .channel-row {
    display: flex;
    gap: 0.3rem;
    align-items: center;
    margin-bottom: 0.25rem;
  }
  .ch-name {
    flex: 1;
    background: var(--bg);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.25rem 0.4rem;
    font-size: 0.7rem;
  }
  .ch-offset {
    width: 3.5rem;
    background: var(--bg);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.25rem 0.4rem;
    font-size: 0.7rem;
    text-align: center;
  }

  .btn-xs {
    font-size: 0.6rem;
    padding: 0.15rem 0.4rem;
    border-radius: 3px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    background: var(--bg-pad);
    color: var(--text-muted);
    cursor: pointer;
  }
  .btn-xs.btn-danger { background: var(--red); color: white; border-color: rgba(233, 69, 96, 0.3); }
  .btn-xs.btn-add { background: var(--green); color: #111; border-color: rgba(78, 205, 196, 0.3); }

  .id-field {
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }
  .id-field input { flex: 1; }
  .id-field input.id-invalid { border-color: rgba(233, 69, 96, 0.6); }
  .id-field input.id-duplicate { border-color: rgba(255, 180, 0, 0.6); }
  .id-status {
    font-size: 0.6rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.03em;
    white-space: nowrap;
  }
  .id-status.valid { color: var(--green); }
  .id-status.invalid { color: var(--red); }
  .id-status.duplicate { color: rgb(255, 180, 0); }
</style>
