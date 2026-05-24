#include "project_yaml.h"
#include "../fixture.h"
#include "../fixture_bank.h"
#include "../scene.h"
#include "../consts.h"
#include "../log.h"

#include <yaml.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Helpers ---- */

static const char *s_event_type_str(yaml_event_type_t t)
{
    switch (t) {
    case YAML_SCALAR_EVENT:         return "scalar";
    case YAML_MAPPING_START_EVENT:  return "mapping_start";
    case YAML_MAPPING_END_EVENT:    return "mapping_end";
    case YAML_SEQUENCE_START_EVENT: return "sequence_start";
    case YAML_SEQUENCE_END_EVENT:   return "sequence_end";
    case YAML_STREAM_END_EVENT:     return "stream_end";
    case YAML_DOCUMENT_END_EVENT:   return "document_end";
    default:                        return "other";
    }
}

static int s_expect(yaml_parser_t *p, yaml_event_t *ev, yaml_event_type_t expected)
{
    if (!yaml_parser_parse(p, ev))
    {
        spark_log_error("project: YAML parse error at line %zu: %s",
            p->problem_mark.line + 1, p->problem);
        return -1;
    }
    if (ev->type != expected)
    {
        spark_log_error("project: expected %s but got %s at line %zu",
            s_event_type_str(expected), s_event_type_str(ev->type),
            ev->start_mark.line + 1);
        yaml_event_delete(ev);
        return -1;
    }
    return 0;
}

static int s_next(yaml_parser_t *p, yaml_event_t *ev)
{
    if (!yaml_parser_parse(p, ev))
    {
        spark_log_error("project: YAML parse error at line %zu: %s",
            p->problem_mark.line + 1, p->problem);
        return -1;
    }
    return 0;
}

static void s_skip_value(yaml_parser_t *p)
{
    yaml_event_t ev;
    int depth = 0;
    for (;;)
    {
        if (s_next(p, &ev) != 0) return;
        yaml_event_type_t t = ev.type;
        yaml_event_delete(&ev);
        if (t == YAML_MAPPING_START_EVENT || t == YAML_SEQUENCE_START_EVENT) depth++;
        else if (t == YAML_MAPPING_END_EVENT || t == YAML_SEQUENCE_END_EVENT) { if (--depth == 0) return; }
        else if (t == YAML_SCALAR_EVENT && depth == 0) return;
    }
}

static int s_unexpected(yaml_parser_t *p, yaml_event_t *ev, const char *section)
{
    (void)p;
    if (ev->type == YAML_SCALAR_EVENT)
    {
        spark_log_error("project: unknown key \"%s\" in %s at line %zu col %zu",
            (const char *)ev->data.scalar.value, section,
            ev->start_mark.line + 1, ev->start_mark.column + 1);
    }
    else
    {
        spark_log_error("project: unexpected element in %s at line %zu col %zu",
            section, ev->start_mark.line + 1, ev->start_mark.column + 1);
    }
    yaml_event_delete(ev);
    return -1;
}

static const char *s_scalar(yaml_event_t *ev)
{
    return (const char *)ev->data.scalar.value;
}

static int s_scalar_to_int(yaml_event_t *ev, int *out)
{
    char *end;
    long val = strtol(s_scalar(ev), &end, 10);
    if (*end != '\0') return -1;
    *out = (int)val;
    return 0;
}

static int s_scalar_to_bool(yaml_event_t *ev, bool *out)
{
    const char *v = s_scalar(ev);
    if (strcmp(v, "true") == 0 || strcmp(v, "yes") == 0)  { *out = true; return 0; }
    if (strcmp(v, "false") == 0 || strcmp(v, "no") == 0)  { *out = false; return 0; }
    return -1;
}

/* ---- Fixture parsing ---- */

