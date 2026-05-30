<script lang="ts">
  import type { EditorScene, EditorFixture, EditorBank, Channel } from '../lib/api';
  import { validateId, type IdStatus } from '../lib/validate';
  import { resolveFixtureChannels } from '../lib/fixture-resolve';
  import DmxValueInput from './DmxValueInput.svelte';

  interface Props {
    scene: EditorScene;
    fixtures: EditorFixture[];
    banks: EditorBank[];
    existingIds?: string[];
    currentIndex?: number;
    ondirty: () => void;
  }

  let { scene = $bindable(), fixtures, banks, existingIds = [], currentIndex, ondirty }: Props = $props();

  let idStatus: IdStatus = $derived(validateId(scene.id, existingIds, currentIndex));

  interface TargetEntry {
    key: string;
    fixture: string;
    channel: string;
    dmxAddr: number;
  }

  function getFixtureTargets(): TargetEntry[] {
    const targets: TargetEntry[] = [];
    const sorted = [...fixtures].sort((a, b) => a.start_address - b.start_address);
    for (const fix of sorted) {
      const channels = resolveFixtureChannels(fix, fixtures, banks);
      for (const ch of channels) {
        targets.push({
          key: `${fix.id}.${ch.name}`,
          fixture: fix.id,
          channel: ch.name,
          dmxAddr: fix.start_address + ch.offset,
        });
      }
    }
    return targets;
  }

  function validateTarget(target: string): 'valid' | 'warn' {
    if (!target || !target.includes('.')) return 'warn';
    const [fixId, chName] = target.split('.', 2);
    const fix = fixtures.find(f => f.id === fixId);
    if (!fix) return 'warn';
    const channels = resolveFixtureChannels(fix, fixtures, banks);
    if (!channels.some(c => c.name === chName)) return 'warn';
    return 'valid';
  }

  function getUsedTargets(excludeIdx?: number, stepIdx?: number): Set<string> {
    const used = new Set<string>();
    if (scene.type === 'static') {
      scene.values.forEach((v, i) => { if (i !== excludeIdx && v.target) used.add(v.target); });
    } else {
      if (stepIdx !== undefined) {
        const step = scene.steps[stepIdx];
        if (step) step.values.forEach((v, i) => { if (i !== excludeIdx && v.target) used.add(v.target); });
      }
    }
    return used;
  }

  /* Combobox state */
  let openCombo: string | null = $state(null);
  let comboFilter = $state('');
  let blurTimeout: ReturnType<typeof setTimeout> | null = null;

  function openCombobox(id: string, currentValue: string) {
    if (blurTimeout) { clearTimeout(blurTimeout); blurTimeout = null; }
    openCombo = id;
    comboFilter = currentValue;
  }

  function closeCombobox() {
    openCombo = null;
    comboFilter = '';
  }

  function filteredTargets(filter: string): TargetEntry[] {
    const all = getFixtureTargets();
    if (!filter) return all;
    const lower = filter.toLowerCase();
    return all.filter(t => t.key.toLowerCase().includes(lower) || String(t.dmxAddr).includes(lower));
  }

  function addValue() {
    scene.values = [...scene.values, { target: '', value: 0 }];
    ondirty();
  }

  function removeValue(i: number) {
    scene.values = scene.values.filter((_, idx) => idx !== i);
    ondirty();
  }

  function addStep() {
    scene.steps = [...scene.steps, { duration_ms: 1000, transition: 'hold', values: [] }];
    ondirty();
  }

  function removeStep(i: number) {
    scene.steps = scene.steps.filter((_, idx) => idx !== i);
    ondirty();
  }

  function addStepValue(stepIdx: number) {
    scene.steps[stepIdx].values = [...scene.steps[stepIdx].values, { target: '', value: 0 }];
    ondirty();
  }

  function removeStepValue(stepIdx: number, valIdx: number) {
    scene.steps[stepIdx].values = scene.steps[stepIdx].values.filter((_, idx) => idx !== valIdx);
    ondirty();
  }

  function handleComboBlur(e: FocusEvent) {
    const related = e.relatedTarget as HTMLElement | null;
    if (related?.closest('.combo-dropdown')) return;
    blurTimeout = setTimeout(() => { blurTimeout = null; closeCombobox(); }, 200);
  }
</script>

