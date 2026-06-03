#include "fixture_bank.h"
#include "fs.h"
#include "log.h"
#include "consts.h"

#include <yaml.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATH_LIST_SEP ';'

#define BANK_ID_SIZE 64

typedef struct {
    char id[BANK_ID_SIZE];
} bank_entry_t;

static bank_entry_t s_banks[SPARK_FIXTURE_BANK_MAX];
static int s_bank_count = 0;

static spark_fixture_t s_templates[SPARK_FIXTURE_BANK_TEMPLATES_MAX];
static int s_template_count = 0;

static spark_channel_def_t s_channel_arena[SPARK_FIXTURE_BANK_CHANNEL_ARENA_SIZE];
static int s_channel_arena_used = 0;

/* Qualified id: "bank-id:fixture-id" stored for lookup */
static char s_qualified_ids[SPARK_FIXTURE_BANK_TEMPLATES_MAX][BANK_ID_SIZE + SPARK_MAX_ID_SIZE + 2];

static spark_channel_def_t *s_alloc_channels(int count)
{
    if (s_channel_arena_used + count > SPARK_FIXTURE_BANK_CHANNEL_ARENA_SIZE)
        return NULL;
    spark_channel_def_t *ptr = &s_channel_arena[s_channel_arena_used];
    s_channel_arena_used += count;
    return ptr;
}

static int s_bank_id_exists(const char *id)
{
    for (int i = 0; i < s_bank_count; i++)
        if (strcmp(s_banks[i].id, id) == 0)
            return 1;
    return 0;
}

/* ---- Minimal YAML parsing for bank files ---- */

static const char *s_scalar(yaml_event_t *ev)
{
    return (const char *)ev->data.scalar.value;
}

static int s_next(yaml_parser_t *p, yaml_event_t *ev)
{
    if (!yaml_parser_parse(p, ev))
    {
        spark_log_error("fixture_bank: YAML parse error");
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

static int s_parse_channels(yaml_parser_t *p, spark_channel_def_t *channels,
                            uint8_t *count, uint8_t max)
{
    yaml_event_t ev;
    *count = 0;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_SEQUENCE_END_EVENT) { yaml_event_delete(&ev); return 0; }
        if (ev.type != YAML_MAPPING_START_EVENT) { yaml_event_delete(&ev); return -1; }
        yaml_event_delete(&ev);

        if (*count >= max) return -1;
        spark_channel_def_t *ch = &channels[*count];
        memset(ch, 0, sizeof(*ch));

        for (;;)
        {
            if (s_next(p, &ev) != 0) return -1;
            if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
            if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

            char key[32];
            strncpy(key, s_scalar(&ev), sizeof(key) - 1);
            key[sizeof(key) - 1] = '\0';
            yaml_event_delete(&ev);

            if (s_next(p, &ev) != 0) return -1;
            if (strcmp(key, "name") == 0)
                strncpy(ch->name, s_scalar(&ev), SPARK_MAX_NAME_SIZE - 1);
            else if (strcmp(key, "offset") == 0)
                ch->offset = (uint8_t)atoi(s_scalar(&ev));
            yaml_event_delete(&ev);
        }
        (*count)++;
    }
}

