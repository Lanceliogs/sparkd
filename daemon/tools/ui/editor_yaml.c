#include "editor.h"
#include "log.h"

#include <yaml.h>
#include <stdio.h>
#include <string.h>

/* ---- Parsing helpers ---- */

static const char *s_scalar(yaml_event_t *ev)
{
    return (const char *)ev->data.scalar.value;
}

static int s_next(yaml_parser_t *p, yaml_event_t *ev)
{
    if (!yaml_parser_parse(p, ev))
    {
        spark_log_error("editor_yaml: parse error at line %zu: %s",
            p->problem_mark.line + 1, p->problem);
        return -1;
    }
    return 0;
}

static int s_expect(yaml_parser_t *p, yaml_event_t *ev, yaml_event_type_t expected)
{
    if (s_next(p, ev) != 0) return -1;
    if (ev->type != expected)
    {
        yaml_event_delete(ev);
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
        if (ev.type == YAML_MAPPING_START_EVENT || ev.type == YAML_SEQUENCE_START_EVENT)
            depth++;
        else if (ev.type == YAML_MAPPING_END_EVENT || ev.type == YAML_SEQUENCE_END_EVENT)
        {
            depth--;
            if (depth <= 0) { yaml_event_delete(&ev); return; }
        }
        else if (ev.type == YAML_SCALAR_EVENT && depth == 0)
        {
            yaml_event_delete(&ev);
            return;
        }
        yaml_event_delete(&ev);
    }
}

/* ---- Parse hardware config ---- */

static int s_parse_midi(yaml_parser_t *p, editor_hw_config_t *hw)
{
    yaml_event_t ev;
    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); return 0; }
        if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

        const char *key = s_scalar(&ev);
        yaml_event_t val_ev;

        if (strcmp(key, "device") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(hw->midi_device, s_scalar(&val_ev), sizeof(hw->midi_device) - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "mode") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(hw->midi_mode, s_scalar(&val_ev), sizeof(hw->midi_mode) - 1);
            yaml_event_delete(&val_ev);
        }
        else
        {
            yaml_event_delete(&ev);
            s_skip_value(p);
        }
    }
}

static int s_parse_dmx(yaml_parser_t *p, editor_hw_config_t *hw)
{
    yaml_event_t ev;
    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); return 0; }
        if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

        const char *key = s_scalar(&ev);
        yaml_event_t val_ev;

        if (strcmp(key, "device") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(hw->dmx_device, s_scalar(&val_ev), sizeof(hw->dmx_device) - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "backend") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(hw->dmx_backend, s_scalar(&val_ev), sizeof(hw->dmx_backend) - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "refresh-rate-hz") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            hw->dmx_refresh_hz = (uint8_t)atoi(s_scalar(&val_ev));
            yaml_event_delete(&val_ev);
        }
        else
        {
            yaml_event_delete(&ev);
            s_skip_value(p);
        }
    }
}

/* ---- Parse scene values (mapping of "fixture.channel": value) ---- */

static int s_parse_scene_values(yaml_parser_t *p, editor_scene_value_t *values,
                                uint8_t *count)
{
    yaml_event_t ev;
    *count = 0;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); return 0; }
        if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

        if (*count >= EDITOR_MAX_SCENE_VALUES)
        {
            yaml_event_delete(&ev);
            s_skip_value(p);
            continue;
        }

        editor_scene_value_t *v = &values[*count];
        strncpy(v->target, s_scalar(&ev), sizeof(v->target) - 1);
        yaml_event_delete(&ev);

        yaml_event_t val_ev;
        if (s_next(p, &val_ev) != 0) return -1;
        v->value = (uint8_t)atoi(s_scalar(&val_ev));
        yaml_event_delete(&val_ev);
        (*count)++;
    }
}

/* ---- Parse scene steps ---- */

