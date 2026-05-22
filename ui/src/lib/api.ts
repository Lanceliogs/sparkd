const BASE = '';

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
  await fetch(`${BASE}/api/engine/start`, { method: 'POST' });
}

export async function engineStop(): Promise<void> {
  await fetch(`${BASE}/api/engine/stop`, { method: 'POST' });
}

export async function setBlackout(enabled: boolean): Promise<void> {
  await fetch(`${BASE}/api/engine/blackout`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ enabled }),
  });
}

export async function activateScene(id: string): Promise<void> {
  await fetch(`${BASE}/api/scenes/${id}/activate`, { method: 'POST' });
}

export async function releaseScene(id: string): Promise<void> {
  await fetch(`${BASE}/api/scenes/${id}/release`, { method: 'POST' });
}

export async function reloadProject(path?: string): Promise<void> {
  await fetch(`${BASE}/api/project/reload`, {
    method: 'POST',
    headers: path ? { 'Content-Type': 'application/json' } : {},
    body: path ? JSON.stringify({ path }) : undefined,
  });
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
  const res = await fetch(`${BASE}/api/editor/status`);
  return res.json();
}

export async function editorOpen(path: string): Promise<void> {
  await fetch(`${BASE}/api/editor/open`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ path }),
  });
}

export async function editorClose(): Promise<void> {
  await fetch(`${BASE}/api/editor/close`, { method: 'POST' });
}

export async function editorSave(): Promise<void> {
  await fetch(`${BASE}/api/editor/save`, { method: 'POST' });
}

export async function editorGetFixtures(): Promise<EditorFixture[]> {
  const res = await fetch(`${BASE}/api/editor/fixtures`);
  return res.json();
}

export async function editorAddFixture(fixture: Omit<EditorFixture, 'index'>): Promise<void> {
  await fetch(`${BASE}/api/editor/fixtures`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(fixture),
  });
}

export async function editorUpdateFixture(index: number, fixture: Omit<EditorFixture, 'index'>): Promise<void> {
  await fetch(`${BASE}/api/editor/fixtures/${index}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(fixture),
  });
}

export async function editorDeleteFixture(index: number): Promise<void> {
  await fetch(`${BASE}/api/editor/fixtures/${index}`, { method: 'DELETE' });
}

export async function editorGetBanks(): Promise<EditorBank[]> {
  const res = await fetch(`${BASE}/api/editor/banks`);
  return res.json();
}

export async function editorBankAddFixture(bankIndex: number, fixture: Omit<BankFixture, 'index'>): Promise<void> {
  await fetch(`${BASE}/api/editor/banks/${bankIndex}/fixtures`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(fixture),
  });
}

export async function editorBankUpdateFixture(bankIndex: number, fixIndex: number, fixture: Omit<BankFixture, 'index'>): Promise<void> {
  await fetch(`${BASE}/api/editor/banks/${bankIndex}/fixtures/${fixIndex}`, {
    method: 'PUT',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(fixture),
  });
}

export async function editorBankDeleteFixture(bankIndex: number, fixIndex: number): Promise<void> {
  await fetch(`${BASE}/api/editor/banks/${bankIndex}/fixtures/${fixIndex}`, { method: 'DELETE' });
}

export async function editorBankSave(bankIndex: number): Promise<void> {
  await fetch(`${BASE}/api/editor/banks/${bankIndex}/save`, { method: 'POST' });
}

export async function editorGetBankDirs(): Promise<string[]> {
  const res = await fetch(`${BASE}/api/editor/bank-dirs`);
  return res.json();
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
  const res = await fetch(`${BASE}/api/editor/browse?path=${encodeURIComponent(path)}`);
  return res.json();
}
