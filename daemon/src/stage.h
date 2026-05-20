/*
 * stage.h - Show state and DMX frame rendering
 *
 * The stage is the central runtime state of the lighting engine. It owns
 * a mutex that protects scene state from concurrent access between the
 * main thread (MIDI input, HTTP) and the DMX output thread (rendering).
 *
 * Responsibilities:
 *   - Dispatch incoming MIDI events to scene activate/deactivate logic
 *   - Render the current 512-byte DMX frame from active scenes
 *   - Manage blackout state (output zeroed, scenes stay active internally)
 *
 * The mutex is held only for short CPU work (event dispatch, frame render).
 * No I/O occurs while holding the lock.
 */
#ifndef SPARK_STAGE_H
#define SPARK_STAGE_H

#include "midi_event.h"
#include "scene.h"
#include "consts.h"

#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    bool blackout;
} spark_stage_t;

/* zero everything, init mutex */
void spark_stage_init(spark_stage_t *stage); 

/* destroy mutex */
void spark_stage_destroy(spark_stage_t *stage);

/* process a MIDI event through the mapping */
void spark_stage_apply_midi(spark_stage_t *stage, const spark_midi_event_t *event);

/* copy the current frame (under lock) into a buffer for the DMX thread to send */
void spark_stage_render(spark_stage_t *stage, uint64_t now_ms, uint8_t out[SPARK_DMX_UNIVERSE_SIZE]);

/* Lock and set the mutex for a call from another thread */
void spark_stage_set_blackout(spark_stage_t *stage, bool value);

#endif
