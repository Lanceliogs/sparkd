/*
 * scene.h - Scene management for sparkd
 *
 * Scenes are the primary unit of lighting output. Each scene maps to a unique
 * slot in a [16 channels][128 notes] lookup table, addressed by MIDI channel
 * and note number. The position in the table IS the trigger address -- no
 * separate trigger channel/note fields are needed.
 *
 * Trigger modes:
 *   GATE   - note-on activates, note-off (or velocity 0) deactivates
 *   TOGGLE - note-on toggles active/inactive, note-off ignored
 *
 * Output modes:
 *   STATIC   - fixed DMX values applied while active
 *   SEQUENCE - timed steps with per-step values and durations
 *
 * Velocity scaling: individual values can be scaled by the triggering
 * note's velocity (0-127 mapped to 0-255). The velocity is captured at
 * activation time and held until release.
 *
 * Configuration vs runtime:
 *   Scene definitions (spark_scene_def_t) describe scenes at config time.
 *   Each value def carries either a raw DMX index or an unresolved
 *   fixture + channel name. The resolver (spark_scene_resolve) walks all
 *   defs, resolves fixture references via the fixture module, and copies
 *   the resolved values into a static arena. Unresolved values (fixture
 *   not found) are skipped and flagged via a `resolved` bool.
 *
 * Memory: resolved values and steps are bump-allocated from static
 * arenas in scene.c. spark_scene_reset() reclaims all arena memory.
 *
 * Lifecycle: scenes are stored in a module-level static array (BSS).
 * An active scene list (pointer array) tracks which scenes are currently
 * contributing to the DMX frame, enabling O(active_count) rendering
 * instead of scanning all 2048 slots.
 *
 * Thread safety: scene functions are NOT self-synchronized. The stage
 * module holds a mutex and calls into scene functions under lock.
 */
#ifndef SPARK_SCENE_H
#define SPARK_SCENE_H

#include "consts.h"

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SPARK_SCENE_GATE,
    SPARK_SCENE_TOGGLE,
} spark_scene_trigger_mode_t;

typedef enum {
    SPARK_SCENE_STATIC,
    SPARK_SCENE_SEQUENCE,
} spark_scene_output_mode_t;

// A single target/value (for static)
typedef struct {
    uint16_t dmx_index;
    uint8_t value;
    bool velocity_scaling;
} spark_scene_value_t;

// A sequence step
typedef struct {
    uint32_t duration_ms;
    // transition type (hold/linear) later?
    spark_scene_value_t *values;
    uint8_t value_count;
} spark_scene_step_t;

typedef struct {
    spark_scene_output_mode_t mode;
    /* static */
    spark_scene_value_t *values;
    uint8_t value_count;
    /* sequence */
    spark_scene_step_t *steps;
    uint8_t step_count;
    bool loop;
} spark_scene_output_t;

  // The scene itself
typedef struct {
    /* Metadata */
    const char *id;
    const char *name;
    const char *comment;
    /* Trigger */
    spark_scene_trigger_mode_t trigger_mode;
    /* Output */
    spark_scene_output_t output;

    /* Runtime */
    bool enabled;
    bool active;
    uint8_t velocity;
    uint64_t start_time_ms; // when activated, for sequence phase
} spark_scene_t;

/* ---- Configuration-time definitions (resolved at load) ---- */

typedef struct {
    uint16_t dmx_index;
    const char *fixture;
    const char *channel;
    uint8_t value;
    bool velocity_scaling;
    bool resolved;
} spark_scene_value_def_t;

typedef struct {
    uint32_t duration_ms;
    spark_scene_value_def_t *values;
    uint8_t value_count;
} spark_scene_step_def_t;

typedef struct {
    uint8_t channel;
    uint8_t note;
    const char *id;
    const char *name;
    spark_scene_trigger_mode_t trigger_mode;
    spark_scene_output_mode_t output_mode;
    spark_scene_value_def_t *values;
    uint8_t value_count;
    spark_scene_step_def_t *steps;
    uint8_t step_count;
    bool loop;
} spark_scene_def_t;

/* Scene storage access */
spark_scene_t *spark_scene_get_all(void);
spark_scene_t *spark_scene_get(uint8_t channel, uint8_t note);
void spark_scene_reset(void);

/* Active scene list */
spark_scene_t **spark_scene_get_active(uint16_t *count);

/* Scene lifecycle */
void spark_scene_activate(spark_scene_t *scene, uint8_t velocity);
void spark_scene_deactivate(spark_scene_t *scene);
void spark_scene_toggle(spark_scene_t *scene, uint8_t velocity);

#endif