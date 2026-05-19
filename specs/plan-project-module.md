# Plan: Project Module

## Goal

Introduce a project loading step that decouples fixture/scene population from
engine start. The engine exposes `spark_engine_load_project(path)` which
delegates to `spark_project_load()` in a dedicated module. This creates a clean
boundary for the future YAML loader.

## Lifecycle

```text
spark_engine_init()
spark_engine_load_project(path)   <- resets, loads, resolves
spark_engine_start(cfg)           <- DMX + MIDI only
...
spark_engine_stop()
spark_engine_load_project(path)   <- reload with new project
spark_engine_start(cfg)
...
spark_engine_destroy()
```

`spark_engine_start` requires a mapping to have been loaded. If called without
a prior `load_project`, it returns an error.

## Module: `project.h` / `project.c`

### Interface

```c
int spark_project_load(const char *path);
```

### Responsibilities

1. `spark_fixture_reset()` + `spark_scene_reset()` — clear previous mapping
2. Parse the project file at `path`
3. For each fixture: call `spark_fixture_add()`
4. For each scene def: call `spark_scene_add_def()`
5. `spark_scene_resolve()` — resolve all defs into runtime scene table

### Temporary fallback

While the YAML parser is not implemented, `spark_project_load(NULL)` loads a
hardcoded test mapping (the current scene from main.c). This keeps the daemon
runnable without a project file during development.

Once libyaml is wired in, this fallback is removed and NULL path becomes an
error.

## Engine changes

### `engine.h`

```c
int spark_engine_load_project(const char *path);
```

### `engine.c`

- Add `static bool s_mapping_loaded = false;`
- `spark_engine_load_project`: call `spark_project_load(path)`, set flag
- `spark_engine_start`: check `s_mapping_loaded`, return `-1` if not set
- Remove `s_resolve_scenes()` — resolution now lives in project_load

## CLI changes (`main.c`)

- Add `--project PATH` to `spark_args_t`
- Call `spark_engine_load_project(args.project)` between `init` and `start`
- Remove inline scene_values/scene_def from main.c

## Build

- Add `project.o` to daemon Makefile

## Future: YAML loading

When libyaml is vendored and wired in, `spark_project_load` will:

1. Open file at path (support both single-file `.spark.yaml` and directory mode
   with `project.yaml` manifest)
2. Parse YAML into intermediate structs
3. Validate (per spec section 16)
4. Call `spark_fixture_add` / `spark_scene_add_def` for each entry
5. Call `spark_scene_resolve`

The project module is the only place that knows about YAML. Fixture and scene
modules remain format-agnostic.

## Future: HTTP reload

The HTTP handler for `POST /api/project/reload` will:

```text
spark_engine_stop()
spark_engine_load_project(new_path)
spark_engine_start(cfg)
```

Same sequence as initial startup, just triggered at runtime.