static int s_parse_scene_steps(yaml_parser_t *p, editor_scene_step_t *steps,
                               uint8_t *count)
{
    yaml_event_t ev;
    *count = 0;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_SEQUENCE_END_EVENT) { yaml_event_delete(&ev); return 0; }
        if (ev.type != YAML_MAPPING_START_EVENT) { yaml_event_delete(&ev); return -1; }
        yaml_event_delete(&ev);

        if (*count >= EDITOR_MAX_SCENE_STEPS)
        {
            s_skip_value(p);
            continue;
        }

        editor_scene_step_t *step = &steps[*count];
        memset(step, 0, sizeof(*step));
        strncpy(step->transition, "hold", sizeof(step->transition) - 1);

        for (;;)
        {
            if (s_next(p, &ev) != 0) return -1;
            if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
            if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

            const char *key = s_scalar(&ev);
            yaml_event_t val_ev;

            if (strcmp(key, "duration-ms") == 0)
            {
                yaml_event_delete(&ev);
                if (s_next(p, &val_ev) != 0) return -1;
                step->duration_ms = (uint32_t)atoi(s_scalar(&val_ev));
                yaml_event_delete(&val_ev);
            }
            else if (strcmp(key, "transition") == 0)
            {
                yaml_event_delete(&ev);
                if (s_next(p, &val_ev) != 0) return -1;
                strncpy(step->transition, s_scalar(&val_ev), sizeof(step->transition) - 1);
                yaml_event_delete(&val_ev);
            }
            else if (strcmp(key, "values") == 0)
            {
                yaml_event_delete(&ev);
                if (s_expect(p, &ev, YAML_MAPPING_START_EVENT) != 0) return -1;
                yaml_event_delete(&ev);
                if (s_parse_scene_values(p, step->values, &step->value_count) != 0)
                    return -1;
            }
            else
            {
                yaml_event_delete(&ev);
                s_skip_value(p);
            }
        }
        (*count)++;
    }
}

/* ---- Parse a single scene ---- */

static int s_parse_scene(yaml_parser_t *p, editor_scene_t *scene)
{
    yaml_event_t ev;
    memset(scene, 0, sizeof(*scene));
    scene->enabled = true;
    strncpy(scene->type, "static", sizeof(scene->type) - 1);
    strncpy(scene->trigger_mode, "gate", sizeof(scene->trigger_mode) - 1);

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); return 0; }
        if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

        const char *key = s_scalar(&ev);
        yaml_event_t val_ev;

        if (strcmp(key, "id") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(scene->id, s_scalar(&val_ev), SPARK_MAX_ID_SIZE - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "name") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(scene->name, s_scalar(&val_ev), SPARK_MAX_NAME_SIZE - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "type") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(scene->type, s_scalar(&val_ev), sizeof(scene->type) - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "enabled") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            scene->enabled = (strcmp(s_scalar(&val_ev), "false") != 0);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "loop") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            scene->loop = (strcmp(s_scalar(&val_ev), "true") == 0);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "trigger") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_MAPPING_START_EVENT) != 0) return -1;
            yaml_event_delete(&ev);

            for (;;)
            {
                if (s_next(p, &ev) != 0) return -1;
                if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
                if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

                const char *tkey = s_scalar(&ev);
                if (strcmp(tkey, "channel") == 0)
                {
                    yaml_event_delete(&ev);
                    if (s_next(p, &val_ev) != 0) return -1;
                    scene->channel = (uint8_t)atoi(s_scalar(&val_ev));
                    yaml_event_delete(&val_ev);
                }
                else if (strcmp(tkey, "note") == 0)
                {
                    yaml_event_delete(&ev);
                    if (s_next(p, &val_ev) != 0) return -1;
                    scene->note = (uint8_t)atoi(s_scalar(&val_ev));
                    yaml_event_delete(&val_ev);
                }
                else if (strcmp(tkey, "mode") == 0)
                {
                    yaml_event_delete(&ev);
                    if (s_next(p, &val_ev) != 0) return -1;
                    strncpy(scene->trigger_mode, s_scalar(&val_ev), sizeof(scene->trigger_mode) - 1);
                    yaml_event_delete(&val_ev);
                }
                else
                {
                    yaml_event_delete(&ev);
                    s_skip_value(p);
                }
            }
        }
        else if (strcmp(key, "values") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_MAPPING_START_EVENT) != 0) return -1;
            yaml_event_delete(&ev);
            if (s_parse_scene_values(p, scene->values, &scene->value_count) != 0)
                return -1;
        }
        else if (strcmp(key, "steps") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SEQUENCE_START_EVENT) != 0) return -1;
            yaml_event_delete(&ev);
            if (s_parse_scene_steps(p, scene->steps, &scene->step_count) != 0)
                return -1;
        }
        else
        {
            yaml_event_delete(&ev);
            s_skip_value(p);
        }
    }
}

/* ---- Parse project fixtures ---- */

