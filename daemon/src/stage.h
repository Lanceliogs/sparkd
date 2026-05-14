#ifndef SPARK_STAGE_H
#define SPARK_STAGE_H

#include "midi_event.h"
#include "consts.h"

#include <stdint.h>
#include <pthread.h>

typedef struct {
    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    pthread_mutex_t mutex;
    uint8_t blackout; // 0 or 1 for now, we might use a bool?
} spark_stage_t;

/* zero everything, init mutex */
void spark_stage_init(spark_stage_t *stage); 

/* destroy mutex */
void spark_stage_destroy(spark_stage_t *stage);

/* process a MIDI event through the mapping */
void spark_stage_apply_midi(spark_stage_t *stage, const midi_event_t *event);

/* copy the current frame (under lock) into a buffer for the DMX thread to send */
void spark_stage_render(spark_stage_t *stage, uint8_t out[SPARK_DMX_UNIVERSE_SIZE]);

#endif