static int s_parse_channels(yaml_parser_t *p, spark_channel_def_t *channels,
                            uint8_t *count, uint8_t max)
{
    yaml_event_t ev;
    *count = 0;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;

        if (ev.type == YAML_SEQUENCE_END_EVENT)
        {
            yaml_event_delete(&ev);
            return 0;
        }

        if (ev.type != YAML_MAPPING_START_EVENT)
        {
            spark_log_error("project: expected channel mapping at line %zu",
                ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }
        yaml_event_delete(&ev);

        if (*count >= max)
        {
            spark_log_error("project: too many channels (max %d)", max);
            return -1;
        }

        spark_channel_def_t *ch = &channels[*count];
        memset(ch, 0, sizeof(*ch));

        for (;;)
        {
            if (s_next(p, &ev) != 0) return -1;
            if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }

            const char *key = s_scalar(&ev);

            if (strcmp(key, "name") == 0)
            {
                yaml_event_delete(&ev);
                if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
                strncpy(ch->name, s_scalar(&ev), SPARK_MAX_NAME_SIZE - 1);
                yaml_event_delete(&ev);
            }
            else if (strcmp(key, "offset") == 0)
            {
                yaml_event_delete(&ev);
                if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
                int val;
                if (s_scalar_to_int(&ev, &val) != 0 || val < 0 || val > 255)
                {
                    spark_log_error("project: invalid channel offset at line %zu",
                        ev.start_mark.line + 1);
                    yaml_event_delete(&ev);
                    return -1;
                }
                ch->offset = (uint8_t)val;
                yaml_event_delete(&ev);
            }
            else
            {
                return s_unexpected(p, &ev, "channel");
            }
        }

        (*count)++;
    }
}

static int s_parse_fixture(yaml_parser_t *p)
{
    yaml_event_t ev;

    spark_fixture_t fix;
    memset(&fix, 0, sizeof(fix));

    spark_channel_def_t channels[64];
    uint8_t channel_count = 0;
    char copy_from[SPARK_MAX_ID_SIZE] = {0};
    char template_ref[SPARK_MAX_ID_SIZE + SPARK_MAX_ID_SIZE] = {0};
    int has_inline_channels = 0;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }

        if (ev.type != YAML_SCALAR_EVENT)
        {
            spark_log_error("project: expected fixture key at line %zu",
                ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }

        const char *key = s_scalar(&ev);

        if (strcmp(key, "id") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            strncpy(fix.id, s_scalar(&ev), SPARK_MAX_ID_SIZE - 1);
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "name") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            strncpy(fix.name, s_scalar(&ev), SPARK_MAX_NAME_SIZE - 1);
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "start-address") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            int val;
            if (s_scalar_to_int(&ev, &val) != 0 || val < 1 || val > 512)
            {
                spark_log_error("project: invalid start-address at line %zu",
                    ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }
            fix.start_address = (uint16_t)val;
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "channel-count") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            int val;
            if (s_scalar_to_int(&ev, &val) != 0 || val < 1 || val > 255)
            {
                spark_log_error("project: invalid channel-count at line %zu",
                    ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }
            fix.channel_count = (uint8_t)val;
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "channels") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SEQUENCE_START_EVENT) != 0) return -1;
            yaml_event_delete(&ev);
            if (s_parse_channels(p, channels, &channel_count, 64) != 0)
                return -1;
            has_inline_channels = 1;
        }
        else if (strcmp(key, "copy-from") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            strncpy(copy_from, s_scalar(&ev), sizeof(copy_from) - 1);
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "template") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            strncpy(template_ref, s_scalar(&ev), sizeof(template_ref) - 1);
            yaml_event_delete(&ev);
        }
        else
        {
            return s_unexpected(p, &ev, "fixture");
        }
    }

    /* Validation */
    if (fix.id[0] == '\0')
    {
        spark_log_error("project: fixture missing 'id'");
        return -1;
    }
    if (fix.start_address == 0)
    {
        spark_log_error("project: fixture '%s' missing 'start-address'", fix.id);
        return -1;
    }

    int source_count = (copy_from[0] != '\0') + (template_ref[0] != '\0') + has_inline_channels;
    if (source_count > 1)
    {
        spark_log_error("project: fixture '%s' has multiple channel sources "
                        "(use only one of: channels, copy-from, template)", fix.id);
        return -1;
    }

    /* Resolve channel data */
    if (copy_from[0] != '\0')
    {
        const spark_fixture_t *src = spark_fixture_find(copy_from);
        if (!src)
        {
            spark_log_error("project: fixture '%s' copy-from '%s' not found "
                            "(must appear earlier in file)", fix.id, copy_from);
            return -1;
        }
        fix.channel_count = src->channel_count;
        fix.channels = src->channels;
        spark_log_debug("project: fixture '%s' copying channels from '%s'", fix.id, copy_from);
    }
    else if (template_ref[0] != '\0')
    {
        const spark_fixture_t *src = spark_fixture_bank_find(template_ref);
        if (!src)
        {
            spark_log_error("project: fixture '%s' template '%s' not found in bank",
                            fix.id, template_ref);
            return -1;
        }
        fix.channel_count = src->channel_count;
        fix.channels = src->channels;
        spark_log_debug("project: fixture '%s' using template '%s'", fix.id, template_ref);
    }
    else
    {
        /* Inline channels (current behavior) */
        if (fix.channel_count == 0 && channel_count == 0)
        {
            spark_log_error("project: fixture '%s' missing channel data "
                            "(provide channels, copy-from, or template)", fix.id);
            return -1;
        }
        if (fix.channel_count == 0)
            fix.channel_count = channel_count;

        fix.channels = channels;
        if (channel_count > 0 && channel_count != fix.channel_count)
        {
            spark_log_warn("project: fixture '%s' channel-count %d != channels listed %d",
                fix.id, fix.channel_count, channel_count);
        }
    }

    if (fix.name[0] == '\0')
        strncpy(fix.name, fix.id, SPARK_MAX_NAME_SIZE - 1);

    return spark_fixture_add(&fix);
}