static int s_parse_bank_fixture(yaml_parser_t *p, const char *bank_id)
{
    yaml_event_t ev;
    char id[SPARK_MAX_ID_SIZE] = {0};
    spark_channel_def_t channels[64];
    uint8_t channel_count = 0;
    uint8_t declared_count = 0;

    for (;;)
    {
        if (s_next(p, &ev) != 0) return -1;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
        if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); return -1; }

        char key[32];
        strncpy(key, s_scalar(&ev), sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';
        yaml_event_delete(&ev);

        if (strcmp(key, "id") == 0)
        {
            if (s_next(p, &ev) != 0) return -1;
            strncpy(id, s_scalar(&ev), SPARK_MAX_ID_SIZE - 1);
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "channel-count") == 0)
        {
            if (s_next(p, &ev) != 0) return -1;
            declared_count = (uint8_t)atoi(s_scalar(&ev));
            yaml_event_delete(&ev);
        }
        else if (strcmp(key, "channels") == 0)
        {
            if (s_next(p, &ev) != 0) return -1;
            if (ev.type != YAML_SEQUENCE_START_EVENT) { yaml_event_delete(&ev); return -1; }
            yaml_event_delete(&ev);
            if (s_parse_channels(p, channels, &channel_count, 64) != 0)
                return -1;
        }
        else
        {
            s_skip_value(p);
        }
    }

    if (id[0] == '\0')
    {
        spark_log_warn("fixture_bank: fixture in bank '%s' missing 'id', skipped", bank_id);
        return 0;
    }

    if (declared_count == 0 && channel_count == 0)
    {
        spark_log_warn("fixture_bank: fixture '%s:%s' has no channels, skipped", bank_id, id);
        return 0;
    }

    if (s_template_count >= SPARK_FIXTURE_BANK_TEMPLATES_MAX)
    {
        spark_log_warn("fixture_bank: template limit reached (%d)", SPARK_FIXTURE_BANK_TEMPLATES_MAX);
        return -1;
    }

    uint8_t final_count = channel_count > 0 ? channel_count : declared_count;
    spark_channel_def_t *arena_ch = s_alloc_channels(final_count);
    if (!arena_ch)
    {
        spark_log_error("fixture_bank: channel arena exhausted");
        return -1;
    }

    if (channel_count > 0)
        memcpy(arena_ch, channels, channel_count * sizeof(spark_channel_def_t));

    spark_fixture_t *tmpl = &s_templates[s_template_count];
    memset(tmpl, 0, sizeof(*tmpl));
    strncpy(tmpl->id, id, SPARK_MAX_ID_SIZE - 1);
    tmpl->channel_count = final_count;
    tmpl->channels = arena_ch;

    snprintf(s_qualified_ids[s_template_count], sizeof(s_qualified_ids[0]),
             "%s:%s", bank_id, id);

    s_template_count++;

    spark_log_debug("fixture_bank: loaded template '%s:%s' (%u channels)",
                    bank_id, id, final_count);
    return 0;
}

static int s_parse_bank_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    yaml_parser_t parser;
    if (!yaml_parser_initialize(&parser))
    {
        fclose(f);
        return -1;
    }
    yaml_parser_set_input_file(&parser, f);

    yaml_event_t ev;
    char bank_id[BANK_ID_SIZE] = {0};
    int found_fixtures = 0;

    /* Consume stream start + document start */
    if (s_next(&parser, &ev) != 0) goto fail;
    yaml_event_delete(&ev);
    if (s_next(&parser, &ev) != 0) goto fail;
    yaml_event_delete(&ev);

    /* Expect top-level mapping */
    if (s_next(&parser, &ev) != 0) goto fail;
    if (ev.type != YAML_MAPPING_START_EVENT) { yaml_event_delete(&ev); goto fail; }
    yaml_event_delete(&ev);

    /* Parse top-level keys */
    for (;;)
    {
        if (s_next(&parser, &ev) != 0) goto fail;
        if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
        if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); goto fail; }

        char section[32];
        strncpy(section, s_scalar(&ev), sizeof(section) - 1);
        section[sizeof(section) - 1] = '\0';
        yaml_event_delete(&ev);

        if (strcmp(section, "bank") == 0)
        {
            /* Parse bank mapping for id */
            if (s_next(&parser, &ev) != 0) goto fail;
            if (ev.type != YAML_MAPPING_START_EVENT) { yaml_event_delete(&ev); goto fail; }
            yaml_event_delete(&ev);

            for (;;)
            {
                if (s_next(&parser, &ev) != 0) goto fail;
                if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
                if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); goto fail; }

                char key[32];
                strncpy(key, s_scalar(&ev), sizeof(key) - 1);
                key[sizeof(key) - 1] = '\0';
                yaml_event_delete(&ev);

                if (s_next(&parser, &ev) != 0) goto fail;
                if (strcmp(key, "id") == 0)
                    strncpy(bank_id, s_scalar(&ev), BANK_ID_SIZE - 1);
                yaml_event_delete(&ev);
            }
        }
        else if (strcmp(section, "fixtures") == 0)
        {
            found_fixtures = 1;

            if (s_next(&parser, &ev) != 0) goto fail;
            if (ev.type != YAML_SEQUENCE_START_EVENT) { yaml_event_delete(&ev); goto fail; }
            yaml_event_delete(&ev);

            /* Don't parse fixtures yet if we haven't seen bank id */
            /* We'll parse them now since YAML is sequential */
            if (bank_id[0] == '\0')
            {
                spark_log_warn("fixture_bank: 'fixtures' before 'bank' in %s, "
                               "place 'bank:' section first", path);
                /* Skip the fixtures sequence */
                int depth = 1;
                while (depth > 0)
                {
                    if (s_next(&parser, &ev) != 0) goto fail;
                    if (ev.type == YAML_SEQUENCE_START_EVENT || ev.type == YAML_MAPPING_START_EVENT)
                        depth++;
                    else if (ev.type == YAML_SEQUENCE_END_EVENT || ev.type == YAML_MAPPING_END_EVENT)
                        depth--;
                    yaml_event_delete(&ev);
                }
                found_fixtures = 0;
                continue;
            }

            for (;;)
            {
                if (s_next(&parser, &ev) != 0) goto fail;
                if (ev.type == YAML_SEQUENCE_END_EVENT) { yaml_event_delete(&ev); break; }
                if (ev.type != YAML_MAPPING_START_EVENT) { yaml_event_delete(&ev); goto fail; }
                yaml_event_delete(&ev);

                if (s_parse_bank_fixture(&parser, bank_id) != 0)
                    goto fail;
            }
        }
        else
        {
            s_skip_value(&parser);
        }
    }

    yaml_parser_delete(&parser);
    fclose(f);

    if (bank_id[0] == '\0')
    {
        spark_log_warn("fixture_bank: no 'bank.id' in %s, skipped", path);
        return 0;
    }

    if (!found_fixtures)
    {
        spark_log_warn("fixture_bank: no 'fixtures' in %s", path);
    }

    return 0;

