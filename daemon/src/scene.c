#include "scene.h"
#include "fixture.h"
#include "clock.h"
#include "log.h"

#include <string.h>

/* ---- Static storage ---- */

static spark_scene_def_t s_defs[SPARK_SCENE_DEFS_MAX];
static uint16_t s_def_count = 0;

static spark_scene_value_def_t s_value_def_arena[SPARK_SCENE_VALUE_DEF_ARENA_SIZE];
static uint16_t s_value_def_arena_used = 0;

static spark_scene_step_def_t s_step_def_arena[SPARK_SCENE_STEP_DEF_ARENA_SIZE];
static uint16_t s_step_def_arena_used = 0;

static spark_scene_t s_scenes[SPARK_SCENES_MAX_COUNT] = {0};

static spark_scene_value_t s_value_arena[SPARK_SCENE_VALUE_ARENA_SIZE];
static uint16_t s_value_arena_used = 0;

static spark_scene_step_t s_step_arena[SPARK_SCENE_STEP_ARENA_SIZE];
static uint16_t s_step_arena_used = 0;

static spark_scene_t *s_active_scenes[SPARK_ACTIVE_SCENES_MAX] = {0};
static uint16_t s_active_scene_count = 0;

/* ---- Scene defs ---- */

static spark_scene_value_def_t *s_value_def_arena_alloc(uint16_t count)
{
    if (s_value_def_arena_used + count > SPARK_SCENE_VALUE_DEF_ARENA_SIZE)
        return NULL;
    spark_scene_value_def_t *ptr = &s_value_def_arena[s_value_def_arena_used];
    s_value_def_arena_used += count;
    return ptr;
}

static spark_scene_step_def_t *s_step_def_arena_alloc(uint8_t count)
{
    if (s_step_def_arena_used + count > SPARK_SCENE_STEP_DEF_ARENA_SIZE)
        return NULL;
    spark_scene_step_def_t *ptr = &s_step_def_arena[s_step_def_arena_used];
    s_step_def_arena_used += count;
    return ptr;
}

int spark_scene_add_def(const spark_scene_def_t *def)
{
    if (s_def_count >= SPARK_SCENE_DEFS_MAX)
    {
        spark_log_error("scene: max scene defs reached (%d)", SPARK_SCENE_DEFS_MAX);
        return -1;
    }

    spark_scene_def_t *dst = &s_defs[s_def_count++];
    *dst = *def;

    /* Deep-copy static values */
    if (def->values && def->value_count > 0)
    {
        spark_scene_value_def_t *vals = s_value_def_arena_alloc(def->value_count);
        if (!vals)
        {
            s_def_count--;
            spark_log_error("scene: value def arena exhausted");
            return -1;
        }
        memcpy(vals, def->values, def->value_count * sizeof(spark_scene_value_def_t));
        dst->values = vals;
    }

    /* Deep-copy step defs and their value arrays */
    if (def->steps && def->step_count > 0)
    {
        spark_scene_step_def_t *steps = s_step_def_arena_alloc(def->step_count);
        if (!steps)
        {
            s_def_count--;
            spark_log_error("scene: step def arena exhausted");
            return -1;
        }
        memcpy(steps, def->steps, def->step_count * sizeof(spark_scene_step_def_t));
        dst->steps = steps;

        for (uint8_t i = 0; i < def->step_count; i++)
        {
            if (steps[i].values && steps[i].value_count > 0)
            {
                spark_scene_value_def_t *sv = s_value_def_arena_alloc(steps[i].value_count);
                if (!sv)
                {
                    s_def_count--;
                    spark_log_error("scene: value def arena exhausted (step %u)", i);
                    return -1;
                }
                memcpy(sv, def->steps[i].values,
                       steps[i].value_count * sizeof(spark_scene_value_def_t));
                steps[i].values = sv;
            }
        }
    }

    spark_log_debug("scene: added def '%s' ch=%u note=%u values=%u steps=%u",
        dst->id, dst->channel, dst->note, dst->value_count, dst->step_count);
    return 0;
}

/* ---- Resolution ---- */

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