static int s_parse_fixtures(yaml_parser_t *p)
{
    yaml_event_t ev;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;

        if (ev.type == YAML_SEQUENCE_END_EVENT)
        {
            yaml_event_delete(&ev);
            return 0;
        }

        if (ev.type != YAML_MAPPING_START_EVENT)
        {
            spark_log_error("project: expected fixture mapping at line %zu",
                ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }
        yaml_event_delete(&ev);

        if (s_parse_fixture(p) != 0)
            return -1;
    }
}

/* ---- Scene parsing ---- */

#define MAX_PARSE_VALUES 64
#define MAX_PARSE_STEPS 32

static int s_parse_target(const char *target, size_t line,
                          char *fixture_out, char *channel_out)
{
    const char *dot = strchr(target, '.');
    if (!dot || dot == target || *(dot + 1) == '\0')
    {
        spark_log_error("project: invalid target '%s' at line %zu (expected fixture.channel)",
            target, line);
        return -1;
    }

    size_t fix_len = (size_t)(dot - target);
    if (fix_len >= SPARK_MAX_ID_SIZE)
    {
        spark_log_error("project: fixture id too long in target '%s' at line %zu", target, line);
        return -1;
    }

    size_t ch_len = strlen(dot + 1);
    if (ch_len >= SPARK_MAX_NAME_SIZE)
    {
        spark_log_error("project: channel name too long in target '%s' at line %zu", target, line);
        return -1;
    }

    memcpy(fixture_out, target, fix_len);
    fixture_out[fix_len] = '\0';
    strcpy(channel_out, dot + 1);
    return 0;
}

