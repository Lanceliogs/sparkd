<script lang="ts">
  import { onMount } from 'svelte';
  import {
    getScenes,
    engineStart,
    engineStop,
    setBlackout,
    activateScene,
    releaseScene,
    reloadProject,
    getMidiStatus,
    getDmxStatus,
    editorBrowse,
    type EngineState,
    type SceneDef,
    type MidiStatus,
    type DmxStatus,
    type BrowseResult,
  } from '../lib/api';

  let state: EngineState = $state({ running: false, blackout: false, project: '' });
  let scenes: SceneDef[] = $state([]);
  let activeScenes: Set<string> = $state(new Set());
  let connected = $state(false);
  let ws: WebSocket | null = null;
  let reconnectTimer: ReturnType<typeof setTimeout>;

  /* Status polling */
  let midiStatus: MidiStatus = $state({ port_count: 0, ports: [] });
  let dmxStatus: DmxStatus = $state({ backend: 'none', state: 'disconnected', stats: { frames_sent: 0, write_errors: 0, reconnects: 0 } });
  let statusPollTimer: ReturnType<typeof setInterval> | null = null;

  /* File browser for project swap */
  let showBrowser = $state(false);
  let browseResult: BrowseResult | null = $state(null);

  function wsUrl(): string {
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${proto}//${location.host}/ws`;
  }

  function connect() {
    ws = new WebSocket(wsUrl());

    ws.onopen = () => {
      connected = true;
    };

    ws.onmessage = (ev) => {
      const msg = JSON.parse(ev.data);
      switch (msg.type) {
        case 'state':
          state = { running: msg.running, blackout: msg.blackout, project: msg.project ?? '' };
          activeScenes = new Set(msg.active_scenes ?? []);
          break;
        case 'started':
          state = { ...state, running: true };
          break;
        case 'stopped':
          state = { ...state, running: false };
          break;
        case 'blackout':
          state = { ...state, blackout: msg.enabled };
          break;
        case 'scene_on':
          activeScenes = new Set([...activeScenes, msg.id]);
          break;
        case 'scene_off':
          activeScenes = new Set([...activeScenes].filter(id => id !== msg.id));
          break;
      }
    };

    ws.onclose = () => {
      connected = false;
      ws = null;
      reconnectTimer = setTimeout(connect, 2000);
    };

    ws.onerror = () => {
      ws?.close();
    };
  }

  async function loadScenes() {
    try {
      scenes = await getScenes();
    } catch { /* will retry on reconnect */ }
  }

  function startStatusPoll() {
    if (statusPollTimer) return;
    pollStatus();
    statusPollTimer = setInterval(pollStatus, 1500);
  }

  function stopStatusPoll() {
    if (statusPollTimer) { clearInterval(statusPollTimer); statusPollTimer = null; }
  }

  async function pollStatus() {
    midiStatus = await getMidiStatus();
    dmxStatus = await getDmxStatus();
  }

  $effect(() => {
    if (state.running) startStatusPoll();
    else stopStatusPoll();
  });

  onMount(() => {
    connect();
    loadScenes();
    return () => {
      clearTimeout(reconnectTimer);
      stopStatusPoll();
      ws?.close();
    };
  });

  async function handleStart() {
    await engineStart();
  }

  async function handleStop() {
    await engineStop();
  }

  async function handleBlackout() {
    await setBlackout(!state.blackout);
  }

  async function handleReload() {
    await engineStop();
    await reloadProject();
    await engineStart();
    loadScenes();
  }

  async function openBrowser() {
    const startPath = state.project
      ? state.project.replace(/[/\\][^/\\]*$/, '')
      : '/';
    browseResult = await editorBrowse(startPath);
    showBrowser = true;
  }

  async function browseTo(path: string) {
    browseResult = await editorBrowse(path);
  }

  function browseUp() {
    if (!browseResult) return;
    const normalized = browseResult.path.replace(/\\/g, '/');
    const parts = normalized.split('/').filter(p => p !== '');
    if (parts.length <= 1) return;
    parts.pop();
    const parent = parts.join('/');
    const result = parent.match(/^[A-Za-z]:$/) ? parent + '/' : parent;
    browseTo(result);
  }

  function joinPath(base: string, name: string): string {
    const sep = base.endsWith('/') || base.endsWith('\\') ? '' : '/';
    return base + sep + name;
  }

  async function selectProject(path: string) {
    showBrowser = false;
    browseResult = null;
    await reloadProject(path);
    loadScenes();
  }

  async function handlePadDown(ev: PointerEvent, scene: SceneDef) {
    if (scene.trigger_mode === 'gate') {
      (ev.currentTarget as HTMLElement).setPointerCapture(ev.pointerId);
      await activateScene(scene.id);
    } else {
      if (activeScenes.has(scene.id)) {
        await releaseScene(scene.id);
      } else {
        await activateScene(scene.id);
      }
    }
  }

  async function handlePadUp(ev: PointerEvent, scene: SceneDef) {
    if (scene.trigger_mode === 'gate') {
      (ev.currentTarget as HTMLElement).releasePointerCapture(ev.pointerId);
      await releaseScene(scene.id);
    }
  }