fail:
    yaml_parser_delete(&parser);
    fclose(f);
    return -1;
}

static int s_is_yaml_file(const char *name)
{
    size_t len = strlen(name);
    if (len >= 5 && strcmp(name + len - 5, ".yaml") == 0) return 1;
    if (len >= 4 && strcmp(name + len - 4, ".yml") == 0) return 1;
    return 0;
}

static int s_process_bank_entry(const char *dir_path, const char *filename)
{
    if (!s_is_yaml_file(filename))
        return 0;

    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s/%s", dir_path, filename);
    spark_log_debug("fixture_bank: found file '%s'", filepath);

    FILE *f = fopen(filepath, "r");
    if (!f) { spark_log_warn("fixture_bank: cannot open '%s'", filepath); return 0; }

    yaml_parser_t peek;
    if (!yaml_parser_initialize(&peek)) { fclose(f); return 0; }
    yaml_parser_set_input_file(&peek, f);

    yaml_event_t ev;
    char bank_id[BANK_ID_SIZE] = {0};

    int ok = 1;
    for (int i = 0; i < 3 && ok; i++)
    {
        if (s_next(&peek, &ev) != 0) { ok = 0; break; }
        yaml_event_delete(&ev);
    }

    if (ok)
    {
        for (int attempts = 0; attempts < 10; attempts++)
        {
            if (s_next(&peek, &ev) != 0) break;
            if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
            if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); break; }

            if (strcmp(s_scalar(&ev), "bank") == 0)
            {
                yaml_event_delete(&ev);
                if (s_next(&peek, &ev) != 0) break;
                if (ev.type != YAML_MAPPING_START_EVENT) { yaml_event_delete(&ev); break; }
                yaml_event_delete(&ev);

                for (;;)
                {
                    if (s_next(&peek, &ev) != 0) break;
                    if (ev.type == YAML_MAPPING_END_EVENT) { yaml_event_delete(&ev); break; }
                    if (ev.type != YAML_SCALAR_EVENT) { yaml_event_delete(&ev); break; }
                    char k[32];
                    strncpy(k, s_scalar(&ev), sizeof(k) - 1);
                    k[sizeof(k) - 1] = '\0';
                    yaml_event_delete(&ev);

                    if (s_next(&peek, &ev) != 0) break;
                    if (strcmp(k, "id") == 0)
                        strncpy(bank_id, s_scalar(&ev), BANK_ID_SIZE - 1);
                    yaml_event_delete(&ev);
                }
                break;
            }
            else
            {
                yaml_event_delete(&ev);
                if (s_next(&peek, &ev) != 0) break;
                if (ev.type == YAML_MAPPING_START_EVENT || ev.type == YAML_SEQUENCE_START_EVENT)
                {
                    int depth = 1;
                    yaml_event_delete(&ev);
                    while (depth > 0)
                    {
                        if (s_next(&peek, &ev) != 0) { depth = 0; break; }
                        if (ev.type == YAML_MAPPING_START_EVENT || ev.type == YAML_SEQUENCE_START_EVENT) depth++;
                        else if (ev.type == YAML_MAPPING_END_EVENT || ev.type == YAML_SEQUENCE_END_EVENT) depth--;
                        yaml_event_delete(&ev);
                    }
                }
                else
                {
                    yaml_event_delete(&ev);
                }
            }
        }
    }

    yaml_parser_delete(&peek);
    fclose(f);

    if (bank_id[0] == '\0')
    {
        spark_log_warn("fixture_bank: no 'bank.id' in %s, skipped", filepath);
        return 0;
    }

    if (s_bank_id_exists(bank_id))
    {
        spark_log_warn("fixture_bank: duplicate bank id '%s' in %s, skipped",
                       bank_id, filepath);
        return 0;
    }

    if (s_bank_count >= SPARK_FIXTURE_BANK_MAX)
    {
        spark_log_warn("fixture_bank: max banks reached (%d)", SPARK_FIXTURE_BANK_MAX);
        return 0;
    }

    strncpy(s_banks[s_bank_count].id, bank_id, BANK_ID_SIZE - 1);
    s_bank_count++;

    if (s_parse_bank_file(filepath) != 0)
        spark_log_warn("fixture_bank: errors parsing %s", filepath);

    return 0;
}

