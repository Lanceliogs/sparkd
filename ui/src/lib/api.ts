const BASE = '';

export class ApiError extends Error {
  status: number;
  code: string;

  constructor(status: number, code: string, message: string) {
    super(message);
    this.status = status;
    this.code = code;
  }
}

async function assertOk(res: Response): Promise<void> {
  if (res.ok) return;
  let code = 'unknown';
  let msg = `Request failed (${res.status})`;
  try {
    const body = await res.json();
    if (body.error) code = body.error;
    if (body.message) msg = body.message;
    else if (body.error) msg = body.error;
  } catch { /* ignore parse failures */ }
  throw new ApiError(res.status, code, msg);
}

export interface EngineState {
  running: boolean;
  blackout: boolean;
  project: string;
  active_scenes?: string[];
}

export interface SceneDef {
  id: string;
  name: string;
  type: 'static' | 'sequence';
  trigger_mode: 'gate' | 'toggle';
  channel: number;
  note: number;
  enabled: boolean;
}

export async function getEngineState(): Promise<EngineState> {
  const res = await fetch(`${BASE}/api/engine/state`);
  return res.json();
}

export async function getScenes(): Promise<SceneDef[]> {
  const res = await fetch(`${BASE}/api/scenes`);
  return res.json();
}

export async function engineStart(): Promise<void> {
  const res = await fetch(`${BASE}/api/engine/start`, { method: 'POST' });
  await assertOk(res);
}

export async function engineStop(): Promise<void> {
  const res = await fetch(`${BASE}/api/engine/stop`, { method: 'POST' });
  await assertOk(res);
}

export async function setBlackout(enabled: boolean): Promise<void> {
  const res = await fetch(`${BASE}/api/engine/blackout`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ enabled }),
  });
  await assertOk(res);
}

export async function activateScene(id: string): Promise<void> {
  const res = await fetch(`${BASE}/api/scenes/${id}/activate`, { method: 'POST' });
  await assertOk(res);
}

export async function releaseScene(id: string): Promise<void> {
  const res = await fetch(`${BASE}/api/scenes/${id}/release`, { method: 'POST' });
  await assertOk(res);
}

export async function reloadProject(path?: string): Promise<void> {
  await fetch(`${BASE}/api/project/reload`, {
    method: 'POST',
    headers: path ? { 'Content-Type': 'application/json' } : {},
    body: path ? JSON.stringify({ path }) : undefined,
  });
}

/* ---- MIDI / DMX Status ---- */

export interface MidiPortStatus {
  pattern: string;
  connected: boolean;
  last_activity_ms: number;
}

export interface MidiStatus {
  port_count: number;
  ports: MidiPortStatus[];
}

export interface DmxStatus {
  backend: string;
  state: string;
  stats: { frames_sent: number; write_errors: number; reconnects: number };
}

export async function getMidiStatus(): Promise<MidiStatus> {
  try {
    const res = await fetch(`${BASE}/api/midi/status`);
    if (!res.ok) return { port_count: 0, ports: [] };
    return await res.json();
  } catch { return { port_count: 0, ports: [] }; }
}

export async function getDmxStatus(): Promise<DmxStatus> {
  try {
    const res = await fetch(`${BASE}/api/dmx/status`);
    if (!res.ok) return { backend: 'none', state: 'disconnected', stats: { frames_sent: 0, write_errors: 0, reconnects: 0 } };
    return await res.json();
  } catch { return { backend: 'none', state: 'disconnected', stats: { frames_sent: 0, write_errors: 0, reconnects: 0 } }; }
}

/* ---- Editor API ---- */

export interface EditorStatus {
  project_loaded: boolean;
  project_path: string;
  dirty: boolean;
  fixture_count: number;
  bank_count: number;
}

export interface Channel {
  name: string;
  offset: number;
}

export interface EditorFixture {
  index?: number;
  id: string;
  name: string;
  start_address: number;
  channel_count: number;
  template: string;
  copy_from: string;
  channels: Channel[];
}

export interface BankFixture {
  index?: number;
  id: string;
  name: string;
  channel_count: number;
  channels: Channel[];
}

export interface EditorBank {
  index: number;
  id: string;
  path: string;
  version: number;
  dirty: boolean;
  fixtures: BankFixture[];
}

export async function editorStatus(): Promise<EditorStatus> {
  try {
    const res = await fetch(`${BASE}/api/editor/status`);
    if (!res.ok) return { project_loaded: false, project_path: '', dirty: false, fixture_count: 0, bank_count: 0 };
    return await res.json();
  } catch { return { project_loaded: false, project_path: '', dirty: false, fixture_count: 0, bank_count: 0 }; }
}

export async function editorOpen(path: string): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/open`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path }),
  });
  await assertOk(res);
}

export async function editorClose(): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/close`, { method: 'POST' });
  await assertOk(res);
}

export async function editorSave(): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/save`, { method: 'POST' });
  await assertOk(res);
}

export async function editorGetFixtures(): Promise<EditorFixture[]> {
  try {
    const res = await fetch(`${BASE}/api/editor/fixtures`);
    if (!res.ok) return [];
    return await res.json();
  } catch { return []; }
}

export async function editorAddFixture(fixture: Omit<EditorFixture, 'index'>): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/fixtures`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(fixture),
  });
  await assertOk(res);
}