static int s_parse_scene_values(yaml_parser_t *p, spark_scene_value_def_t *vals,
                                uint8_t *count, uint8_t max)
{
    yaml_event_t ev;
    *count = 0;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;

        if (ev.type == YAML_MAPPING_END_EVENT)
        {
            yaml_event_delete(&ev);
            return 0;
        }

        if (ev.type != YAML_SCALAR_EVENT)
        {
            spark_log_error("project: expected target key at line %zu",
                ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }

        if (*count >= max)
        {
            spark_log_error("project: too many values in scene (max %d)", max);
            yaml_event_delete(&ev);
            return -1;
        }

        spark_scene_value_def_t *vd = &vals[*count];
        memset(vd, 0, sizeof(*vd));

        size_t line = ev.start_mark.line + 1;
        if (s_parse_target(s_scalar(&ev), line, vd->fixture, vd->channel) != 0)
        {
            yaml_event_delete(&ev);
            return -1;
        }
        yaml_event_delete(&ev);

        if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
        int val;
        if (s_scalar_to_int(&ev, &val) != 0 || val < 0 || val > 255)
        {
            spark_log_error("project: invalid DMX value at line %zu", ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }
        vd->value = (uint8_t)val;
        yaml_event_delete(&ev);

        (*count)++;
    }
}

static int s_parse_scene_steps(yaml_parser_t *p, spark_scene_step_def_t *steps,
                               uint8_t *count, uint8_t max,
                               spark_scene_value_def_t *val_buf, uint8_t *val_buf_used,
                               uint8_t val_buf_cap)
{
    yaml_event_t ev;
    *count = 0;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;

        if (ev.type == YAML_SEQUENCE_END_EVENT)
        {
            yaml_event_delete(&ev);
            return 0;
        }

        if (ev.type != YAML_MAPPING_START_EVENT)
        {
            spark_log_error("project: expected step mapping at line %zu",
                ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }
        yaml_event_delete(&ev);

        if (*count >= max)
        {
            spark_log_error("project: too many steps (max %d)", max);
            return -1;
        }

        spark_scene_step_def_t *step = &steps[*count];
        memset(step, 0, sizeof(*step));
        step->transition = SPARK_SCENE_HOLD;

        for (;;)
        {
            if (s_next(p, &ev) != 0) return -1;
            if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }

            if (ev.type != YAML_SCALAR_EVENT)
            {
                spark_log_error("project: expected step key at line %zu",
                    ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }

            const char *key = s_scalar(&ev);

            if (strcmp(key, "duration-ms") == 0)
            {
                yaml_event_delete(&ev);
                if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
                int val;
                if (s_scalar_to_int(&ev, &val) != 0 || val <= 0)
                {
                    spark_log_error("project: invalid duration-ms at line %zu",
                        ev.start_mark.line + 1);
                    yaml_event_delete(&ev);
                    return -1;
                }
                step->duration_ms = (uint32_t)val;
                yaml_event_delete(&ev);
            }
            else if (strcmp(key, "transition") == 0)
            {
                yaml_event_delete(&ev);
                if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
                const char *v = s_scalar(&ev);
                if (strcmp(v, "hold") == 0)
                    step->transition = SPARK_SCENE_HOLD;
                else if (strcmp(v, "linear") == 0)
                    step->transition = SPARK_SCENE_LINEAR;
                else
                {
                    spark_log_error("project: unknown transition '%s' at line %zu",
                        v, ev.start_mark.line + 1);
                    yaml_event_delete(&ev);
                    return -1;
                }
                yaml_event_delete(&ev);
            }
            else if (strcmp(key, "values") == 0)
            {
                yaml_event_delete(&ev);
                if (s_expect(p, &ev, YAML_MAPPING_START_EVENT) != 0) return -1;
                yaml_event_delete(&ev);

                uint8_t remaining = val_buf_cap - *val_buf_used;
                uint8_t vc = 0;
                if (s_parse_scene_values(p, &val_buf[*val_buf_used], &vc, remaining) != 0)
                    return -1;
                step->values = &val_buf[*val_buf_used];
                step->value_count = vc;
                *val_buf_used += vc;
            }
            else
            {
                return s_unexpected(p, &ev, "step");
            }
        }

        if (step->duration_ms == 0)
        {
            spark_log_error("project: step missing 'duration-ms'");
            return -1;
        }

        (*count)++;
    }
}

static int s_parse_scene_trigger(yaml_parser_t *p, spark_scene_def_t *def)
{
    yaml_event_t ev;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); return 0; }

        if (ev.type != YAML_SCALAR_EVENT)
        {
            spark_log_error("project: expected trigger key at line %zu",
                ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }

        const char *key = s_scalar(&ev);

        if (strcmp(key, "channel") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            int val;
            if (s_scalar_to_int(&ev, &val) != 0 || val < 1 || val > 16)
            {
                spark_log_error("project: invalid trigger channel at line %zu",
                    ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }
            def->channel = (uint8_t)(val - 1);
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "note") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            int val;
            if (s_scalar_to_int(&ev, &val) != 0 || val < 0 || val > 127)
            {
                spark_log_error("project: invalid trigger note at line %zu",
                    ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }
            def->note = (uint8_t)val;
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "mode") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            const char *v = s_scalar(&ev);
            if (strcmp(v, "gate") == 0)
                def->trigger_mode = SPARK_SCENE_GATE;
            else if (strcmp(v, "toggle") == 0)
                def->trigger_mode = SPARK_SCENE_TOGGLE;
            else
            {
                spark_log_error("project: unknown trigger mode '%s' at line %zu",
                    v, ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }
            yaml_event_delete(&ev);
        }
        else
        {
            return s_unexpected(p, &ev, "trigger");
        }
    }
}

