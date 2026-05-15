#include "stage.h"

#include <string.h>

typedef struct {
    bool active;
    uint8_t midi_channel;
    uint8_t midi_note;
    uint16_t dmx_channel;
    uint8_t dmx_value;
} hardcoded_scene_t;

static hardcoded_scene_t scene = {
    .active = false,
    .midi_channel = 0,
    .midi_note = 60,
    .dmx_channel = 0,
    .dmx_value = 0,
};

void spark_stage_init(spark_stage_t *stage)
{
    stage->blackout = false;
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

    if (event->type == SPARK_MIDI_NOTE_ON &&
        event->channel == scene.midi_channel &&
        event->note == scene.midi_note)
    {
        if (event->velocity == 0)
            scene.active = false;
        else
        {
            scene.dmx_value = (event->velocity * 255) / 127; 
            scene.active = true; 
        }
    }
    else if (event->type == SPARK_MIDI_NOTE_OFF &&
        event->channel == scene.midi_channel &&
        event->note == scene.midi_note)
    {
        scene.active = false;
    }

    pthread_mutex_unlock(&stage->mutex);
}

/* For now we go the easy way, we just lock as soon as possible and render */
void spark_stage_render(spark_stage_t *stage, uint8_t out[SPARK_DMX_UNIVERSE_SIZE])
{
    pthread_mutex_lock(&stage->mutex);

    memset(out, 0, SPARK_DMX_UNIVERSE_SIZE);
    if (!stage->blackout && scene.active)
        out[scene.dmx_channel] = scene.dmx_value;

    pthread_mutex_unlock(&stage->mutex);
}

void spark_stage_set_blackout(spark_stage_t *stage, bool value)
{
    pthread_mutex_lock(&stage->mutex);
    stage->blackout = value;
    pthread_mutex_unlock(&stage->mutex);
}
