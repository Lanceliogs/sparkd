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
    type EngineState,
    type SceneDef,
  } from '../lib/api';

  let state: EngineState = $state({ running: false, blackout: false, project: '' });
  let scenes: SceneDef[] = $state([]);
  let activeScenes: Set<string> = $state(new Set());
  let connected = $state(false);
  let ws: WebSocket | null = null;
  let reconnectTimer: ReturnType<typeof setTimeout>;

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

  onMount(() => {
    connect();
    loadScenes();
    return () => {
      clearTimeout(reconnectTimer);
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
    {/if}
  {/if}
</section>

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
    border: 2px solid transparent;
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
</style>
