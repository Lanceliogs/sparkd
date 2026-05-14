#include "stage.h"

#include <string.h>

void spark_stage_init(spark_stage_t *stage)
{
    stage->blackout = 0;
    memset(stage->frame, 0, SPARK_DMX_UNIVERSE_SIZE);
    pthread_mutex_init(&stage->mutex, NULL); // For now, I guess it's enough
}

void spark_stage_destroy(spark_stage_t *stage)
{
    pthread_mutex_destroy(&stage->mutex);
}

void spark_stage_apply_midi(spark_stage_t *stage, const midi_event_t *event)
{
    pthread_mutex_lock(&stage->mutex);

    /* Check event against the event registry or something */
    /* Enable or disable scenes */
    /* Hardcoded mapping for now */

    if (event->type == SPARK_MIDI_NOTE_ON && event->channel == 0 && event->note == 60)
    {
        if (event->velocity == 0)
            stage->frame[0] = 0;
        else
            stage->frame[0] = (event->velocity * 255) / 127;
    }
    else if (event->type == SPARK_MIDI_NOTE_OFF && event->channel == 0 && event->note == 60)
    {
        stage->frame[0] = 0;
    }

    pthread_mutex_unlock(&stage->mutex);
}

/* For now we go the easy way, we just lock as soon as possible and copy */
void spark_stage_render(spark_stage_t *stage, uint8_t out[SPARK_DMX_UNIVERSE_SIZE])
{
    pthread_mutex_lock(&stage->mutex);
    memcpy(out, stage->frame, SPARK_DMX_UNIVERSE_SIZE);
    pthread_mutex_unlock(&stage->mutex);
}