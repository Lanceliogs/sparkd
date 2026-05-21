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

void spark_stage_apply_midi(spark_stage_t *stage, const spark_midi_event_t *event)
{
    pthread_mutex_lock(&stage->mutex);

    spark_scene_t *scene = spark_scene_get(event->channel, event->note);
    if (!scene->def || !scene->def->enabled)
    {
        pthread_mutex_unlock(&stage->mutex);
        return;
    }

    switch (scene->def->trigger_mode)
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

static void s_apply_values(spark_scene_value_t *values, uint8_t count,
                           uint8_t velocity, uint8_t out[SPARK_DMX_UNIVERSE_SIZE])
{
    for (uint8_t v = 0; v < count; v++)
    {
        spark_scene_value_t *val = &values[v];
        uint8_t dmx_val = val->value;
        if (val->velocity_scaling)
            dmx_val = (val->value * velocity) / 127;
        out[val->dmx_index] = dmx_val;
    }
}

typedef struct {
    int index;
    uint64_t elapsed_in_step;
} step_phase_t;

static step_phase_t s_sequence_phase(spark_scene_output_t *out, uint64_t elapsed_ms)
{
    uint64_t total_ms = 0;
    for (uint8_t i = 0; i < out->step_count; i++)
        total_ms += out->steps[i].duration_ms;

    if (total_ms == 0)
        return (step_phase_t){ .index = 0, .elapsed_in_step = 0 };

    if (out->loop)
        elapsed_ms = elapsed_ms % total_ms;
    else if (elapsed_ms >= total_ms)
        return (step_phase_t){ .index = out->step_count - 1, .elapsed_in_step = 0 };

    uint64_t step_start = 0;
    for (uint8_t i = 0; i < out->step_count; i++)
    {
        if (elapsed_ms < step_start + out->steps[i].duration_ms)
            return (step_phase_t){ .index = i, .elapsed_in_step = elapsed_ms - step_start };
        step_start += out->steps[i].duration_ms;
    }
    return (step_phase_t){ .index = out->step_count - 1, .elapsed_in_step = 0 };
}

static uint8_t s_find_next_value(spark_scene_step_t *next_step, uint16_t dmx_index,
                                 uint8_t fallback)
{
    for (uint8_t i = 0; i < next_step->value_count; i++)
    {
        if (next_step->values[i].dmx_index == dmx_index)
            return next_step->values[i].value;
    }
    return fallback;
}

static void s_apply_values_lerp(spark_scene_step_t *cur, spark_scene_step_t *next,
                                uint64_t elapsed_in_step, uint8_t velocity,
                                uint8_t out[SPARK_DMX_UNIVERSE_SIZE])
{
    uint32_t duration = cur->duration_ms;
    for (uint8_t v = 0; v < cur->value_count; v++)
    {
        spark_scene_value_t *val = &cur->values[v];
        uint8_t from = val->value;
        uint8_t to = s_find_next_value(next, val->dmx_index, from);
        uint8_t dmx_val = from + (int)(to - from) * (int)elapsed_in_step / (int)duration;
        if (val->velocity_scaling)
            dmx_val = (dmx_val * velocity) / 127;
        out[val->dmx_index] = dmx_val;
    }
}

void spark_stage_render(spark_stage_t *stage, uint64_t now_ms, uint8_t out[SPARK_DMX_UNIVERSE_SIZE])
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
    spark_log_debug("stage:render: %u active scene(s)", count);
    for (uint16_t i = 0; i < count; i++)
    {
        spark_scene_t *scene = active[i];
        if (scene->output.mode == SPARK_SCENE_STATIC)
        {
            s_apply_values(scene->output.values, scene->output.value_count,
                           scene->velocity, out);
        }
        else if (scene->output.mode == SPARK_SCENE_SEQUENCE)
        {
            uint64_t elapsed = now_ms - scene->start_time_ms;
            step_phase_t phase = s_sequence_phase(&scene->output, elapsed);
            spark_scene_step_t *step = &scene->output.steps[phase.index];

            if (step->transition == SPARK_SCENE_LINEAR && step->duration_ms > 0)
            {
                int next_idx = (phase.index + 1) % scene->output.step_count;
                spark_scene_step_t *next = &scene->output.steps[next_idx];
                s_apply_values_lerp(step, next, phase.elapsed_in_step,
                                    scene->velocity, out);
            }
            else
            {
                s_apply_values(step->values, step->value_count,
                               scene->velocity, out);
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