static int s_parse_channels(yaml_parser_t *p, editor_channel_t *channels,
                            uint8_t *count)
{
    yaml_event_t ev;
    *count = 0;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_SEQUENCE_END_EVENT) { yaml_event_delete(&ev); return 0; }
        if (ev.type != YAML_MAPPING_START_EVENT) { yaml_event_delete(&ev); return -1; }
        yaml_event_delete(&ev);

        if (*count >= EDITOR_MAX_CHANNELS) return -1;
        editor_channel_t *ch = &channels[*count];
        memset(ch, 0, sizeof(*ch));

        for (;;)
        {
            if (s_next(p, &ev) != 0) return -1;
            if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
            if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

            const char *key = s_scalar(&ev);
            yaml_event_t val_ev;

            if (strcmp(key, "name") == 0)
            {
                yaml_event_delete(&ev);
                if (s_next(p, &val_ev) != 0) return -1;
                strncpy(ch->name, s_scalar(&val_ev), SPARK_MAX_NAME_SIZE - 1);
                yaml_event_delete(&val_ev);
            }
            else if (strcmp(key, "offset") == 0)
            {
                yaml_event_delete(&ev);
                if (s_next(p, &val_ev) != 0) return -1;
                ch->offset = (uint8_t)atoi(s_scalar(&val_ev));
                yaml_event_delete(&val_ev);
            }
            else
            {
                yaml_event_delete(&ev);
                s_skip_value(p);
            }
        }
        (*count)++;
    }
}

static int s_parse_fixture(yaml_parser_t *p, editor_fixture_t *fix)
{
    yaml_event_t ev;
    memset(fix, 0, sizeof(*fix));

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
        if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

        const char *key = s_scalar(&ev);
        yaml_event_t val_ev;

        if (strcmp(key, "id") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(fix->id, s_scalar(&val_ev), SPARK_MAX_ID_SIZE - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "name") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(fix->name, s_scalar(&val_ev), SPARK_MAX_NAME_SIZE - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "start-address") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            fix->start_address = (uint16_t)atoi(s_scalar(&val_ev));
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "channel-count") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            fix->channel_count = (uint8_t)atoi(s_scalar(&val_ev));
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "template") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(fix->template_ref, s_scalar(&val_ev), sizeof(fix->template_ref) - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "copy-from") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(fix->copy_from, s_scalar(&val_ev), SPARK_MAX_ID_SIZE - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "channels") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SEQUENCE_START_EVENT) != 0) return -1;
            yaml_event_delete(&ev);
            if (s_parse_channels(p, fix->channels, &fix->channel_count) != 0)
                return -1;
        }
        else
        {
            yaml_event_delete(&ev);
            s_skip_value(p);
        }
    }
    return 0;
}

