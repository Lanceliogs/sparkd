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
  /* Status bar */
  .status-bar {
    display: flex;
    align-items: center;
    gap: 1.5rem;
    padding: 0.7rem 1rem;
    background: var(--bg-surface);
    border: 1px solid rgba(255,255,255,0.04);
    border-radius: var(--radius);
    margin-bottom: 1rem;
    box-shadow: inset 0 1px 3px rgba(0,0,0,0.4);
  }

  .status-item {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    font-size: 0.8rem;
  }

  .project {
    margin-left: auto;
    color: var(--text-muted);
    font-size: 0.75rem;
    font-family: monospace;
  }

  .dot {
    width: 9px;
    height: 9px;
    border-radius: 50%;
  }

  .dot.green { background: var(--green); box-shadow: 0 0 6px var(--green-glow); }
  .dot.yellow { background: var(--yellow); box-shadow: 0 0 6px rgba(247, 211, 84, 0.4); }
  .dot.red { background: var(--red); box-shadow: 0 0 6px var(--accent-glow); }

  .badge {
    padding: 0.2rem 0.5rem;
    border-radius: 4px;
    font-size: 0.65rem;
    font-weight: 700;
    text-transform: uppercase;
    letter-spacing: 0.05em;
  }

  .badge.blackout {
    background: var(--red);
    color: white;
    box-shadow: 0 0 8px var(--accent-glow);
  }

  /* Controls */
  .controls {
    display: flex;
    gap: 0.5rem;
    margin-bottom: 1.5rem;
  }

  .btn-start {
    background: var(--green);
    color: #111;
    border: 1px solid rgba(78, 205, 196, 0.3);
    box-shadow: 0 2px 6px rgba(78, 205, 196, 0.2);
  }
  .btn-start:hover {
    box-shadow: 0 2px 12px var(--green-glow);
  }

  .btn-stop {
    background: var(--red);
    color: white;
    border: 1px solid rgba(233, 69, 96, 0.3);
    box-shadow: 0 2px 6px rgba(233, 69, 96, 0.2);
  }
  .btn-stop:hover {
    box-shadow: 0 2px 12px var(--accent-glow);
  }

  .btn-blackout {
    background: var(--bg-pad);
    color: var(--text);
    border: 1px solid rgba(255,255,255,0.1);
  }
  .btn-blackout:hover {
    border-color: var(--accent);
    box-shadow: 0 0 8px var(--accent-glow);
  }

  .btn-blackout.active {
    background: var(--red);
    color: white;
    border-color: var(--red);
    box-shadow: 0 0 12px var(--accent-glow);
  }

  .btn-reload {
    background: var(--bg-pad);
    color: var(--text);
    border: 1px solid rgba(255,255,255,0.1);
  }
  .btn-reload:hover {
    border-color: var(--accent);
    box-shadow: 0 0 8px var(--accent-glow);
  }

  /* Pad grid */
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
    background: var(--bg-pad);
    border: 1.5px solid rgba(255,255,255,0.06);
    border-radius: var(--pad-radius);
    color: var(--text);
    padding: 0.5rem;
    user-select: none;
    touch-action: none;
    min-width: var(--pad-size);
    min-height: var(--pad-size);
    box-shadow:
      inset 0 2px 4px rgba(0,0,0,0.4),
      inset 0 -1px 2px rgba(255,255,255,0.03);
    transition: box-shadow 0.12s, background 0.12s, transform 0.08s, border-color 0.12s;
  }

  .pad:hover {
    border-color: rgba(233, 69, 96, 0.4);
    box-shadow:
      inset 0 2px 4px rgba(0,0,0,0.4),
      0 0 8px var(--accent-glow);
  }

  .pad:active {
    transform: translateY(1px);
    box-shadow: inset 0 3px 6px rgba(0,0,0,0.6);
  }

  .pad.active {
    background: var(--accent);
    color: white;
    border-color: var(--accent);
    box-shadow:
      0 0 14px var(--accent-glow),
      0 0 4px var(--accent-glow),
      inset 0 1px 2px rgba(255,255,255,0.15);
  }

  .pad-name {
    font-size: 0.7rem;
    font-weight: 700;
    text-align: center;
    word-break: break-word;
    letter-spacing: 0.02em;
  }

  .pad-type {
    font-size: 0.55rem;
    color: var(--text-muted);
    text-transform: uppercase;
    letter-spacing: 0.06em;
  }

  .pad.active .pad-type {
    color: rgba(255, 255, 255, 0.7);
  }

  .btn-load {
    background: var(--bg-pad);
    color: var(--text);
    border: 1px solid rgba(233, 69, 96, 0.3);
  }
  .btn-load:hover {
    box-shadow: 0 0 10px var(--accent-glow);
    border-color: var(--accent);
  }

  /* Status footer */
  .status-footer {
    position: fixed;
    bottom: 0;
    left: 0;
    right: 0;
    display: flex;
    gap: 2rem;
    padding: 0.5rem 1rem;
    background: var(--bg-surface);
    border-top: 1px solid rgba(233, 69, 96, 0.15);
    font-size: 0.75rem;
    z-index: 50;
    box-shadow: 0 -2px 8px rgba(0,0,0,0.3);
  }

  .status-group {
    display: flex;
    align-items: center;
    gap: 0.5rem;
  }

  .status-label {
    font-weight: 700;
    text-transform: uppercase;
    color: var(--text-muted);
    font-size: 0.6rem;
    letter-spacing: 0.08em;
  }

  .status-value {
    display: flex;
    align-items: center;
    gap: 0.3rem;
    font-family: monospace;
    font-size: 0.7rem;
  }

  .status-value.muted { color: var(--text-muted); }
  .status-value.error { color: var(--red); }

  .dot-sm {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    display: inline-block;
  }

  .dot-sm.green { background: var(--green); box-shadow: 0 0 4px var(--green-glow); }
  .dot-sm.yellow { background: var(--yellow); box-shadow: 0 0 4px rgba(247, 211, 84, 0.4); }
  .dot-sm.red { background: var(--red); box-shadow: 0 0 4px var(--accent-glow); }

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
    background: rgba(0, 0, 0, 0.7);
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
  .modal-close:hover {
    color: var(--accent);
    box-shadow: none;
  }

  .browse-path {
    padding: 0.5rem 1rem;
    font-size: 0.7rem;
    color: var(--text-muted);
    font-family: monospace;
    border-bottom: 1px solid rgba(255,255,255,0.04);
    word-break: break-all;
    background: var(--bg);
  }

  .browse-list {
    overflow-y: auto;
    flex: 1;
    padding: 0.4rem;
  }

  .browse-item {
    display: block;
    width: 100%;
    text-align: left;
    padding: 0.35rem 0.8rem;
    border: none;
    background: none;
    color: var(--text);
    font-size: 0.8rem;
    border-radius: 4px;
  }
  .browse-item:hover {
    background: var(--bg-card);
    box-shadow: none;
  }
  .browse-item.dir { color: var(--accent); font-weight: 600; }
  .browse-item.file { font-weight: 500; }
  .browse-item.disabled { color: var(--text-muted); opacity: 0.5; }
</style>