<div class="form-grid">
  <label for="sc-id">ID</label>
  <div class="id-field">
    <input id="sc-id" type="text" bind:value={scene.id} oninput={ondirty} class:id-invalid={idStatus === 'invalid' || idStatus === 'empty'} class:id-duplicate={idStatus === 'duplicate'} />
    <span class="id-status" class:valid={idStatus === 'valid'} class:invalid={idStatus === 'invalid' || idStatus === 'empty'} class:duplicate={idStatus === 'duplicate'}>
      {#if idStatus === 'valid'}Valid{:else if idStatus === 'duplicate'}Already used{:else}Invalid{/if}
    </span>
  </div>
  <label for="sc-name">Name</label>
  <input id="sc-name" type="text" bind:value={scene.name} oninput={ondirty} />
  <label for="sc-type">Type</label>
  <select id="sc-type" bind:value={scene.type} onchange={ondirty}>
    <option value="static">static</option>
    <option value="sequence">sequence</option>
  </select>
  <label for="sc-trigger">Trigger</label>
  <select id="sc-trigger" bind:value={scene.trigger_mode} onchange={ondirty}>
    <option value="gate">gate</option>
    <option value="toggle">toggle</option>
  </select>
  <label for="sc-ch">Channel</label>
  <input id="sc-ch" type="number" min="1" max="16" bind:value={scene.channel} oninput={ondirty} />
  <label for="sc-note">Note</label>
  <input id="sc-note" type="number" min="0" max="127" bind:value={scene.note} oninput={ondirty} />
  <label for="sc-enabled">Enabled</label>
  <input id="sc-enabled" type="checkbox" bind:checked={scene.enabled} onchange={ondirty} />
  {#if scene.type === 'sequence'}
    <label for="sc-loop">Loop</label>
    <input id="sc-loop" type="checkbox" bind:checked={scene.loop} onchange={ondirty} />
  {/if}
</div>

{#if scene.type === 'static'}
  <div class="channel-section">
    <div class="ch-header">
      <span>Values ({scene.values.length})</span>
      <button class="btn-xs btn-add" onclick={addValue}>+</button>
    </div>
    {#each scene.values as val, i}
      {@const used = getUsedTargets(i)}
      {@const comboId = `static-${i}`}
      <div class="channel-row">
        <div class="combo-wrap">
          <input type="text" placeholder="fixture.channel"
            bind:value={val.target}
            oninput={(e) => { const v = (e.target as HTMLInputElement).value; comboFilter = v; if (openCombo !== comboId) openCombo = comboId; ondirty(); }}
                onfocus={() => openCombobox(comboId, val.target)}
                onblur={handleComboBlur}
                class="ch-name"
                class:warn={validateTarget(val.target) === 'warn' && !!val.target}
                autocomplete="off" />
              {#if openCombo === comboId}
                <div class="combo-dropdown">
                  {#each filteredTargets(comboFilter) as t (t.key)}
                    {@const isUsed = used.has(t.key)}
                    <button class="combo-option" class:used={isUsed}
                        onmousedown={(e) => { e.preventDefault(); val.target = t.key; closeCombobox(); ondirty(); }}
                        tabindex="-1">
                      <span class="opt-key">{t.key}</span>
                      <span class="opt-addr">DMX {t.dmxAddr}</span>
                      {#if isUsed}<span class="opt-used">used</span>{/if}
                    </button>
                  {/each}
                  {#if filteredTargets(comboFilter).length === 0}
                    <span class="combo-empty">No matches</span>
                  {/if}
                </div>
              {/if}
            </div>
            <DmxValueInput bind:value={val.value} onchange={ondirty} />
            <button class="btn-xs btn-danger" onclick={() => removeValue(i)}>x</button>
          </div>
        {/each}
      </div>
{:else}
  <div class="channel-section">
    <div class="ch-header">
      <span>Steps ({scene.steps.length})</span>
      <button class="btn-xs btn-add" onclick={addStep}>+</button>
    </div>
    {#each scene.steps as step, si}
      <div class="step-card">
        <div class="step-header">
          <span class="step-label">Step {si + 1}</span>
          <div class="step-field">
            <label for="step-duration-{si}">Duration</label>
            <input id="step-duration-{si}" type="number" min="0" placeholder="ms" bind:value={step.duration_ms} oninput={ondirty} class="ch-offset" />
          </div>
          <div class="step-field">
            <label for="step-transition-{si}">Transition</label>
            <select id="step-transition-{si}" bind:value={step.transition} onchange={ondirty} class="input-sm">
              <option value="hold">hold</option>
              <option value="linear">linear</option>
            </select>
          </div>
          <button class="btn-xs btn-step-remove" onclick={() => removeStep(si)}>DEL.</button>
        </div>
        {#each step.values as val, vi}
          {@const used = getUsedTargets(vi, si)}
          {@const comboId = `step-${si}-${vi}`}
          <div class="channel-row">
            <div class="combo-wrap">
              <input type="text" placeholder="fixture.channel"
                bind:value={val.target}
                oninput={(e) => { const v = (e.target as HTMLInputElement).value; comboFilter = v; if (openCombo !== comboId) openCombo = comboId; ondirty(); }}
                onfocus={() => openCombobox(comboId, val.target)}
                onblur={handleComboBlur}
                class="ch-name"
                class:warn={validateTarget(val.target) === 'warn' && !!val.target}
                autocomplete="off" />
              {#if openCombo === comboId}
                <div class="combo-dropdown">
                  {#each filteredTargets(comboFilter) as t (t.key)}
                    {@const isUsed = used.has(t.key)}
                    <button class="combo-option" class:used={isUsed}
                      onmousedown={(e) => { e.preventDefault(); val.target = t.key; closeCombobox(); ondirty(); }}
                      tabindex="-1">
                      <span class="opt-key">{t.key}</span>
                      <span class="opt-addr">DMX {t.dmxAddr}</span>
                      {#if isUsed}<span class="opt-used">used</span>{/if}
                    </button>
                  {/each}
                  {#if filteredTargets(comboFilter).length === 0}
                    <span class="combo-empty">No matches</span>
                  {/if}
                </div>
              {/if}
            </div>
            <DmxValueInput bind:value={val.value} onchange={ondirty} />
            <button class="btn-xs btn-danger" onclick={() => removeStepValue(si, vi)}>x</button>
          </div>
        {/each}
        <button class="btn-xs btn-add" onclick={() => addStepValue(si)}>+ Value</button>
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
  .form-grid input, .form-grid select {
    background: var(--bg);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.3rem 0.5rem;
    font-size: 0.75rem;
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.3);
  }
  .form-grid input:focus, .form-grid select:focus {
    outline: none;
    border-color: rgba(233, 69, 96, 0.4);
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.3), 0 0 4px var(--accent-glow);
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

  .combo-wrap {
    flex: 1;
    position: relative;
  }
  .ch-name {
    width: 100%;
    background: var(--bg);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 4px;
    color: var(--text);
    padding: 0.25rem 0.4rem;
    font-size: 0.7rem;
  }
  .ch-name:focus {
    outline: none;
    border-color: rgba(233, 69, 96, 0.4);
    box-shadow: inset 0 1px 2px rgba(0, 0, 0, 0.3), 0 0 4px var(--accent-glow);
  }
  .ch-name.warn {
    border-color: var(--yellow);
    box-shadow: 0 0 3px rgba(247, 211, 84, 0.3);
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

  .combo-dropdown {
    position: absolute;
    top: 100%;
    left: 0;
    right: 0;
    z-index: 200;
    background: var(--bg-surface);
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 4px;
    max-height: 160px;
    overflow-y: auto;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.5);
    margin-top: 2px;
  }
  .combo-option {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    width: 100%;
    padding: 0.3rem 0.5rem;
    border: none;
    background: none;
    color: var(--text);
    font-size: 0.68rem;
    text-align: left;
    cursor: pointer;
    border-radius: 2px;
  }
  .combo-option:hover {
    background: var(--bg-card);
  }
  .combo-option.used {
    opacity: 0.5;
  }
  .opt-key {
    flex: 1;
    font-weight: 600;
  }
  .opt-addr {
    color: var(--text-muted);
    font-family: monospace;
    font-size: 0.6rem;
  }
  .opt-used {
    font-size: 0.55rem;
    color: var(--yellow);
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.04em;
  }
  .combo-empty {
    display: block;
    padding: 0.4rem 0.5rem;
    color: var(--text-muted);
    font-size: 0.65rem;
    font-style: italic;
  }

  .step-card {
    background: var(--bg-surface);
    border: 1px solid rgba(255, 255, 255, 0.04);
    border-radius: var(--radius);
    padding: 0.5rem;
    margin-bottom: 0.5rem;
  }
  .step-header {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    margin-bottom: 0.4rem;
  }
  .step-label {
    font-size: 0.7rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.04em;
    color: var(--text);
    margin-right: auto;
  }
  .step-field {
    display: flex;
    align-items: center;
    gap: 0.3rem;
  }
  .step-field label {
    font-size: 0.65rem;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.03em;
    color: var(--text-muted);
  }
  .btn-xs.btn-step-remove {
    background: transparent;
    border: 1px solid var(--red);
    color: var(--red);
  }
  .btn-xs.btn-step-remove:hover {
    background: var(--red);
    color: white;
  }

  .input-sm {
    font-size: 0.7rem;
    padding: 0.25rem 0.4rem;
    width: auto;
    min-width: 4rem;
    background: var(--bg);
    border: 1px solid rgba(255, 255, 255, 0.08);
    border-radius: 4px;
    color: var(--text);
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