static uint8_t s_resolve_value_defs(spark_scene_value_def_t *vdefs, uint8_t count)
{
    uint8_t resolved = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        spark_scene_value_def_t *vd = &vdefs[i];
        if (vd->fixture[0] == '\0')
        {
            vd->resolved = true;
            resolved++;
        }
        else
        {
            const spark_fixture_t *fix = spark_fixture_find(vd->fixture);
            if (!fix)
            {
                spark_log_warn("scene:resolve: fixture '%s' not found", vd->fixture);
                vd->resolved = false;
                continue;
            }
            const spark_channel_def_t *ch = spark_fixture_find_channel(fix, vd->channel);
            if (!ch)
            {
                spark_log_warn("scene:resolve: channel '%s' not found in fixture '%s'",
                    vd->channel, vd->fixture);
                vd->resolved = false;
                continue;
            }
            vd->dmx_index = spark_fixture_resolve_channel(fix, ch->offset);
            vd->resolved = true;
            resolved++;
        }
    }
    return resolved;
}

static spark_scene_value_t *s_copy_resolved_values(spark_scene_value_def_t *vdefs,
                                                   uint8_t count, uint8_t resolved_count)
{
    if (resolved_count == 0)
        return NULL;

    spark_scene_value_t *vals = s_value_arena_alloc(resolved_count);
    if (!vals)
        return NULL;

    uint8_t idx = 0;
    for (uint8_t i = 0; i < count; i++)
    {
        spark_scene_value_def_t *vd = &vdefs[i];
        if (!vd->resolved)
            continue;
        vals[idx].dmx_index = vd->dmx_index;
        vals[idx].value = vd->value;
        vals[idx].velocity_scaling = vd->velocity_scaling;
        idx++;
    }
    return vals;
}

static int s_resolve_static(spark_scene_def_t *def, spark_scene_t *scene)
{
    uint8_t resolved_count = s_resolve_value_defs(def->values, def->value_count);

    if (resolved_count == 0)
        return 0;

    spark_scene_value_t *vals = s_copy_resolved_values(
        def->values, def->value_count, resolved_count);
    if (!vals)
    {
        spark_log_error("scene:resolve: value arena exhausted");
        return -1;
    }

    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = vals;
    scene->output.value_count = resolved_count;
    return 0;
}

static int s_resolve_sequence(spark_scene_def_t *def, spark_scene_t *scene)
{
    spark_scene_step_t *steps = s_step_arena_alloc(def->step_count);
    if (!steps)
    {
        spark_log_error("scene:resolve: step arena exhausted");
        return -1;
    }

    for (uint8_t i = 0; i < def->step_count; i++)
    {
        spark_scene_step_def_t *sd = &def->steps[i];
        uint8_t resolved_count = s_resolve_value_defs(sd->values, sd->value_count);

        spark_scene_value_t *vals = s_copy_resolved_values(
            sd->values, sd->value_count, resolved_count);
        if (resolved_count > 0 && !vals)
        {
            spark_log_error("scene:resolve: value arena exhausted (step %u)", i);
            return -1;
        }

        steps[i].duration_ms = sd->duration_ms;
        steps[i].transition = sd->transition;
        steps[i].values = vals;
        steps[i].value_count = resolved_count;
    }

    scene->output.mode = SPARK_SCENE_SEQUENCE;
    scene->output.steps = steps;
    scene->output.step_count = def->step_count;
    scene->output.loop = def->loop;
    return 0;
}

int spark_scene_resolve(void)
{
    for (uint16_t i = 0; i < s_def_count; i++)
    {
        spark_scene_def_t *def = &s_defs[i];
        spark_scene_t *scene = spark_scene_get(def->channel, def->note);

        scene->id = def->id;
        scene->name = def->name;
        scene->enabled = def->enabled;
        scene->trigger_mode = def->trigger_mode;

        int rc = 0;
        if (def->output_mode == SPARK_SCENE_STATIC)
            rc = s_resolve_static(def, scene);
        else if (def->output_mode == SPARK_SCENE_SEQUENCE)
            rc = s_resolve_sequence(def, scene);

        if (rc != 0)
            return rc;

        spark_log_debug("scene:resolve: [%u] '%s' ch=%u note=%u mode=%d",
            i, def->id, def->channel, def->note, def->output_mode);
    }
    return 0;
}

/* ---- Runtime ---- */

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
    s_def_count = 0;
    s_value_def_arena_used = 0;
    s_step_def_arena_used = 0;
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
