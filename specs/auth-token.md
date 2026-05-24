# Auth Token Spec (v0.2)

## Overview

spark-ui is the only process exposed on the network. sparkd stays on localhost.
A UUID token gates all HTTP and WebSocket access to spark-ui.

## Token Lifecycle

1. On startup, spark-ui generates two UUID v4 tokens:
   - **Admin token**: full access (editor, file browser, load project, everything)
   - **Live token**: restricted to live control only (start, stop, blackout, pads)
2. Tokens are printed to stdout:
   ```
   Admin token: 550e8400-e29b-...
   Live token:  7c9e6679-a1f4-...
   ```
3. Env overrides: `SPARK_UI_ADMIN_TOKEN`, `SPARK_UI_LIVE_TOKEN`
4. Tokens live in memory only — never written to disk unless user sets them in `.spark.env`
5. Both tokens can be rotated independently via `POST /api/auth/rotate` (requires admin token)
6. On rotation, new UUIDs are generated; existing sessions with old tokens are invalidated

## Authentication

Every request to spark-ui must carry a token:

- **HTTP**: `Authorization: Bearer <token>` header
- **WebSocket**: `?token=<token>` query parameter on the upgrade request

If missing or invalid: respond `401 Unauthorized` with JSON `{"error": "unauthorized"}`.
If valid but insufficient permissions: respond `403 Forbidden` with JSON `{"error": "forbidden"}`.

Exception: `GET /` and static assets are served without auth (the HTML/CSS/JS is not secret).
The token is only enforced on `/api/*` and `/ws` routes.

## Permission Levels

Two roles: **admin** (full access) and **live** (show control only).

### Live token allows:

| Route | Description |
|-------|-------------|
| `GET /api/engine/state` | Engine state |
| `POST /api/engine/start` | Start engine |
| `POST /api/engine/stop` | Stop engine |
| `POST /api/engine/blackout` | Toggle blackout |
| `POST /api/engine/midi/reconnect` | Reconnect MIDI |
| `POST /api/project/reload` (no path body) | Reload current project |
| `GET /api/scenes` | List scenes |
| `POST /api/scenes/{id}/activate` | Trigger scene |
| `POST /api/scenes/{id}/release` | Release scene |
| `GET /api/midi/status` | MIDI status |
| `GET /api/dmx/status` | DMX status |
| `/ws` | WebSocket (state, scene events) |

### Live token denies (admin only):

| Route | Reason |
|-------|--------|
| `POST /api/project/reload` (with path) | Filesystem access |
| `/api/editor/*` | Project editing |
| `/api/editor/browse/*` | Filesystem browsing |
| `POST /api/auth/rotate` | Token management |

### Frontend behavior with live token:

- Hide the Editor tab entirely
- Hide "Load Project" button (which opens the file browser)
- Show only: Start, Stop, Blackout, Reload (current project), and scene pads
- On 403 response: show "Insufficient permissions" message (not a login prompt)

## Localhost Injection

When serving `index.html` to a client connecting from `127.0.0.1`:
- Inject `<meta name="spark-token" content="<admin-uuid>">` into `<head>`
- Inject `<meta name="spark-role" content="admin">` into `<head>`
- This is safe because TCP prevents remote IP spoofing of loopback addresses

For non-localhost clients, the meta tags are not injected.

## Frontend Token Flow

On page load (in order of priority):

1. Check `<meta name="spark-token">` → if present, store in `localStorage`
2. Check URL `?token=<value>` → if present, store in `localStorage`, strip from URL via `history.replaceState`
3. Check `localStorage('spark-token')` → use if present
4. If none found → show "Enter access code" input

Once stored, all `fetch()` calls in `api.ts` add `Authorization: Bearer <token>` header.
WebSocket connects as `ws://host/ws?token=<token>`.

On 401 response: clear `localStorage`, show the access code input.

## QR Code Sharing

A "Share Access" button in the UI (admin only) opens a modal displaying two options:

**Share Live Control:**
- QR code encoding `http://<lan-ip>:<port>/?token=<live-uuid>`
- Gives remote user access to start/stop/blackout/pads only
- No editor, no file browser, no project loading

**Share Full Access:**
- QR code encoding `http://<lan-ip>:<port>/?token=<admin-uuid>`
- Full admin access (use with caution)

The LAN IP is detected server-side and exposed via `GET /api/auth/info` (no auth required):
```json
{"lan_ip": "192.168.1.50", "port": 7601}
```

## Token Rotation

`POST /api/auth/rotate` (requires admin token):
- Body: `{"level": "admin"}` or `{"level": "live"}` or `{}` (rotates both)
- Returns `{"admin_token": "<new-uuid>", "live_token": "<new-uuid>"}`
- Localhost admin session auto-recovers on next page load (meta injection)
- Remote sessions with rotated tokens get 401, must re-scan QR or re-enter token
- Useful as a "kick everyone off" button during a show

## Security Notes

- Token protects against unauthorized LAN access (e.g., random devices on show WiFi)
- Localhost IP trust for meta injection is safe (TCP handshake prevents spoofing)
- Not designed to protect against attackers with local machine access (they can read env/memory)
- HTTPS is not required for LAN-only use but could be added later via self-signed cert