int editor_yaml_parse_project(const char *path, editor_project_t *project)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        spark_log_error("editor: cannot open '%s'", path);
        return -1;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0)
    {
        fclose(f);
        return -1;
    }

    char *buf = (char *)malloc((size_t)file_size + 1);
    if (!buf) { fclose(f); return -1; }
    size_t nread = fread(buf, 1, (size_t)file_size, f);
    buf[nread] = '\0';
    fclose(f);

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser))
    {
        free(buf);
        return -1;
    }
    yaml_parser_set_input_string(&parser, (const unsigned char *)buf, nread);

    yaml_event_t ev;
    int rc = -1;

    memset(project, 0, sizeof(*project));
    strncpy(project->path, path, EDITOR_PATH_MAX - 1);
    project->raw_buf = buf;
    project->raw_buf_len = nread;

    /* stream start + document start */
    if (s_expect(&parser, &ev, YAML_STREAM_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);
    if (s_expect(&parser, &ev, YAML_DOCUMENT_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);
    if (s_expect(&parser, &ev, YAML_MAPPING_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);

    size_t prev_section_start = 0;
    char prev_key[64] = {0};
    int prev_is_structured = 0;
    int has_prev = 0;

    for (;;)
    {
        if (s_next(&parser, &ev) != 0) goto done;

        if (ev.type == YAML_MAPPING_END_EVENT)
        {
            if (has_prev && !prev_is_structured &&
                project->raw_section_count < EDITOR_MAX_RAW_SECTIONS)
            {
                editor_raw_section_t *sec = &project->raw_sections[project->raw_section_count];
                strncpy(sec->key, prev_key, sizeof(sec->key) - 1);
                sec->start = prev_section_start;
                sec->len = ev.start_mark.index - prev_section_start;
                project->raw_section_count++;
            }
            yaml_event_delete(&ev);
            break;
        }

        if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); goto done; }

        size_t this_key_start = ev.start_mark.index;
        if (has_prev && !prev_is_structured &&
            project->raw_section_count < EDITOR_MAX_RAW_SECTIONS)
        {
            editor_raw_section_t *sec = &project->raw_sections[project->raw_section_count];
            strncpy(sec->key, prev_key, sizeof(sec->key) - 1);
            sec->start = prev_section_start;
            sec->len = this_key_start - prev_section_start;
            project->raw_section_count++;
        }

        const char *key = s_scalar(&ev);
        prev_section_start = this_key_start;
        strncpy(prev_key, key, sizeof(prev_key) - 1);
        prev_key[sizeof(prev_key) - 1] = '\0';
        has_prev = 1;

        if (strcmp(key, "fixtures") == 0)
        {
            prev_is_structured = 1;
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_SEQUENCE_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);

            for (;;)
            {
                if (s_next(&parser, &ev) != 0) goto done;
                if (ev.type == YAML_SEQUENCE_END_EVENT) { yaml_event_delete(&ev); break; }
                if (ev.type != YAML_MAPPING_START_EVENT) { yaml_event_delete(&ev); goto done; }
                yaml_event_delete(&ev);

                if (project->fixture_count >= EDITOR_MAX_FIXTURES)
                {
                    spark_log_error("editor: too many fixtures");
                    goto done;
                }
                if (s_parse_fixture(&parser, &project->fixtures[project->fixture_count]) != 0)
                    goto done;
                project->fixture_count++;
            }
        }
        else if (strcmp(key, "midi") == 0)
        {
            prev_is_structured = 1;
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_MAPPING_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);
            if (s_parse_midi(&parser, &project->hw) != 0) goto done;
        }
        else if (strcmp(key, "dmx") == 0)
        {
            prev_is_structured = 1;
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_MAPPING_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);
            if (s_parse_dmx(&parser, &project->hw) != 0) goto done;
        }
        else if (strcmp(key, "scenes") == 0)
        {
            prev_is_structured = 1;
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_SEQUENCE_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);

            for (;;)
            {
                if (s_next(&parser, &ev) != 0) goto done;
                if (ev.type == YAML_SEQUENCE_END_EVENT) { yaml_event_delete(&ev); break; }
                if (ev.type != YAML_MAPPING_START_EVENT) { yaml_event_delete(&ev); goto done; }
                yaml_event_delete(&ev);

                if (project->scene_count >= EDITOR_MAX_SCENES)
                {
                    spark_log_error("editor: too many scenes");
                    goto done;
                }
                if (s_parse_scene(&parser, &project->scenes[project->scene_count]) != 0)
                    goto done;
                project->scene_count++;
            }
        }
        else
        {
            prev_is_structured = 0;
            yaml_event_delete(&ev);
            s_skip_value(&parser);
        }
    }

    project->loaded = true;
    rc = 0;

done:
    yaml_parser_delete(&parser);
    if (rc != 0)
    {
        free(buf);
        project->raw_buf = NULL;
        project->raw_buf_len = 0;
    }
    return rc;
}

/* ---- Parse bank file ---- */

static int s_parse_bank_fixture(yaml_parser_t *p, editor_bank_fixture_t *fix)
{
    yaml_event_t ev;
    memset(fix, 0, sizeof(*fix));

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
        if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

        const char *key = s_scalar(&ev);
        yaml_event_t val_ev;

        if (strcmp(key, "id") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(fix->id, s_scalar(&val_ev), SPARK_MAX_ID_SIZE - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "name") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            strncpy(fix->name, s_scalar(&val_ev), SPARK_MAX_NAME_SIZE - 1);
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "channel-count") == 0)
        {
            yaml_event_delete(&ev);
            if (s_next(p, &val_ev) != 0) return -1;
            fix->channel_count = (uint8_t)atoi(s_scalar(&val_ev));
            yaml_event_delete(&val_ev);
        }
        else if (strcmp(key, "channels") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(p, &ev, YAML_SEQUENCE_START_EVENT) != 0) return -1;
            yaml_event_delete(&ev);
            if (s_parse_channels(p, fix->channels, &fix->channel_count) != 0)
                return -1;
        }
        else
        {
            yaml_event_delete(&ev);
            s_skip_value(p);
        }
    }
    return 0;
}

