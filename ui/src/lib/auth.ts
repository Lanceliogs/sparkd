export type Role = 'admin' | 'live' | null;

const STORAGE_KEY = 'spark-token';
const ROLE_KEY = 'spark-role';

let cachedToken: string | null = null;
let cachedRole: Role = null;
let onAuthLost: (() => void) | null = null;

export function initAuth(): { token: string | null; role: Role } {
  // 1. Check meta tag (localhost injection)
  const metaToken = document.querySelector<HTMLMetaElement>('meta[name="spark-token"]')?.content;
  const metaRole = document.querySelector<HTMLMetaElement>('meta[name="spark-role"]')?.content;
  if (metaToken) {
    localStorage.setItem(STORAGE_KEY, metaToken);
    if (metaRole) localStorage.setItem(ROLE_KEY, metaRole);
    cachedToken = metaToken;
    cachedRole = (metaRole as Role) || 'admin';
    return { token: cachedToken, role: cachedRole };
  }

  // 2. Check URL param
  const params = new URLSearchParams(window.location.search);
  const urlToken = params.get('token');
  if (urlToken) {
    localStorage.setItem(STORAGE_KEY, urlToken);
    cachedToken = urlToken;
    cachedRole = null; // resolved later via /api/auth/role
    params.delete('token');
    const newUrl = window.location.pathname + (params.toString() ? '?' + params.toString() : '');
    history.replaceState(null, '', newUrl);
    return { token: cachedToken, role: cachedRole };
  }

  // 3. Check localStorage
  const storedToken = localStorage.getItem(STORAGE_KEY);
  if (storedToken) {
    cachedToken = storedToken;
    cachedRole = (localStorage.getItem(ROLE_KEY) as Role) || null;
    return { token: cachedToken, role: cachedRole };
  }

  // 4. No token found
  cachedToken = null;
  cachedRole = null;
  return { token: null, role: null };
}

export async function resolveRole(): Promise<Role> {
  if (!cachedToken) return null;
  if (cachedRole) return cachedRole;
  try {
    const res = await fetch('/api/auth/role', {
      headers: { Authorization: `Bearer ${cachedToken}` }
    });
    if (res.ok) {
      const data = await res.json();
      cachedRole = data.role as Role;
      if (cachedRole) localStorage.setItem(ROLE_KEY, cachedRole);
      return cachedRole;
    }
    clearToken();
    return null;
  } catch {
    return null;
  }
}

export function getToken(): string | null {
  return cachedToken;
}

export function getRole(): Role {
  return cachedRole;
}

export function setToken(token: string): void {
  cachedToken = token;
  cachedRole = null;
  localStorage.setItem(STORAGE_KEY, token);
  localStorage.removeItem(ROLE_KEY);
}

export function clearToken(): void {
  cachedToken = null;
  cachedRole = null;
  localStorage.removeItem(STORAGE_KEY);
  localStorage.removeItem(ROLE_KEY);
  onAuthLost?.();
}

export function onAuthLostCallback(cb: () => void): void {
  onAuthLost = cb;
}