export async function editorUpdateFixture(index: number, fixture: Omit<EditorFixture, 'index'>): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/fixtures/${index}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(fixture),
  });
  await assertOk(res);
}

export async function editorDeleteFixture(index: number): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/fixtures/${index}`, { method: 'DELETE' });
  await assertOk(res);
}

export async function editorGetBanks(): Promise<EditorBank[]> {
  try {
    const res = await fetch(`${BASE}/api/editor/banks`);
    if (!res.ok) return [];
    return await res.json();
  } catch { return []; }
}

export async function editorBankAddFixture(bankIndex: number, fixture: Omit<BankFixture, 'index'>): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/banks/${bankIndex}/fixtures`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(fixture),
  });
  await assertOk(res);
}

export async function editorBankUpdateFixture(bankIndex: number, fixIndex: number, fixture: Omit<BankFixture, 'index'>): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/banks/${bankIndex}/fixtures/${fixIndex}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(fixture),
  });
  await assertOk(res);
}

export async function editorBankDeleteFixture(bankIndex: number, fixIndex: number): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/banks/${bankIndex}/fixtures/${fixIndex}`, { method: 'DELETE' });
  await assertOk(res);
}

export async function editorBankSave(bankIndex: number): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/banks/${bankIndex}/save`, { method: 'POST' });
  await assertOk(res);
}

export async function editorGetBankDirs(): Promise<string[]> {
  try {
    const res = await fetch(`${BASE}/api/editor/bank-dirs`);
    if (!res.ok) return [];
    return await res.json();
  } catch { return []; }
}

export async function editorCreateBank(id: string, directory: string): Promise<void> {
  await fetch(`${BASE}/api/editor/banks/create`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ id, directory }),
  });
}

export interface BrowseResult {
  path: string;
  entries: { name: string; type: 'file' | 'dir' }[];
}

export async function editorBrowse(path: string): Promise<BrowseResult> {
  try {
    const res = await fetch(`${BASE}/api/editor/browse?path=${encodeURIComponent(path)}`);
    if (!res.ok) return { path: path || '.', entries: [] };
    return await res.json();
  } catch { return { path: path || '.', entries: [] }; }
}

export interface BrowseRoots {
  places: { label: string; path: string }[];
  drives: { label: string; path: string }[];
}

export async function editorBrowseRoots(): Promise<BrowseRoots> {
  try {
    const res = await fetch(`${BASE}/api/editor/browse/roots`);
    if (!res.ok) return { places: [], drives: [] };
    return await res.json();
  } catch { return { places: [], drives: [] }; }
}

/* ---- Save As ---- */

export async function editorSaveAs(path: string): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/save-as`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path }),
  });
  await assertOk(res);
}

/* ---- Fixtures Sort ---- */

export async function editorFixturesSort(): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/fixtures/sort`, { method: 'POST' });
  await assertOk(res);
}

/* ---- Hardware Config ---- */

export interface HardwareConfig {
  midi_device: string;
  midi_mode: string;
  dmx_device: string;
  dmx_backend: string;
  dmx_refresh_hz: number;
}

export async function editorGetHardware(): Promise<HardwareConfig> {
  try {
    const res = await fetch(`${BASE}/api/editor/hardware`);
    if (!res.ok) return { midi_device: '', midi_mode: '', dmx_device: '', dmx_backend: '', dmx_refresh_hz: 0 };
    return await res.json();
  } catch { return { midi_device: '', midi_mode: '', dmx_device: '', dmx_backend: '', dmx_refresh_hz: 0 }; }
}

export async function editorUpdateHardware(hw: HardwareConfig): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/hardware`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(hw),
  });
  await assertOk(res);
}

/* ---- Editor Scenes ---- */

export interface SceneValue {
  target: string;
  value: number;
}

export interface SceneStep {
  duration_ms: number;
  transition: string;
  values: SceneValue[];
}

export interface EditorScene {
  index?: number;
  id: string;
  name: string;
  type: 'static' | 'sequence';
  trigger_mode: 'gate' | 'toggle';
  channel: number;
  note: number;
  enabled: boolean;
  loop: boolean;
  values: SceneValue[];
  steps: SceneStep[];
}

export async function editorGetScenes(): Promise<EditorScene[]> {
  try {
    const res = await fetch(`${BASE}/api/editor/scenes`);
    if (!res.ok) return [];
    return await res.json();
  } catch { return []; }
}

export async function editorAddScene(scene: Omit<EditorScene, 'index'>): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/scenes`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(scene),
  });
  await assertOk(res);
}

export async function editorUpdateScene(index: number, scene: Omit<EditorScene, 'index'>): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/scenes/${index}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(scene),
  });
  await assertOk(res);
}

export async function editorDeleteScene(index: number): Promise<void> {
  const res = await fetch(`${BASE}/api/editor/scenes/${index}`, { method: 'DELETE' });
  await assertOk(res);
}