int editor_yaml_parse_bank(const char *path, editor_bank_t *bank)
{
    FILE *f = fopen(path, "rb");
    if (!f)
    {
        spark_log_error("editor: cannot open bank '%s'", path);
        return -1;
    }

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser))
    {
        fclose(f);
        return -1;
    }
    yaml_parser_set_input_file(&parser, f);

    yaml_event_t ev;
    int rc = -1;

    memset(bank, 0, sizeof(*bank));
    strncpy(bank->path, path, EDITOR_PATH_MAX - 1);

    if (s_expect(&parser, &ev, YAML_STREAM_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);
    if (s_expect(&parser, &ev, YAML_DOCUMENT_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);
    if (s_expect(&parser, &ev, YAML_MAPPING_START_EVENT) != 0) goto done;
    yaml_event_delete(&ev);

    for (;;)
    {
        if (s_next(&parser, &ev) != 0) goto done;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
        if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); goto done; }

        const char *key = s_scalar(&ev);

        if (strcmp(key, "bank") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_MAPPING_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);

            for (;;)
            {
                if (s_next(&parser, &ev) != 0) goto done;
                if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
                if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); goto done; }

                const char *bkey = s_scalar(&ev);
                yaml_event_t val_ev;

                if (strcmp(bkey, "id") == 0)
                {
                    yaml_event_delete(&ev);
                    if (s_next(&parser, &val_ev) != 0) goto done;
                    strncpy(bank->id, s_scalar(&val_ev), SPARK_MAX_ID_SIZE - 1);
                    yaml_event_delete(&val_ev);
                }
                else if (strcmp(bkey, "version") == 0)
                {
                    yaml_event_delete(&ev);
                    if (s_next(&parser, &val_ev) != 0) goto done;
                    bank->version = (uint16_t)atoi(s_scalar(&val_ev));
                    yaml_event_delete(&val_ev);
                }
                else
                {
                    yaml_event_delete(&ev);
                    s_skip_value(&parser);
                }
            }
        }
        else if (strcmp(key, "fixtures") == 0)
        {
            yaml_event_delete(&ev);
            if (s_expect(&parser, &ev, YAML_SEQUENCE_START_EVENT) != 0) goto done;
            yaml_event_delete(&ev);

            for (;;)
            {
                if (s_next(&parser, &ev) != 0) goto done;
                if (ev.type == YAML_SEQUENCE_END_EVENT) { yaml_event_delete(&ev); break; }
                if (ev.type != YAML_MAPPING_START_EVENT) { yaml_event_delete(&ev); goto done; }
                yaml_event_delete(&ev);

                if (bank->fixture_count >= EDITOR_MAX_BANK_FIXTURES)
                {
                    spark_log_error("editor: too many bank fixtures");
                    goto done;
                }
                if (s_parse_bank_fixture(&parser, &bank->fixtures[bank->fixture_count]) != 0)
                    goto done;
                bank->fixture_count++;
            }
        }
        else
        {
            yaml_event_delete(&ev);
            s_skip_value(&parser);
        }
    }

    rc = 0;

done:
    yaml_parser_delete(&parser);
    fclose(f);
    return rc;
}

/* ---- Emitting helpers ---- */

static int s_emit_event(yaml_emitter_t *e, yaml_event_t *ev)
{
    if (!yaml_emitter_emit(e, ev))
    {
        spark_log_error("editor_yaml: emit error");
        return -1;
    }
    return 0;
}

static int s_emit_scalar(yaml_emitter_t *e, const char *value)
{
    yaml_event_t ev;
    yaml_scalar_event_initialize(&ev, NULL, NULL,
        (yaml_char_t *)value, (int)strlen(value),
        1, 1, YAML_PLAIN_SCALAR_STYLE);
    return s_emit_event(e, &ev);
}

static int s_emit_scalar_quoted(yaml_emitter_t *e, const char *value)
{
    yaml_event_t ev;
    yaml_scalar_event_initialize(&ev, NULL, NULL,
        (yaml_char_t *)value, (int)strlen(value),
        0, 1, YAML_DOUBLE_QUOTED_SCALAR_STYLE);
    return s_emit_event(e, &ev);
}

static int s_emit_int(yaml_emitter_t *e, int value)
{
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", value);
    return s_emit_scalar(e, buf);
}

static int s_emit_mapping_start(yaml_emitter_t *e)
{
    yaml_event_t ev;
    yaml_mapping_start_event_initialize(&ev, NULL, NULL, 1, YAML_BLOCK_MAPPING_STYLE);
    return s_emit_event(e, &ev);
}

