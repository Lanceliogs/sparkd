<script lang="ts">
  import { onMount } from 'svelte';
  import PadGrid from '../components/PadGrid.svelte';
  import FileBrowser from '../components/FileBrowser.svelte';
  import StatusFooter from '../components/StatusFooter.svelte';
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
    type EngineState,
    type SceneDef,
    type MidiStatus,
    type DmxStatus,
    getEngineState,
  } from '../lib/api';
  import { showError } from '../lib/toast';

  let engine: EngineState = $state({ running: false, blackout: false, project: '' });
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

  function wsUrl(): string {
    const proto = location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${proto}//${location.host}/ws`;
  }

  function connect() {
    ws = new WebSocket(wsUrl());

    ws.onopen = () => {
      connected = true;
      loadScenes();
    };

    ws.onmessage = (ev) => {
      let msg;
      try { msg = JSON.parse(ev.data); }
      catch { return; }
      switch (msg.type) {
        case 'state':
          engine = { running: msg.running, blackout: msg.blackout, project: msg.project ?? '' };
          activeScenes = new Set(msg.active_scenes ?? []);
          break;
        case 'started':
          engine = { ...engine, running: true };
          break;
        case 'stopped':
          engine = { ...engine, running: false };
          break;
        case 'blackout':
          engine = { ...engine, blackout: msg.enabled };
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
    if (engine.running) startStatusPoll();
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
    try { await engineStart(); }
    catch (e: any) { showError(e.message || 'Start failed'); }
  }

  async function handleStop() {
    try { await engineStop(); }
    catch (e: any) { showError(e.message || 'Stop failed'); }
  }

  async function handleBlackout() {
    try { await setBlackout(!engine.blackout); }
    catch (e: any) { showError(e.message || 'Blackout toggle failed'); }
  }

  async function handleReload() {
    try {
      const oldState = await getEngineState();
      if (oldState.running) await engineStop();
      await reloadProject();
      if (oldState.running) await engineStart();
      loadScenes();
    } catch (e: any) { showError(e.message || 'Reload failed'); }
  }

  function getBrowserInitialPath(): string {
    if (!engine.project) return '';
    return engine.project.replace(/\\/g, '/').replace(/\/[^/]*$/, '');
  }

  async function handleBrowserConfirm(path: string) {
    showBrowser = false;
    try {
      await reloadProject(path);
      loadScenes();
    } catch (e: any) { showError(e.message || 'Failed to load project'); }
  }

  async function handlePadDown(ev: PointerEvent, scene: SceneDef) {
    try {
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
    } catch (e: any) { showError(e.message || 'Scene activation failed'); }
  }

  async function handlePadUp(ev: PointerEvent, scene: SceneDef) {
    try {
      if (scene.trigger_mode === 'gate') {
        (ev.currentTarget as HTMLElement).releasePointerCapture(ev.pointerId);
        await releaseScene(scene.id);
      }
    } catch (e: any) { showError(e.message || 'Scene release failed'); }
  }
</script>

<header class="status-bar">
  <div class="status-item">
    <span class="dot" class:green={connected && engine.running} class:yellow={connected && !engine.running} class:red={!connected}></span>
    {#if !connected}
      disconnected
    {:else if engine.running}
      running
    {:else}
      stopped
    {/if}
  </div>
  <div class="status-item">
    {#if engine.blackout}
      <span class="badge blackout">BLACKOUT</span>
    {/if}
  </div>
  <div class="status-item project">
    {engine.project || 'no project'}
  </div>
</header>

<section class="controls">
  {#if connected}
    {#if !engine.running}
      <button class="btn-start" onclick={handleStart}>Start</button>
    {:else}
      <button class="btn-stop" onclick={handleStop}>Stop</button>
    {/if}
    <button class="btn-blackout" class:active={engine.blackout} onclick={handleBlackout}>
      {engine.blackout ? 'Clear Blackout' : 'Blackout'}
    </button>
    <button class="btn-reload" onclick={handleReload}>Reload</button>
    {#if !engine.running}
      <button class="btn-load" onclick={() => showBrowser = true}>Load Project</button>
    {/if}
  {/if}
</section>

{#if engine.running}
  <StatusFooter {midiStatus} {dmxStatus} />
{/if}

<section class="pad-section">
  <PadGrid
    items={scenes.map((s, i) => ({ id: String(i), label: s.name, sublabel: s.type, active: activeScenes.has(s.id), dim: !s.enabled }))}
    columns="repeat(auto-fill, minmax(var(--pad-size), 1fr))"
    onactivate={(id, ev) => { const s = scenes[Number(id)]; if (s) handlePadDown(ev, s); }}
    onrelease={(id, ev) => { const s = scenes[Number(id)]; if (s) handlePadUp(ev, s); }}
  />
</section>

{#if showBrowser}
  <FileBrowser
    mode="open"
    initialPath={getBrowserInitialPath()}
    onconfirm={handleBrowserConfirm}
    oncancel={() => showBrowser = false}
  />
{/if}

<style>
  /* Status bar */
  .status-bar {
    display: flex;
    align-items: center;
    gap: 1.5rem;
    min-height: 2.5rem;
    padding: 0 1rem;
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

  /* Pad section */
  .pad-section {
    padding: 0.5rem;
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

</style>
