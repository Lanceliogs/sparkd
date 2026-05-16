#include "stage.h"
#include "log.h"

#include <string.h>

void spark_stage_init(spark_stage_t *stage)
{
    stage->blackout = false;
    spark_scene_reset();
    pthread_mutex_init(&stage->mutex, NULL);
}

void spark_stage_destroy(spark_stage_t *stage)
{
    pthread_mutex_destroy(&stage->mutex);
}

void spark_stage_apply_midi(spark_stage_t *stage, const midi_event_t *event)
{
    pthread_mutex_lock(&stage->mutex);

    spark_scene_t *scene = spark_scene_get(event->channel, event->note);
    if (!scene->enabled)
    {
        pthread_mutex_unlock(&stage->mutex);
        return;
    }

    switch (scene->trigger.trigger_mode)
    {
        case SPARK_SCENE_GATE:
            if (event->type == SPARK_MIDI_NOTE_ON && event->velocity > 0)
                spark_scene_activate(scene, event->velocity);
            else if (event->type == SPARK_MIDI_NOTE_OFF || (event->type == SPARK_MIDI_NOTE_ON && event->velocity == 0))
                spark_scene_deactivate(scene);
            break;
        case SPARK_SCENE_TOGGLE:
            if (event->type == SPARK_MIDI_NOTE_ON && event->velocity > 0)
                spark_scene_toggle(scene, event->velocity);
            break;
        default:
            spark_log_warn("stage:apply_midi: Unknown scene trigger mode");
            break;
    }

    pthread_mutex_unlock(&stage->mutex);
}

void spark_stage_render(spark_stage_t *stage, uint8_t out[SPARK_DMX_UNIVERSE_SIZE])
{
    pthread_mutex_lock(&stage->mutex);

    memset(out, 0, SPARK_DMX_UNIVERSE_SIZE);

    if (stage->blackout)
    {
        pthread_mutex_unlock(&stage->mutex);
        return;
    }

    uint16_t count;
    spark_scene_t **active = spark_scene_get_active(&count);
    for (uint16_t i = 0; i < count; i++)
    {
        spark_scene_t *scene = active[i];
        if (scene->output.mode == SPARK_SCENE_STATIC)
        {
            for (uint8_t v = 0; v < scene->output.value_count; v++)
            {
                spark_scene_value_t *val = &scene->output.values[v];
                uint8_t dmx_val = val->value;
                if (val->velocity_scaling)
                    dmx_val = (val->value * scene->velocity) / 127;
                out[val->dmx_index] = dmx_val;
            }
        }
    }

    pthread_mutex_unlock(&stage->mutex);
}

void spark_stage_set_blackout(spark_stage_t *stage, bool value)
{
    pthread_mutex_lock(&stage->mutex);
    stage->blackout = value;
    pthread_mutex_unlock(&stage->mutex);
}