static int s_emit_mapping_end(yaml_emitter_t *e)
{
    yaml_event_t ev;
    yaml_mapping_end_event_initialize(&ev);
    return s_emit_event(e, &ev);
}

static int s_emit_sequence_start(yaml_emitter_t *e)
{
    yaml_event_t ev;
    yaml_sequence_start_event_initialize(&ev, NULL, NULL, 1, YAML_BLOCK_SEQUENCE_STYLE);
    return s_emit_event(e, &ev);
}

static int s_emit_sequence_end(yaml_emitter_t *e)
{
    yaml_event_t ev;
    yaml_sequence_end_event_initialize(&ev);
    return s_emit_event(e, &ev);
}

/* ---- Emit project YAML ---- */

static const char *s_key_order[] = {
    "format", "app", "midi", "dmx", "fixtures", "scenes", NULL
};

static const editor_raw_section_t *s_find_raw_section(
    const editor_project_t *project, const char *key)
{
    for (uint16_t i = 0; i < project->raw_section_count; i++)
    {
        if (strcmp(project->raw_sections[i].key, key) == 0)
            return &project->raw_sections[i];
    }
    return NULL;
}

/* ---- Section emitters (produce YAML text into a buffer) ---- */

static void s_buf_emit_fixtures(const editor_project_t *project,
                                char *buf, size_t cap, size_t *pos)
{
    *pos += (size_t)snprintf(buf + *pos, cap - *pos, "fixtures:\n");
    for (uint16_t i = 0; i < project->fixture_count; i++)
    {
        const editor_fixture_t *fix = &project->fixtures[i];
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "  - id: %s\n", fix->id);
        if (fix->name[0] != '\0')
            *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    name: %s\n", fix->name);
        if (fix->start_address > 0)
            *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    start-address: %d\n", fix->start_address);

        if (fix->template_ref[0] != '\0')
            *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    template: \"%s\"\n", fix->template_ref);
        else if (fix->copy_from[0] != '\0')
            *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    copy-from: \"%s\"\n", fix->copy_from);
        else if (fix->channel_count > 0)
        {
            *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    channel-count: %d\n", fix->channel_count);
            *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    channels:\n");
            for (uint8_t j = 0; j < fix->channel_count; j++)
            {
                *pos += (size_t)snprintf(buf + *pos, cap - *pos,
                    "      - name: %s\n        offset: %d\n",
                    fix->channels[j].name, fix->channels[j].offset);
            }
        }
    }
}

static void s_buf_emit_midi(const editor_hw_config_t *hw,
                            char *buf, size_t cap, size_t *pos)
{
    if (hw->midi_device[0] == '\0' && hw->midi_mode[0] == '\0') return;
    *pos += (size_t)snprintf(buf + *pos, cap - *pos, "midi:\n");
    if (hw->midi_device[0] != '\0')
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "  device: %s\n", hw->midi_device);
    if (hw->midi_mode[0] != '\0')
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "  mode: %s\n", hw->midi_mode);
}

static void s_buf_emit_dmx(const editor_hw_config_t *hw,
                           char *buf, size_t cap, size_t *pos)
{
    if (hw->dmx_device[0] == '\0' && hw->dmx_backend[0] == '\0' && hw->dmx_refresh_hz == 0)
        return;
    *pos += (size_t)snprintf(buf + *pos, cap - *pos, "dmx:\n");
    if (hw->dmx_device[0] != '\0')
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "  device: %s\n", hw->dmx_device);
    if (hw->dmx_backend[0] != '\0')
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "  backend: %s\n", hw->dmx_backend);
    if (hw->dmx_refresh_hz > 0)
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "  refresh-rate-hz: %d\n", hw->dmx_refresh_hz);
}

static void s_buf_emit_scene_values(const editor_scene_value_t *values, uint8_t count,
                                    const char *indent, char *buf, size_t cap, size_t *pos)
{
    *pos += (size_t)snprintf(buf + *pos, cap - *pos, "%svalues:\n", indent);
    for (uint8_t i = 0; i < count; i++)
    {
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "%s  %s: %d\n",
                                 indent, values[i].target, values[i].value);
    }
}

