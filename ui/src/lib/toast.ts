export type ToastType = 'success' | 'error' | 'warning';

export interface Toast {
  id: number;
  type: ToastType;
  message: string;
  ondetail?: () => void;
}

let nextId = 0;
let toasts: Toast[] = [];
let listeners: Array<(toasts: Toast[]) => void> = [];

function notify() {
  for (const fn of listeners) fn([...toasts]);
}

function addToast(type: ToastType, message: string, ondetail?: () => void, durationMs = 4000) {
  const id = nextId++;
  toasts = [...toasts, { id, type, message, ondetail }];
  notify();

  const timeout = type === 'error' ? Math.max(durationMs, 6000) : durationMs;
  setTimeout(() => removeToast(id), timeout);
}

export function removeToast(id: number) {
  toasts = toasts.filter(t => t.id !== id);
  notify();
}

export function subscribeToasts(fn: (toasts: Toast[]) => void): () => void {
  listeners.push(fn);
  fn([...toasts]);
  return () => { listeners = listeners.filter(l => l !== fn); };
}

export function showError(message: string, ondetail?: () => void) { addToast('error', message, ondetail); }
export function showWarning(message: string, ondetail?: () => void) { addToast('warning', message, ondetail); }
export function showSuccess(message: string, ondetail?: () => void) { addToast('success', message, ondetail); }