</script>

<header class="status-bar">
  <div class="status-item">
    <span class="dot" class:green={connected && state.running} class:yellow={connected && !state.running} class:red={!connected}></span>
    {#if !connected}
      disconnected
    {:else if state.running}
      running
    {:else}
      stopped
    {/if}
  </div>
  <div class="status-item">
    {#if state.blackout}
      <span class="badge blackout">BLACKOUT</span>
    {/if}
  </div>
  <div class="status-item project">
    {state.project || 'no project'}
  </div>
</header>

<section class="controls">
  {#if connected}
    {#if !state.running}
      <button class="btn-start" onclick={handleStart}>Start</button>
    {:else}
      <button class="btn-stop" onclick={handleStop}>Stop</button>
    {/if}
    <button class="btn-blackout" class:active={state.blackout} onclick={handleBlackout}>
      {state.blackout ? 'Clear Blackout' : 'Blackout'}
    </button>
    {#if !state.running}
      <button class="btn-reload" onclick={handleReload}>Reload & Start</button>
      <button class="btn-load" onclick={openBrowser}>Load Project</button>
    {/if}
  {/if}
</section>

{#if state.running}
<section class="status-footer">
  <div class="status-group">
    <span class="status-label">MIDI</span>
    {#if midiStatus.port_count === 0}
      <span class="status-value muted">no ports</span>
    {:else}
      {#each midiStatus.ports as port}
        <span class="status-value">
          <span class="dot-sm" class:green={port.connected} class:red={!port.connected}></span>
          {port.pattern}
        </span>
      {/each}
    {/if}
  </div>
  <div class="status-group">
    <span class="status-label">DMX</span>
    <span class="status-value">
      <span class="dot-sm" class:green={dmxStatus.state === 'connected'} class:yellow={dmxStatus.state === 'connecting'} class:red={dmxStatus.state === 'disconnected' || dmxStatus.state === 'error'}></span>
      {dmxStatus.backend}
    </span>
    <span class="status-value muted">{dmxStatus.stats.frames_sent} frames</span>
    {#if dmxStatus.stats.write_errors > 0}
      <span class="status-value error">{dmxStatus.stats.write_errors} errors</span>
    {/if}
  </div>
</section>
{/if}

<section class="pad-grid">
  {#each scenes as scene}
    <button
      class="pad"
      class:active={activeScenes.has(scene.id)}
      class:gate={scene.trigger_mode === 'gate'}
      class:toggle={scene.trigger_mode === 'toggle'}
      onpointerdown={(ev) => handlePadDown(ev, scene)}
      onpointerup={(ev) => handlePadUp(ev, scene)}
    >
      <span class="pad-name">{scene.name}</span>
      <span class="pad-type">{scene.type}</span>
    </button>
  {/each}
</section>

{#if showBrowser}
<div class="modal-overlay" role="dialog" tabindex="-1" onkeydown={(e) => { if (e.key === 'Escape') showBrowser = false; }}>
  <button class="modal-backdrop" aria-label="Close" onclick={() => showBrowser = false} tabindex="-1"></button>
  <div class="modal">
    <header class="modal-header">
      <h3>Load Project</h3>
      <button class="modal-close" onclick={() => showBrowser = false}>X</button>
    </header>
    {#if browseResult}
      <div class="browse-path">{browseResult.path}</div>
      <div class="browse-list">
        <button class="browse-item dir" onclick={browseUp}>..</button>
        {#each browseResult.entries as entry}
          {#if entry.type === 'dir'}
            <button class="browse-item dir" onclick={() => browseTo(joinPath(browseResult!.path, entry.name))}>
              {entry.name}/
            </button>
          {:else if entry.name.endsWith('.yaml') || entry.name.endsWith('.yml')}
            <button class="browse-item file" onclick={() => selectProject(joinPath(browseResult!.path, entry.name))}>
              {entry.name}
            </button>
          {:else}
            <span class="browse-item disabled">{entry.name}</span>
          {/if}
        {/each}
      </div>
    {/if}
  </div>
</div>
{/if}

<style>
  .status-bar {
    display: flex;
    align-items: center;
    gap: 1.5rem;
    padding: 0.8rem 1rem;
    background: var(--bg-surface);
    border-radius: var(--radius);
    margin-bottom: 1rem;
  }

  .status-item {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    font-size: 0.85rem;
  }

  .project {
    margin-left: auto;
    color: var(--text-muted);
    font-size: 0.8rem;
  }

  .dot {
    width: 10px;
    height: 10px;
    border-radius: 50%;
  }

  .dot.green { background: var(--green); }
  .dot.yellow { background: var(--yellow); }
  .dot.red { background: var(--red); }

  .badge {
    padding: 0.2rem 0.5rem;
    border-radius: 4px;
    font-size: 0.75rem;
    font-weight: 700;
    text-transform: uppercase;
  }

  .badge.blackout {
    background: var(--red);
    color: white;
  }

  .controls {
    display: flex;
    gap: 0.5rem;
    margin-bottom: 1.5rem;
  }

  .btn-start {
    background: var(--green);
    color: #111;
  }

  .btn-stop {
    background: var(--red);
    color: white;
  }

  .btn-blackout {
    background: var(--bg-card);
    color: var(--text);
    border: 2px solid var(--text-muted);
  }

  .btn-blackout.active {
    background: var(--red);
    color: white;
    border-color: var(--red);
  }

  .btn-reload {
    background: var(--bg-card);
    color: var(--text);
    border: 2px solid var(--text-muted);
  }

  .pad-grid {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(var(--pad-size), 1fr));
    gap: 0.5rem;
  }

  .pad {
    aspect-ratio: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    gap: 0.3rem;
    background: var(--bg-card);
    border: 2px solid var(--text-muted);
    color: var(--text);
    padding: 0.5rem;
    user-select: none;
    touch-action: none;
    min-width: var(--pad-size);
    min-height: var(--pad-size);
  }

  .pad:hover {
    border-color: var(--accent);
  }

  .pad.active {
    background: var(--accent);
    color: white;
    border-color: var(--accent-hover);
  }

  .pad-name {
    font-size: 0.75rem;
    font-weight: 600;
    text-align: center;
    word-break: break-word;
  }

  .pad-type {
    font-size: 0.6rem;
    color: var(--text-muted);
    text-transform: uppercase;
  }

  .pad.active .pad-type {
    color: rgba(255, 255, 255, 0.7);
  }

  .btn-load {
    background: var(--bg-card);
    color: var(--text);
    border: 2px solid var(--accent);
  }

  /* Status footer */
  .status-footer {
    position: fixed;
    bottom: 0;
    left: 0;
    right: 0;
    display: flex;
    gap: 2rem;
    padding: 0.6rem 1rem;
    background: var(--bg-surface);
    border-top: 1px solid var(--bg-card);
    font-size: 0.8rem;
    z-index: 50;
  }

  .status-group {
    display: flex;
    align-items: center;
    gap: 0.6rem;
  }

  .status-label {
    font-weight: 700;
    text-transform: uppercase;
    color: var(--text-muted);
    font-size: 0.7rem;
  }

  .status-value {
    display: flex;
    align-items: center;
    gap: 0.3rem;
  }

  .status-value.muted { color: var(--text-muted); }
  .status-value.error { color: var(--red); }

  .dot-sm {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    display: inline-block;
  }

  .dot-sm.green { background: var(--green); }
  .dot-sm.yellow { background: var(--yellow); }
  .dot-sm.red { background: var(--red); }

  /* Modal */
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
    background: rgba(0, 0, 0, 0.6);
    border: none;
    cursor: default;
  }

  .modal {
    position: relative;
    background: var(--bg-surface);
    border-radius: var(--radius);
    width: 90%;
    max-width: 500px;
    max-height: 70vh;
    display: flex;
    flex-direction: column;
    overflow: hidden;
  }

  .modal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    padding: 0.8rem 1rem;
    border-bottom: 1px solid var(--bg-card);
  }

  .modal-header h3 { margin: 0; font-size: 1rem; }

  .modal-close {
    background: none;
    border: none;
    color: var(--text-muted);
    font-size: 1rem;
    padding: 0.2rem 0.5rem;
  }

  .browse-path {
    padding: 0.5rem 1rem;
    font-size: 0.75rem;
    color: var(--text-muted);
    border-bottom: 1px solid var(--bg-card);
    word-break: break-all;
  }

  .browse-list {
    overflow-y: auto;
    flex: 1;
    padding: 0.5rem;
  }

  .browse-item {
    display: block;
    width: 100%;
    text-align: left;
    padding: 0.4rem 0.8rem;
    border: none;
    background: none;
    color: var(--text);
    font-size: 0.85rem;
    border-radius: 4px;
  }

  .browse-item:hover { background: var(--bg-card); }
  .browse-item.dir { color: var(--accent); }
  .browse-item.file { font-weight: 500; }
  .browse-item.disabled { color: var(--text-muted); opacity: 0.5; }
</style>