static void s_buf_emit_scenes(const editor_project_t *project,
                              char *buf, size_t cap, size_t *pos)
{
    *pos += (size_t)snprintf(buf + *pos, cap - *pos, "scenes:\n");
    for (uint16_t i = 0; i < project->scene_count; i++)
    {
        const editor_scene_t *sc = &project->scenes[i];
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "  - id: %s\n", sc->id);
        if (sc->name[0] != '\0')
            *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    name: %s\n", sc->name);
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    type: %s\n", sc->type);
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    trigger:\n");
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "      channel: %d\n", sc->channel);
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "      note: %d\n", sc->note);
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "      mode: %s\n", sc->trigger_mode);
        if (!sc->enabled)
            *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    enabled: false\n");
        if (sc->loop)
            *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    loop: true\n");

        if (strcmp(sc->type, "static") == 0 && sc->value_count > 0)
        {
            s_buf_emit_scene_values(sc->values, sc->value_count, "    ", buf, cap, pos);
        }
        else if (strcmp(sc->type, "sequence") == 0 && sc->step_count > 0)
        {
            *pos += (size_t)snprintf(buf + *pos, cap - *pos, "    steps:\n");
            for (uint8_t s = 0; s < sc->step_count; s++)
            {
                const editor_scene_step_t *step = &sc->steps[s];
                *pos += (size_t)snprintf(buf + *pos, cap - *pos, "      - duration-ms: %u\n", step->duration_ms);
                *pos += (size_t)snprintf(buf + *pos, cap - *pos, "        transition: %s\n", step->transition);
                if (step->value_count > 0)
                    s_buf_emit_scene_values(step->values, step->value_count, "        ", buf, cap, pos);
            }
        }
    }
}

int editor_yaml_emit_project(const char *path, const editor_project_t *project)
{
    size_t cap = 64 * 1024;
    char *buf = (char *)malloc(cap);
    if (!buf) return -1;
    size_t pos = 0;

    FILE *f = fopen(path, "wb");
    if (!f)
    {
        spark_log_error("editor: cannot write '%s'", path);
        free(buf);
        return -1;
    }

    int need_newline = 0;

    /* Write sections in canonical key order */
    for (int k = 0; s_key_order[k] != NULL; k++)
    {
        const char *section = s_key_order[k];

        if (strcmp(section, "fixtures") == 0)
        {
            if (project->fixture_count > 0)
            {
                if (need_newline) fputc('\n', f);
                pos = 0;
                s_buf_emit_fixtures(project, buf, cap, &pos);
                fwrite(buf, 1, pos, f);
                need_newline = 1;
            }
        }
        else if (strcmp(section, "midi") == 0)
        {
            if (project->hw.midi_device[0] != '\0' || project->hw.midi_mode[0] != '\0')
            {
                if (need_newline) fputc('\n', f);
                pos = 0;
                s_buf_emit_midi(&project->hw, buf, cap, &pos);
                fwrite(buf, 1, pos, f);
                need_newline = 1;
            }
        }
        else if (strcmp(section, "dmx") == 0)
        {
            if (project->hw.dmx_device[0] != '\0' || project->hw.dmx_backend[0] != '\0' || project->hw.dmx_refresh_hz > 0)
            {
                if (need_newline) fputc('\n', f);
                pos = 0;
                s_buf_emit_dmx(&project->hw, buf, cap, &pos);
                fwrite(buf, 1, pos, f);
                need_newline = 1;
            }
        }
        else if (strcmp(section, "scenes") == 0)
        {
            if (project->scene_count > 0)
            {
                if (need_newline) fputc('\n', f);
                pos = 0;
                s_buf_emit_scenes(project, buf, cap, &pos);
                fwrite(buf, 1, pos, f);
                need_newline = 1;
            }
        }
        else
        {
            /* Raw-preserved section (format, app) */
            const editor_raw_section_t *sec = s_find_raw_section(project, section);
            if (!sec) continue;

            if (need_newline)
            {
                const char *raw = project->raw_buf + sec->start;
                if (raw[0] != '\n') fputc('\n', f);
            }
            fwrite(project->raw_buf + sec->start, 1, sec->len, f);
            need_newline = 1;
        }
    }

    /* Append any raw sections not in s_key_order (forward compatibility) */
    for (uint16_t i = 0; i < project->raw_section_count; i++)
    {
        const editor_raw_section_t *sec = &project->raw_sections[i];
        int found = 0;
        for (int k = 0; s_key_order[k] != NULL; k++)
        {
            if (strcmp(sec->key, s_key_order[k]) == 0) { found = 1; break; }
        }
        if (found) continue;

        if (need_newline)
        {
            const char *raw = project->raw_buf + sec->start;
            if (raw[0] != '\n') fputc('\n', f);
        }
        fwrite(project->raw_buf + sec->start, 1, sec->len, f);
        need_newline = 1;
    }

    /* Fallback: no raw sections at all (brand new project) */
    if (!project->raw_buf || project->raw_section_count == 0)
    {
        if (pos == 0 && !need_newline)
            fprintf(f, "format:\n  name: spark-project\n  version: 1\n\n");
    }

    fclose(f);
    free(buf);
    return 0;
}