static int s_parse_scene(yaml_parser_t *p)
{
    yaml_event_t ev;

    spark_scene_def_t def;
    memset(&def, 0, sizeof(def));
    def.enabled = true;
    def.trigger_mode = SPARK_SCENE_GATE;
    def.output_mode = SPARK_SCENE_STATIC;

    spark_scene_value_def_t val_buf[MAX_PARSE_VALUES];
    uint8_t val_buf_used = 0;

    spark_scene_step_def_t step_buf[MAX_PARSE_STEPS];
    uint8_t step_count = 0;

    bool has_trigger = false;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }

        if (ev.type != YAML_SCALAR_EVENT)
        {
            spark_log_error("project: expected scene key at line %zu",
                ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }

        const char *key = s_scalar(&ev);

        if (strcmp(key, "id") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            strncpy(def.id, s_scalar(&ev), SPARK_MAX_ID_SIZE - 1);
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "name") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            strncpy(def.name, s_scalar(&ev), SPARK_MAX_NAME_SIZE - 1);
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "enabled") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            if (s_scalar_to_bool(&ev, &def.enabled) != 0)
            {
                spark_log_error("project: invalid 'enabled' value at line %zu",
                    ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "type") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            const char *v = s_scalar(&ev);
            if (strcmp(v, "static") == 0)
                def.output_mode = SPARK_SCENE_STATIC;
            else if (strcmp(v, "sequence") == 0)
                def.output_mode = SPARK_SCENE_SEQUENCE;
            else
            {
                spark_log_error("project: unknown scene type '%s' at line %zu",
                    v, ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "trigger") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_MAPPING_START_EVENT) != 0) return -1;
            yaml_event_delete(&ev);
            if (s_parse_scene_trigger(p, &def) != 0) return -1;
            has_trigger = true;
        }
        else if (strcmp(key, "values") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_MAPPING_START_EVENT) != 0) return -1;
            yaml_event_delete(&ev);
            if (s_parse_scene_values(p, val_buf, &val_buf_used, MAX_PARSE_VALUES) != 0)
                return -1;
        }
        else if (strcmp(key, "loop") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            if (s_scalar_to_bool(&ev, &def.loop) != 0)
            {
                spark_log_error("project: invalid 'loop' value at line %zu",
                    ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "steps") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SEQUENCE_START_EVENT) != 0) return -1;
            yaml_event_delete(&ev);
            if (s_parse_scene_steps(p, step_buf, &step_count, MAX_PARSE_STEPS,
                                    val_buf, &val_buf_used, MAX_PARSE_VALUES) != 0)
                return -1;
        }
        else
        {
            return s_unexpected(p, &ev, "scene");
        }
    }

    if (def.id[0] == '\0')
    {
        spark_log_error("project: scene missing 'id'");
        return -1;
    }
    if (!has_trigger)
    {
        spark_log_error("project: scene '%s' missing 'trigger'", def.id);
        return -1;
    }
    if (def.name[0] == '\0')
        strncpy(def.name, def.id, SPARK_MAX_NAME_SIZE - 1);

    if (def.output_mode == SPARK_SCENE_STATIC)
    {
        def.values = val_buf;
        def.value_count = val_buf_used;
    }
    else
    {
        def.steps = step_buf;
        def.step_count = step_count;
    }

    return spark_scene_add_def(&def);
}

static int s_parse_scenes(yaml_parser_t *p)
{
    yaml_event_t ev;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;

        if (ev.type == YAML_SEQUENCE_END_EVENT)
        {
            yaml_event_delete(&ev);
            return 0;
        }

        if (ev.type != YAML_MAPPING_START_EVENT)
        {
            spark_log_error("project: expected scene mapping at line %zu",
                ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }
        yaml_event_delete(&ev);

        if (s_parse_scene(p) != 0)
            return -1;
    }
}

/* ---- MIDI section ---- */

