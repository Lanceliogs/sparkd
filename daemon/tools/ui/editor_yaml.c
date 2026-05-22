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

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser))
    {
        fclose(f);
        return -1;
    }
    yaml_parser_set_input_file(&parser, f);

    yaml_event_t ev;
    int rc = -1;

    memset(project, 0, sizeof(*project));
    strncpy(project->path, path, EDITOR_PATH_MAX - 1);

    /* stream start + document start */
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

        if (strcmp(key, "fixtures") == 0)
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
        else
        {
            yaml_event_delete(&ev);
            s_skip_value(&parser);
        }
    }

    project->loaded = true;
    rc = 0;

done:
    yaml_parser_delete(&parser);
    fclose(f);
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

/* ---- Emit project YAML (fixtures section only) ---- */

static int s_emit_fixture_channels(yaml_emitter_t *e, const editor_channel_t *channels,
                                   uint8_t count)
{
    if (s_emit_scalar(e, "channels") != 0) return -1;
    if (s_emit_sequence_start(e) != 0) return -1;

    for (uint8_t i = 0; i < count; i++)
    {
        if (s_emit_mapping_start(e) != 0) return -1;
        if (s_emit_scalar(e, "name") != 0) return -1;
        if (s_emit_scalar(e, channels[i].name) != 0) return -1;
        if (s_emit_scalar(e, "offset") != 0) return -1;
        if (s_emit_int(e, channels[i].offset) != 0) return -1;
        if (s_emit_mapping_end(e) != 0) return -1;
    }

    return s_emit_sequence_end(e);
}

static int s_emit_project_fixture(yaml_emitter_t *e, const editor_fixture_t *fix)
{
    if (s_emit_mapping_start(e) != 0) return -1;

    if (s_emit_scalar(e, "id") != 0) return -1;
    if (s_emit_scalar(e, fix->id) != 0) return -1;

    if (fix->name[0] != '\0')
    {
        if (s_emit_scalar(e, "name") != 0) return -1;
        if (s_emit_scalar(e, fix->name) != 0) return -1;
    }

    if (fix->start_address > 0)
    {
        if (s_emit_scalar(e, "start-address") != 0) return -1;
        if (s_emit_int(e, fix->start_address) != 0) return -1;
    }

    if (fix->template_ref[0] != '\0')
    {
        if (s_emit_scalar(e, "template") != 0) return -1;
        if (s_emit_scalar_quoted(e, fix->template_ref) != 0) return -1;
    }
    else if (fix->copy_from[0] != '\0')
    {
        if (s_emit_scalar(e, "copy-from") != 0) return -1;
        if (s_emit_scalar_quoted(e, fix->copy_from) != 0) return -1;
    }
    else if (fix->channel_count > 0)
    {
        if (s_emit_scalar(e, "channel-count") != 0) return -1;
        if (s_emit_int(e, fix->channel_count) != 0) return -1;
        if (s_emit_fixture_channels(e, fix->channels, fix->channel_count) != 0) return -1;
    }

    return s_emit_mapping_end(e);
}

int editor_yaml_emit_project(const char *path, const editor_project_t *project)
{
    FILE *f = fopen(path, "wb");
    if (!f)
    {
        spark_log_error("editor: cannot write '%s'", path);
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

    /* format section */
    if (s_emit_scalar(&emitter, "format") != 0) goto done;
    if (s_emit_mapping_start(&emitter) != 0) goto done;
    if (s_emit_scalar(&emitter, "name") != 0) goto done;
    if (s_emit_scalar(&emitter, "spark-project") != 0) goto done;
    if (s_emit_scalar(&emitter, "version") != 0) goto done;
    if (s_emit_int(&emitter, 1) != 0) goto done;
    if (s_emit_mapping_end(&emitter) != 0) goto done;

    /* fixtures section */
    if (project->fixture_count > 0)
    {
        if (s_emit_scalar(&emitter, "fixtures") != 0) goto done;
        if (s_emit_sequence_start(&emitter) != 0) goto done;

        for (uint16_t i = 0; i < project->fixture_count; i++)
        {
            if (s_emit_project_fixture(&emitter, &project->fixtures[i]) != 0) goto done;
        }
        if (s_emit_sequence_end(&emitter) != 0) goto done;
    }

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
