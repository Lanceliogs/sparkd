#include "scene.h"
#include "clock.h"
#include "log.h"

#include <string.h>

static spark_scene_t scenes[SPARK_SCENES_MAX_COUNT] = {0};
static spark_scene_t *active_scenes[SPARK_ACTIVE_SCENES_MAX] = {0};
static uint16_t active_scene_count = 0;

spark_scene_t *spark_scene_get_all(void)
{
    return scenes;
}

spark_scene_t *spark_scene_get(uint8_t channel, uint8_t note)
{
    return &scenes[channel * 128 + note];
}

void spark_scene_reset(void)
{
    memset(scenes, 0, sizeof(scenes));
    memset(active_scenes, 0, sizeof(active_scenes));
    active_scene_count = 0;
}

spark_scene_t **spark_scene_get_active(uint16_t *count)
{
    *count = active_scene_count;
    return active_scenes;
}

void spark_scene_activate(spark_scene_t *scene, uint8_t velocity)
{
    if (scene->active)
        return;
    scene->active = true;
    scene->velocity = velocity;
    scene->start_time_ms = spark_clock_monotonic_ms();
    active_scenes[active_scene_count++] = scene;
    spark_log_debug("scene:activate: '%s' velocity=%u active_count=%u",
                    scene->id, velocity, active_scene_count);
}

void spark_scene_deactivate(spark_scene_t *scene)
{
    if (!scene->active)
        return;
    scene->active = false;
    for (int i = 0; i < active_scene_count; i++)
    {
        if (active_scenes[i] == scene) {
            active_scenes[i] = active_scenes[--active_scene_count];
            break;
        }
    }
    spark_log_debug("scene:deactivate: '%s' active_count=%u",
                    scene->id, active_scene_count);
}

void spark_scene_toggle(spark_scene_t *scene, uint8_t velocity)
{
    if (scene->active)
        spark_scene_deactivate(scene);
    else
        spark_scene_activate(scene, velocity);
}