static int s_parse_midi(yaml_parser_t *p, spark_project_config_t *cfg)
{
    yaml_event_t ev;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); return 0; }

        if (ev.type != YAML_SCALAR_EVENT)
        {
            spark_log_error("project: expected midi key at line %zu", ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }

        const char *key = s_scalar(&ev);

        if (strcmp(key, "mode") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            const char *v = s_scalar(&ev);
            if (strcmp(v, "open-existing") == 0)
                cfg->midi_mode = SPARK_MIDI_MODE_OPEN_EXISTING;
            else if (strcmp(v, "create-virtual") == 0)
                cfg->midi_mode = SPARK_MIDI_MODE_CREATE_VIRTUAL;
            else
            {
                spark_log_error("project: unknown midi mode '%s' at line %zu",
                    v, ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "device") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            strncpy(cfg->midi_device, s_scalar(&ev), SPARK_MIDI_PORT_STRLEN - 1);
            yaml_event_delete(&ev);
        }
        else
        {
            return s_unexpected(p, &ev, "midi");
        }
    }
}

/* ---- DMX section ---- */

static int s_parse_dmx(yaml_parser_t *p, spark_project_config_t *cfg)
{
    yaml_event_t ev;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); return 0; }

        if (ev.type != YAML_SCALAR_EVENT)
        {
            spark_log_error("project: expected dmx key at line %zu", ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }

        const char *key = s_scalar(&ev);

        if (strcmp(key, "backend") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            const char *v = s_scalar(&ev);
            if (strcmp(v, "open") == 0)
                cfg->dmx_backend = SPARK_DMX_BACKEND_OPEN;
            else if (strcmp(v, "pro") == 0)
                cfg->dmx_backend = SPARK_DMX_BACKEND_PRO;
            else if (strcmp(v, "dummy") == 0)
                cfg->dmx_backend = SPARK_DMX_BACKEND_DUMMY;
            else
            {
                spark_log_error("project: unknown dmx backend '%s' at line %zu",
                    v, ev.start_mark.line + 1);
                yaml_event_delete(&ev);
                return -1;
            }
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "device") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            strncpy(cfg->dmx_device, s_scalar(&ev), SPARK_SERIAL_PORT_STRLEN - 1);
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "refresh-rate-hz") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;
            int val;
            if (s_scalar_to_int(&ev, &val) == 0 && val >= 1 && val <= 44)
                cfg->dmx_refresh_rate_hz = (uint8_t)val;
            else
                spark_log_warn("project: refresh-rate-hz out of range (1-44) at line %zu, using default",
                    ev.start_mark.line + 1);
            yaml_event_delete(&ev);
        }
        else
        {
            return s_unexpected(p, &ev, "dmx");
        }
    }
}

/* ---- Includes (directory mode) ---- */

static int s_parse_include_file(const char *base_dir, const char *filename,
                                int (*parse_fn)(yaml_parser_t *))
{
    char filepath[SPARK_PROJECT_PATH_STRLEN];
    snprintf(filepath, sizeof(filepath), "%s/%s", base_dir, filename);

    FILE *f = fopen(filepath, "rb");
    if (!f)
    {
        spark_log_error("project: cannot open include '%s'", filepath);
        return -1;
    }

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser))
    {
        spark_log_error("project: failed to initialize YAML parser for '%s'", filepath);
        fclose(f);
        return -1;
    }

    yaml_parser_set_input_file(&parser, f);

    yaml_event_t ev;
    int rc = -1;

    if (s_expect(&parser, &ev, YAML_STREAM_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);
    if (s_expect(&parser, &ev, YAML_DOCUMENT_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);
    if (s_expect(&parser, &ev, YAML_SEQUENCE_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);

    if (parse_fn(&parser) != 0) goto done;
    rc = 0;

done:
    yaml_parser_delete(&parser);
    fclose(f);
    return rc;
}

static int s_parse_includes(yaml_parser_t *p, const char *base_dir,
                            bool *has_fixtures, bool *has_scenes)
{
    yaml_event_t ev;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); return 0; }

        if (ev.type != YAML_SCALAR_EVENT)
        {
            spark_log_error("project: expected includes key at line %zu",
                ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            return -1;
        }

        char key[SPARK_MAX_ID_SIZE];
        strncpy(key, s_scalar(&ev), SPARK_MAX_ID_SIZE - 1);
        key[SPARK_MAX_ID_SIZE - 1] = '\0';
        yaml_event_delete(&ev);

        if (s_expect(p, &ev, YAML_SCALAR_EVENT) != 0) return -1;

        char filename[SPARK_PROJECT_PATH_STRLEN];
        strncpy(filename, s_scalar(&ev), SPARK_PROJECT_PATH_STRLEN - 1);
        filename[SPARK_PROJECT_PATH_STRLEN - 1] = '\0';
        yaml_event_delete(&ev);

        if (strcmp(key, "fixtures") == 0)
        {
            if (*has_fixtures)
            {
                spark_log_error("project: 'fixtures' defined both inline and in includes");
                return -1;
            }
            if (s_parse_include_file(base_dir, filename, s_parse_fixtures) != 0)
                return -1;
            *has_fixtures = true;
        }
        else if (strcmp(key, "scenes") == 0)
        {
            if (*has_scenes)
            {
                spark_log_error("project: 'scenes' defined both inline and in includes");
                return -1;
            }
            if (s_parse_include_file(base_dir, filename, s_parse_scenes) != 0)
                return -1;
            *has_scenes = true;
        }
        else
        {
            spark_log_debug("project: skipping unknown include '%s'", key);
        }
    }
}

