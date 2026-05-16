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

typedef struct {
    spark_scene_trigger_mode_t trigger_mode;
    uint8_t channel;
    uint8_t note; /* note mode for gate and toggle */
} spark_scene_trigger_t;

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
    spark_scene_trigger_t trigger;
    /* Output */
    spark_scene_output_t output;

    /* Runtime */
    bool enabled;
    bool active;
    uint8_t velocity;
    uint64_t start_time_ms; // when activated, for sequence phase
} spark_scene_t;

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