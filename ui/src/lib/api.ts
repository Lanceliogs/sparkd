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
