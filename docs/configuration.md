# Configuration

sparkd is configured through environment variables and a `.spark.env` file. No config file is strictly required — sensible defaults are used when nothing is set.

## The .spark.env File

sparkd looks for a `.spark.env` file at startup in this order:

1. Current working directory (`./.spark.env`)
2. Home directory (`~/.spark/.spark.env`)

The first file found is loaded. Values are simple `KEY=VALUE` pairs, one per line:

```bash
SPARK_PROJECT_PATH=/home/user/shows/main.spark.yaml
SPARK_FIXTURE_BANK_PATH=/home/user/shows/banks
SPARK_HTTP_ADDR=127.0.0.1:7600
SPARK_UI_HTTP_ADDR=0.0.0.0:7601
```

Lines starting with `#` are comments. Empty lines are ignored.

### Priority

Real environment variables always override `.spark.env` values. CLI arguments override both.

```
CLI argument > environment variable > .spark.env > built-in default
```

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `SPARK_PROJECT_PATH` | none | Project file to load at startup |
| `SPARK_HTTP_ADDR` | `127.0.0.1:7600` | Daemon HTTP listen address |
| `SPARK_UI_HTTP_ADDR` | `127.0.0.1:7601` | UI server listen address |
| `SPARK_FIXTURE_BANK_PATH` | `~/.spark/fixtures/` | Fixture bank search directories (`;`-separated) |
| `SPARK_PROJECT_ROOTS` | none | Custom file browser roots (see below) |
| `SPARK_AUTH_TOKEN` | none | Static auth token for HTTP API access |

## Fixture Banks

Fixture banks are YAML files that define reusable fixture templates (channel layouts). They save you from manually defining channels every time you add a fixture.

### Bank directory structure

```
~/.spark/fixtures/
  stairville.yaml       # bank file for Stairville fixtures
  chauvet.yaml          # bank file for Chauvet fixtures
  custom/
    my-lights.yaml      # your own custom definitions
```

Each `.yaml` file in the search path is scanned. Files must have a `bank:` section with a unique `id`.

### Search paths

By default, sparkd looks in `~/.spark/fixtures/`. To add more directories:

```bash
SPARK_FIXTURE_BANK_PATH=/path/to/banks;/another/path
```

Separate multiple paths with `;` (works on both Windows and Linux).

### Bank file format

```yaml
bank:
  id: my-brand
  version: 1

fixtures:
  - id: par-8ch
    channel-count: 8
    channels:
      - name: dimmer
        offset: 0
      - name: red
        offset: 1
      - name: green
        offset: 2
      - name: blue
        offset: 3
      - name: white
        offset: 4
      - name: mode
        offset: 5
      - name: color
        offset: 6
      - name: strobe
        offset: 7
```

Reference templates in your project as `bank-id:fixture-id` (e.g. `my-brand:par-8ch`).

## Project Roots

By default, the file browser sidebar shows common Places (Home, Documents, Desktop) and Drives. You can add custom project directories for quick access:

```bash
SPARK_PROJECT_ROOTS=Shows@/home/user/shows;Templates@/home/user/templates
```

Format: `Label@/path;Label2@/path2`

- `@` separates the display label from the path
- `;` separates multiple entries
- Paths must exist (non-existent paths are silently ignored)

Custom roots appear in a **Projects** category at the top of the file browser sidebar.

## Network Access (LAN)

By default, sparkd listens on `127.0.0.1` (localhost only). This is intentional — the daemon is a local service, and spark-ui acts as the network-facing gateway by proxying API requests to it.

To allow access from other devices on your network (e.g. a phone for live control), you only need to expose spark-ui:

```bash
SPARK_UI_HTTP_ADDR=0.0.0.0:7601
```

This enables the **Share** feature in the UI, which generates a QR code for mobile access. spark-ui proxies `/api/*` requests to the daemon internally.

> **Note:** You *can* also expose sparkd directly with `SPARK_HTTP_ADDR=0.0.0.0:7600` if you have a reason to, but it's not the intended architecture.

## Authentication

Authentication is disabled by default. If you set `SPARK_AUTH_TOKEN`, all HTTP API requests will require a matching `Bearer` token:

```bash
SPARK_AUTH_TOKEN=my-secret-token
```

With no token set, the API is open (fine for localhost, be careful on `0.0.0.0`).

When a token is set, `sparkctl` reads it from the environment and sends it with every request. The Share feature appends the token to the QR code URL so mobile devices can authenticate without manual entry.

## Windows Paths

On Windows, use forward slashes or escaped backslashes in `.spark.env`:

```bash
SPARK_PROJECT_PATH=C:/Users/Me/shows/main.spark.yaml
SPARK_FIXTURE_BANK_PATH=C:/Users/Me/banks;D:/shared-banks
```

## Example .spark.env

```bash
# Project to load by default
SPARK_PROJECT_PATH=~/shows/current.spark.yaml

# Listen on all interfaces for LAN access
SPARK_HTTP_ADDR=127.0.0.1:7600
SPARK_UI_HTTP_ADDR=0.0.0.0:7601

# Fixture banks
SPARK_FIXTURE_BANK_PATH=~/shows/banks;/opt/sparkd/banks

# Quick access in file browser
SPARK_PROJECT_ROOTS=Shows@~/shows;Archive@~/shows/archive

# Authentication (optional — omit to disable auth entirely)
SPARK_AUTH_TOKEN=a1b2c3d4-e5f6-7890-abcd-ef1234567890
```