/* ---- Top-level parser ---- */

static void s_get_base_dir(const char *path, char *out, size_t out_size)
{
    strncpy(out, path, out_size - 1);
    out[out_size - 1] = '\0';
    char *last_sep = strrchr(out, '/');
#ifdef _WIN32
    char *last_bsep = strrchr(out, '\\');
    if (last_bsep > last_sep) last_sep = last_bsep;
#endif
    if (last_sep)
        *last_sep = '\0';
    else
        strcpy(out, ".");
}

int spark_project_parse_yaml(const char *path, spark_project_config_t *cfg)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        spark_log_error("project: cannot open '%s'", path);
        return -1;
    }

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser))
    {
        spark_log_error("project: failed to initialize YAML parser");
        fclose(f);
        return -1;
    }

    yaml_parser_set_input_file(&parser, f);

    char base_dir[SPARK_PROJECT_PATH_STRLEN];
    s_get_base_dir(path, base_dir, sizeof(base_dir));

    yaml_event_t ev;
    int rc = -1;
    bool has_fixtures = false;
    bool has_scenes = false;

    if (s_expect(&parser, &ev, YAML_STREAM_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);

    if (s_expect(&parser, &ev, YAML_DOCUMENT_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);

    if (s_expect(&parser, &ev, YAML_MAPPING_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);

    for (;;)
    {
        if (s_next(&parser, &ev) != 0) goto done;

        if (ev.type == YAML_MAPPING_END_EVENT)
        {
            yaml_event_delete(&ev);
            break;
        }

        if (ev.type != YAML_SCALAR_EVENT)
        {
            spark_log_error("project: expected top-level key at line %zu",
                ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            goto done;
        }

        const char *key = s_scalar(&ev);

        if (strcmp(key, "fixtures") == 0)
        {
            if (has_fixtures)
            {
                spark_log_error("project: 'fixtures' defined both inline and in includes");
                yaml_event_delete(&ev);
                goto done;
            }
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_SEQUENCE_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);
            if (s_parse_fixtures(&parser) != 0) goto done;
            has_fixtures = true;
        }
        else if (strcmp(key, "scenes") == 0)
        {
            if (has_scenes)
            {
                spark_log_error("project: 'scenes' defined both inline and in includes");
                yaml_event_delete(&ev);
                goto done;
            }
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_SEQUENCE_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);
            if (s_parse_scenes(&parser) != 0) goto done;
            has_scenes = true;
        }
        else if (strcmp(key, "midi") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_MAPPING_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);
            if (s_parse_midi(&parser, cfg) != 0) goto done;
        }
        else if (strcmp(key, "dmx") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_MAPPING_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);
            if (s_parse_dmx(&parser, cfg) != 0) goto done;
        }
        else if (strcmp(key, "includes") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_MAPPING_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);
            if (s_parse_includes(&parser, base_dir, &has_fixtures, &has_scenes) != 0)
                goto done;
        }
        else if (strcmp(key, "format") == 0 || strcmp(key, "app") == 0)
        {
            yaml_event_delete(&ev);
            s_skip_value(&parser);
        }
        else
        {
            spark_log_error("project: unknown top-level key \"%s\" at line %zu",
                key, ev.start_mark.line + 1);
            yaml_event_delete(&ev);
            goto done;
        }
    }

    rc = 0;

done:
    yaml_parser_delete(&parser);
    fclose(f);
    return rc;
}