struct scan_ctx {
    const char *dir_path;
};

static void s_scan_cb(const char *name, int is_dir, void *ctx)
{
    if (is_dir) return;
    struct scan_ctx *sc = (struct scan_ctx *)ctx;
    s_process_bank_entry(sc->dir_path, name);
}

static int s_scan_directory(const char *dir_path)
{
    spark_log_debug("fixture_bank: scanning '%s'", dir_path);
    struct scan_ctx ctx = { .dir_path = dir_path };
    int rc = spark_fs_list_dir(dir_path, s_scan_cb, &ctx);
    if (rc != 0)
        spark_log_warn("fixture_bank: cannot open directory '%s'", dir_path);
    return rc;
}

int spark_fixture_bank_load(const char *search_paths)
{
    spark_log_debug("fixture_bank: search_paths='%s'", search_paths ? search_paths : "(null)");

    if (!search_paths || search_paths[0] == '\0')
    {
        char home[1024];
        if (spark_fs_home(home, sizeof(home)) != 0) return 0;
        char default_path[1024];
        spark_fs_path_join(default_path, sizeof(default_path), home, ".spark/fixtures");
        s_scan_directory(default_path);
        return 0;
    }

    /* Split on PATH_LIST_SEP and scan each directory */
    char buf[4096];
    strncpy(buf, search_paths, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(buf, (char[]){PATH_LIST_SEP, '\0'}, &saveptr);
    while (token)
    {
        /* Trim leading/trailing spaces */
        while (*token == ' ') token++;
        size_t tlen = strlen(token);
        while (tlen > 0 && token[tlen - 1] == ' ') token[--tlen] = '\0';

        if (tlen > 0)
            s_scan_directory(token);

        token = strtok_r(NULL, (char[]){PATH_LIST_SEP, '\0'}, &saveptr);
    }

    spark_log_info("fixture_bank: loaded %d banks, %d templates", s_bank_count, s_template_count);
    return 0;
}

int spark_fixture_bank_reload(const char *search_paths)
{
    spark_fixture_bank_reset();
    return spark_fixture_bank_load(search_paths);
}

const spark_fixture_t *spark_fixture_bank_find(const char *qualified_id)
{
    for (int i = 0; i < s_template_count; i++)
    {
        if (strcmp(s_qualified_ids[i], qualified_id) == 0)
            return &s_templates[i];
    }
    return NULL;
}

void spark_fixture_bank_reset(void)
{
    s_bank_count = 0;
    s_template_count = 0;
    s_channel_arena_used = 0;
}
