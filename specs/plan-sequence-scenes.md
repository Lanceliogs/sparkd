# Plan: Sequence Scenes

## Goal

Implement the `SPARK_SCENE_SEQUENCE` output mode. A sequence scene has multiple
timed steps, each with its own set of DMX values. The scene advances through
steps based on elapsed time since activation, with optional looping and
transitions.

## Data structures (already defined in scene.h)

```c
typedef struct {
    uint32_t duration_ms;
    spark_scene_value_t *values;
    uint8_t value_count;
} spark_scene_step_t;

typedef struct {
    spark_scene_output_mode_t mode;
    spark_scene_value_t *values;    // static
    uint8_t value_count;
    spark_scene_step_t *steps;      // sequence
    uint8_t step_count;
    bool loop;
} spark_scene_output_t;
```

Runtime state already exists on `spark_scene_t`:
- `start_time_ms` — captured at activation, used to compute current phase

## Changes

### 1. Resolution: `s_resolve_sequence` in `scene.c`

Parallel to `s_resolve_static`. For each step def:

1. Resolve all value defs (fixture+channel -> dmx_index), same logic as static
2. Allocate resolved values from `s_value_arena`
3. Allocate a `spark_scene_step_t` from `s_step_arena`
4. Copy duration + resolved values pointer into the step

Wire into `spark_scene_resolve`:

```c
if (def->output_mode == SPARK_SCENE_SEQUENCE)
    rc = s_resolve_sequence(def, scene);
```

Set `scene->output.steps`, `scene->output.step_count`, `scene->output.loop`.

### 2. Scene def loading: step defs in `spark_scene_add_def`

The `spark_scene_step_def_t` has a `values` pointer that also needs deep-copying.
Add a step def arena or reuse the value_def arena for the step's value arrays.

Option A (simpler): each step's value_def array is allocated from the same
`s_value_def_arena`. The step_def structs themselves are small and few — store
them inline in the scene_def or add a small step_def arena.

Option B: add `SPARK_SCENE_STEP_DEF_ARENA_SIZE` in consts.h.

Recommendation: Option A — value defs already have an arena, step defs are just
a handful per scene. Add a `spark_scene_step_def_t` arena
(`s_step_def_arena[SPARK_SCENE_STEP_DEF_ARENA_SIZE]`) for the step structs, and
allocate each step's values from `s_value_def_arena`.

### 3. Rendering: sequence output in `stage.c`

In `spark_stage_render`, add the sequence branch alongside static:

```c
if (scene->output.mode == SPARK_SCENE_SEQUENCE)
{
    uint64_t elapsed = now_ms - scene->start_time_ms;
    // determine current step index from elapsed time
    // apply step values to frame
}
```

#### Step phase calculation

```c
static int s_sequence_current_step(spark_scene_output_t *out, uint64_t elapsed_ms)
{
    uint64_t total_ms = 0;
    for (uint8_t i = 0; i < out->step_count; i++)
        total_ms += out->steps[i].duration_ms;

    if (out->loop)
        elapsed_ms = elapsed_ms % total_ms;
    else if (elapsed_ms >= total_ms)
        return out->step_count - 1;  // hold last step

    uint64_t acc = 0;
    for (uint8_t i = 0; i < out->step_count; i++)
    {
        acc += out->steps[i].duration_ms;
        if (elapsed_ms < acc)
            return i;
    }
    return out->step_count - 1;
}
```

#### Hold transition (v1)

For hold transition, just apply the current step's values directly (same as
static values). No interpolation.

#### Linear transition (future, not v1)

Interpolate between current step values and next step values based on position
within the step duration. Requires matching value targets across steps.

For v1, all transitions are implicitly "hold". The transition field can be
parsed and stored but ignored at render time.

### 4. Auto-deactivation (`max-active-ms`)

Optional field on sequence scenes. If set, the scene auto-deactivates after the
specified duration regardless of trigger state.

Check in the render loop or in a dedicated tick function:

```c
if (scene->max_active_ms > 0 && elapsed >= scene->max_active_ms)
    spark_scene_deactivate(scene);
```

This can be deferred to after basic sequencing works.

### 5. Monotonic time dependency

`spark_stage_render` needs access to current time for elapsed calculation.
Options:

- Pass `now_ms` as a parameter to `spark_stage_render`
- Call `spark_clock_monotonic_ms()` inside render

The DMX output thread already runs on a timer. Passing `now_ms` in is cleaner
(testable, no hidden dependency). Update the signature:

```c
void spark_stage_render(spark_stage_t *stage, uint64_t now_ms, uint8_t out[SPARK_DMX_UNIVERSE_SIZE]);
```

The DMX output thread captures `now_ms` before locking and passes it in.

### 6. Consts

Already defined:
- `SPARK_SCENE_STEP_ARENA_SIZE 256` — for resolved steps

May need to add:
- `SPARK_SCENE_STEP_DEF_ARENA_SIZE` — for step def deep-copies during loading

## Implementation order

1. Add `now_ms` parameter to `spark_stage_render` (update stage.h, stage.c,
   dmx_out.c caller)
2. Implement `s_resolve_sequence` in scene.c
3. Add step def deep-copy support to `spark_scene_add_def`
4. Add sequence rendering branch in `spark_stage_render`
5. Test with a hardcoded two-step looping sequence (blink)

## Test case

A two-step blink (from the spec):

```text
Step 0: 80ms — dimmer=255, r=255, g=255, b=255
Step 1: 80ms — dimmer=0
Loop: true
```

Expected: at 40Hz render rate (25ms per frame), the output alternates roughly
every 3 frames between full white and black.
