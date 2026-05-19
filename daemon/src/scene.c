#include "scene.h"
#include "clock.h"
#include "log.h"

#include <string.h>

static spark_scene_t s_scenes[SPARK_SCENES_MAX_COUNT] = {0};
static spark_scene_t *s_active_scenes[SPARK_ACTIVE_SCENES_MAX] = {0};
static uint16_t s_active_scene_count = 0;

static spark_scene_value_t s_value_arena[SPARK_SCENE_VALUE_ARENA_SIZE];
static uint16_t s_value_arena_used = 0;

static spark_scene_step_t s_step_arena[SPARK_SCENE_STEP_ARENA_SIZE];
static uint16_t s_step_arena_used = 0;

static spark_scene_value_t *s_value_arena_alloc(uint16_t count)
{
    if (s_value_arena_used + count > SPARK_SCENE_VALUE_ARENA_SIZE)
        return NULL;
    spark_scene_value_t *ptr = &s_value_arena[s_value_arena_used];
    s_value_arena_used += count;
    return ptr;
}

static spark_scene_step_t *s_step_arena_alloc(uint16_t count)
{
    if (s_step_arena_used + count > SPARK_SCENE_STEP_ARENA_SIZE)
        return NULL;
    spark_scene_step_t *ptr = &s_step_arena[s_step_arena_used];
    s_step_arena_used += count;
    return ptr;
}

spark_scene_t *spark_scene_get_all(void)
{
    return s_scenes;
}

spark_scene_t *spark_scene_get(uint8_t channel, uint8_t note)
{
    return &s_scenes[channel * 128 + note];
}

void spark_scene_reset(void)
{
    memset(s_scenes, 0, sizeof(s_scenes));
    memset(s_active_scenes, 0, sizeof(s_active_scenes));
    s_active_scene_count = 0;
    s_value_arena_used = 0;
    s_step_arena_used = 0;
}

spark_scene_t **spark_scene_get_active(uint16_t *count)
{
    *count = s_active_scene_count;
    return s_active_scenes;
}

void spark_scene_activate(spark_scene_t *scene, uint8_t velocity)
{
    if (scene->active)
        return;
    scene->active = true;
    scene->velocity = velocity;
    scene->start_time_ms = spark_clock_monotonic_ms();
    s_active_scenes[s_active_scene_count++] = scene;
    spark_log_debug("scene:activate: '%s' velocity=%u active_count=%u",
                    scene->id, velocity, s_active_scene_count);
}

void spark_scene_deactivate(spark_scene_t *scene)
{
    if (!scene->active)
        return;
    scene->active = false;
    for (int i = 0; i < s_active_scene_count; i++)
    {
        if (s_active_scenes[i] == scene) {
            s_active_scenes[i] = s_active_scenes[--s_active_scene_count];
            break;
        }
    }
    spark_log_debug("scene:deactivate: '%s' active_count=%u",
                    scene->id, s_active_scene_count);
}

void spark_scene_toggle(spark_scene_t *scene, uint8_t velocity)
{
    if (scene->active)
        spark_scene_deactivate(scene);
    else
        spark_scene_activate(scene, velocity);
}