/* ---- Emit bank YAML ---- */

static int s_emit_bank_fixture(yaml_emitter_t *e, const editor_bank_fixture_t *fix)
{
    if (s_emit_mapping_start(e) != 0) return -1;

    if (s_emit_scalar(e, "id") != 0) return -1;
    if (s_emit_scalar(e, fix->id) != 0) return -1;

    if (fix->name[0] != '\0')
    {
        if (s_emit_scalar(e, "name") != 0) return -1;
        if (s_emit_scalar(e, fix->name) != 0) return -1;
    }

    if (fix->channel_count > 0)
    {
        if (s_emit_scalar(e, "channel-count") != 0) return -1;
        if (s_emit_int(e, fix->channel_count) != 0) return -1;
        if (s_emit_scalar(e, "channels") != 0) return -1;
        if (s_emit_sequence_start(e) != 0) return -1;

        for (uint8_t i = 0; i < fix->channel_count; i++)
        {
            if (s_emit_mapping_start(e) != 0) return -1;
            if (s_emit_scalar(e, "name") != 0) return -1;
            if (s_emit_scalar(e, fix->channels[i].name) != 0) return -1;
            if (s_emit_scalar(e, "offset") != 0) return -1;
            if (s_emit_int(e, fix->channels[i].offset) != 0) return -1;
            if (s_emit_mapping_end(e) != 0) return -1;
        }
        if (s_emit_sequence_end(e) != 0) return -1;
    }

    return s_emit_mapping_end(e);
}

int editor_yaml_emit_bank(const char *path, const editor_bank_t *bank)
{
    FILE *f = fopen(path, "wb");
    if (!f)
    {
        spark_log_error("editor: cannot write bank '%s'", path);
        return -1;
    }

    yaml_emitter_t emitter;
    if (!yaml_emitter_initialize(&emitter))
    {
        fclose(f);
        return -1;
    }
    yaml_emitter_set_output_file(&emitter, f);
    yaml_emitter_set_unicode(&emitter, 1);

    yaml_event_t ev;
    int rc = -1;

    yaml_stream_start_event_initialize(&ev, YAML_UTF8_ENCODING);
    if (s_emit_event(&emitter, &ev) != 0) goto done;

    yaml_document_start_event_initialize(&ev, NULL, NULL, NULL, 0);
    if (s_emit_event(&emitter, &ev) != 0) goto done;

    if (s_emit_mapping_start(&emitter) != 0) goto done;

    /* bank section */
    if (s_emit_scalar(&emitter, "bank") != 0) goto done;
    if (s_emit_mapping_start(&emitter) != 0) goto done;
    if (s_emit_scalar(&emitter, "id") != 0) goto done;
    if (s_emit_scalar_quoted(&emitter, bank->id) != 0) goto done;
    if (s_emit_scalar(&emitter, "version") != 0) goto done;
    if (s_emit_int(&emitter, bank->version > 0 ? bank->version : 1) != 0) goto done;
    if (s_emit_mapping_end(&emitter) != 0) goto done;

    /* fixtures section */
    if (s_emit_scalar(&emitter, "fixtures") != 0) goto done;
    if (s_emit_sequence_start(&emitter) != 0) goto done;

    for (uint16_t i = 0; i < bank->fixture_count; i++)
    {
        if (s_emit_bank_fixture(&emitter, &bank->fixtures[i]) != 0) goto done;
    }
    if (s_emit_sequence_end(&emitter) != 0) goto done;

    if (s_emit_mapping_end(&emitter) != 0) goto done;

    yaml_document_end_event_initialize(&ev, 0);
    if (s_emit_event(&emitter, &ev) != 0) goto done;

    yaml_stream_end_event_initialize(&ev);
    if (s_emit_event(&emitter, &ev) != 0) goto done;

    rc = 0;

done:
    yaml_emitter_delete(&emitter);
    fclose(f);
    return rc;
}
