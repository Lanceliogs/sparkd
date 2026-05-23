#include "editor_http.h"
#include "mg_helpers.h"
#include "editor.h"
#include "editor_places.h"
#include "env.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

static editor_state_t s_editor;

void editor_http_init(const char *bank_paths)
{
    editor_load_banks(&s_editor, bank_paths);
}

/* ---- Helpers ---- */

static void s_escape_json_str(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 2; i++)
    {
        if (src[i] == '\\' || src[i] == '"')
            dst[j++] = '\\';
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

/* ---- Project lifecycle ---- */

static void s_handle_status(struct mg_connection *c)
{
    char escaped_path[EDITOR_PATH_MAX * 2];
    s_escape_json_str(s_editor.project.path, escaped_path, sizeof(escaped_path));

    char buf[4096];
    snprintf(buf, sizeof(buf),
        "{\"project_loaded\":%s,\"project_path\":\"%s\",\"dirty\":%s,"
        "\"fixture_count\":%d,\"bank_count\":%d}",
        s_editor.project.loaded ? "true" : "false",
        escaped_path,
        s_editor.project.dirty ? "true" : "false",
        s_editor.project.fixture_count,
        s_editor.bank_count);
    mg_json_reply(c, 200, buf);
}

static void s_handle_open(struct mg_connection *c, struct mg_http_message *hm)
{
    char path[EDITOR_PATH_MAX] = {0};
    int n = mg_json_get_str_buf(hm->body, "$.path", path, sizeof(path));
    if (n <= 0)
    {
        mg_json_reply(c, 400, "{\"error\":\"missing path\"}");
        return;
    }
    if (editor_open_project(&s_editor, path) != 0)
    {
        mg_json_reply(c, 500, "{\"error\":\"failed to open project\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_close(struct mg_connection *c)
{
    editor_close_project(&s_editor);
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_save(struct mg_connection *c)
{
    if (editor_save_project(&s_editor) != 0)
    {
        mg_json_reply(c, 500, "{\"error\":\"save failed\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_save_as(struct mg_connection *c, struct mg_http_message *hm)
{
    char path[EDITOR_PATH_MAX] = {0};
    int n = mg_json_get_str_buf(hm->body, "$.path", path, sizeof(path));
    if (n <= 0)
    {
        mg_json_reply(c, 400, "{\"error\":\"missing path\"}");
        return;
    }
    if (editor_save_project_as(&s_editor, path) != 0)
    {
        mg_json_reply(c, 500, "{\"error\":\"save-as failed\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

/* ---- Hardware config ---- */

static void s_handle_hardware_get(struct mg_connection *c)
{
    const editor_hw_config_t *hw = &s_editor.project.hw;
    char buf[1024];
    snprintf(buf, sizeof(buf),
        "{\"midi_device\":\"%s\",\"midi_mode\":\"%s\","
        "\"dmx_device\":\"%s\",\"dmx_backend\":\"%s\",\"dmx_refresh_hz\":%d}",
        hw->midi_device, hw->midi_mode,
        hw->dmx_device, hw->dmx_backend, hw->dmx_refresh_hz);
    mg_json_reply(c, 200, buf);
}

static void s_handle_hardware_put(struct mg_connection *c, struct mg_http_message *hm)
{
    editor_hw_config_t hw = {0};
    mg_json_get_str_buf(hm->body, "$.midi_device", hw.midi_device, sizeof(hw.midi_device));
    mg_json_get_str_buf(hm->body, "$.midi_mode", hw.midi_mode, sizeof(hw.midi_mode));
    mg_json_get_str_buf(hm->body, "$.dmx_device", hw.dmx_device, sizeof(hw.dmx_device));
    mg_json_get_str_buf(hm->body, "$.dmx_backend", hw.dmx_backend, sizeof(hw.dmx_backend));
    double hz;
    if (mg_json_get_num(hm->body, "$.dmx_refresh_hz", &hz))
        hw.dmx_refresh_hz = (uint8_t)hz;

    editor_hw_update(&s_editor, &hw);
    mg_json_reply(c, 200, "{\"ok\":true}");
}

/* ---- Scenes ---- */

static void s_emit_scene_json(char *buf, size_t cap, size_t *pos, const editor_scene_t *sc, int idx)
{
    *pos += (size_t)snprintf(buf + *pos, cap - *pos,
        "{\"index\":%d,\"id\":\"%s\",\"name\":\"%s\",\"type\":\"%s\","
        "\"trigger_mode\":\"%s\",\"channel\":%d,\"note\":%d,"
        "\"enabled\":%s,\"loop\":%s,\"values\":[",
        idx, sc->id, sc->name, sc->type,
        sc->trigger_mode, sc->channel, sc->note,
        sc->enabled ? "true" : "false",
        sc->loop ? "true" : "false");

    for (uint8_t v = 0; v < sc->value_count; v++)
    {
        if (v > 0) buf[(*pos)++] = ',';
        *pos += (size_t)snprintf(buf + *pos, cap - *pos,
            "{\"target\":\"%s\",\"value\":%d}",
            sc->values[v].target, sc->values[v].value);
    }

    *pos += (size_t)snprintf(buf + *pos, cap - *pos, "],\"steps\":[");

    for (uint8_t s = 0; s < sc->step_count; s++)
    {
        const editor_scene_step_t *step = &sc->steps[s];
        if (s > 0) buf[(*pos)++] = ',';
        *pos += (size_t)snprintf(buf + *pos, cap - *pos,
            "{\"duration_ms\":%u,\"transition\":\"%s\",\"values\":[",
            step->duration_ms, step->transition);
        for (uint8_t v = 0; v < step->value_count; v++)
        {
            if (v > 0) buf[(*pos)++] = ',';
            *pos += (size_t)snprintf(buf + *pos, cap - *pos,
                "{\"target\":\"%s\",\"value\":%d}",
                step->values[v].target, step->values[v].value);
        }
        *pos += (size_t)snprintf(buf + *pos, cap - *pos, "]}");
    }

    *pos += (size_t)snprintf(buf + *pos, cap - *pos, "]}");
}

static void s_handle_scenes_get(struct mg_connection *c)
{
    size_t cap = 64 * 1024;
    char *buf = (char *)malloc(cap);
    if (!buf) { mg_json_reply(c, 500, "{\"error\":\"alloc\"}"); return; }
    size_t pos = 0;
    buf[pos++] = '[';

    for (uint16_t i = 0; i < s_editor.project.scene_count; i++)
    {
        if (i > 0) buf[pos++] = ',';
        s_emit_scene_json(buf, cap, &pos, &s_editor.project.scenes[i], i);
    }
    buf[pos++] = ']';
    buf[pos] = '\0';

    mg_http_reply(c, 200,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%s", buf);
    free(buf);
}

static int s_parse_scene_from_json(struct mg_str body, editor_scene_t *scene)
{
    memset(scene, 0, sizeof(*scene));
    scene->enabled = true;
    strncpy(scene->type, "static", sizeof(scene->type) - 1);
    strncpy(scene->trigger_mode, "gate", sizeof(scene->trigger_mode) - 1);

    mg_json_get_str_buf(body, "$.id", scene->id, sizeof(scene->id));
    mg_json_get_str_buf(body, "$.name", scene->name, sizeof(scene->name));
    mg_json_get_str_buf(body, "$.type", scene->type, sizeof(scene->type));
    mg_json_get_str_buf(body, "$.trigger_mode", scene->trigger_mode, sizeof(scene->trigger_mode));

    double val;
    if (mg_json_get_num(body, "$.channel", &val))
        scene->channel = (uint8_t)val;
    if (mg_json_get_num(body, "$.note", &val))
        scene->note = (uint8_t)val;

    bool bval;
    if (mg_json_get_bool(body, "$.enabled", &bval))
        scene->enabled = bval;
    if (mg_json_get_bool(body, "$.loop", &bval))
        scene->loop = bval;

    /* Parse values array */
    char path[64];
    for (int v = 0; v < EDITOR_MAX_SCENE_VALUES; v++)
    {
        snprintf(path, sizeof(path), "$.values[%d].target", v);
        char target[128] = {0};
        if (mg_json_get_str_buf(body, path, target, sizeof(target)) <= 0) break;
        strncpy(scene->values[v].target, target, sizeof(scene->values[v].target) - 1);
        snprintf(path, sizeof(path), "$.values[%d].value", v);
        double vv;
        if (mg_json_get_num(body, path, &vv))
            scene->values[v].value = (uint8_t)vv;
        scene->value_count++;
    }

    /* Parse steps array */
    for (int s = 0; s < EDITOR_MAX_SCENE_STEPS; s++)
    {
        snprintf(path, sizeof(path), "$.steps[%d].duration_ms", s);
        double dur;
        if (!mg_json_get_num(body, path, &dur)) break;

        editor_scene_step_t *step = &scene->steps[s];
        step->duration_ms = (uint32_t)dur;
        snprintf(path, sizeof(path), "$.steps[%d].transition", s);
        mg_json_get_str_buf(body, path, step->transition, sizeof(step->transition));
        if (step->transition[0] == '\0')
            strncpy(step->transition, "hold", sizeof(step->transition) - 1);

        for (int v = 0; v < EDITOR_MAX_SCENE_VALUES; v++)
        {
            char vpath[96];
            snprintf(vpath, sizeof(vpath), "$.steps[%d].values[%d].target", s, v);
            char t[128] = {0};
            if (mg_json_get_str_buf(body, vpath, t, sizeof(t)) <= 0) break;
            strncpy(step->values[v].target, t, sizeof(step->values[v].target) - 1);
            snprintf(vpath, sizeof(vpath), "$.steps[%d].values[%d].value", s, v);
            double sv;
            if (mg_json_get_num(body, vpath, &sv))
                step->values[v].value = (uint8_t)sv;
            step->value_count++;
        }
        scene->step_count++;
    }

    return (scene->id[0] != '\0') ? 0 : -1;
}

static void s_handle_scene_add(struct mg_connection *c, struct mg_http_message *hm)
{
    editor_scene_t scene;
    if (s_parse_scene_from_json(hm->body, &scene) != 0)
    {
        mg_json_reply(c, 400, "{\"error\":\"invalid scene\"}");
        return;
    }
    if (editor_scene_add(&s_editor, &scene) != 0)
    {
        mg_json_reply(c, 500, "{\"error\":\"too many scenes\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_scene_update(struct mg_connection *c,
                                  struct mg_http_message *hm, int index)
{
    editor_scene_t scene;
    if (s_parse_scene_from_json(hm->body, &scene) != 0)
    {
        mg_json_reply(c, 400, "{\"error\":\"invalid scene\"}");
        return;
    }
    if (editor_scene_update(&s_editor, index, &scene) != 0)
    {
        mg_json_reply(c, 404, "{\"error\":\"scene not found\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_scene_delete(struct mg_connection *c, int index)
{
    if (editor_scene_remove(&s_editor, index) != 0)
    {
        mg_json_reply(c, 404, "{\"error\":\"scene not found\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_fixtures_sort(struct mg_connection *c)
{
    editor_fixtures_sort(&s_editor);
    mg_json_reply(c, 200, "{\"ok\":true}");
}

/* ---- Project fixtures ---- */

static void s_handle_fixtures_get(struct mg_connection *c)
{
    char *out = (char *)malloc(64 * 1024);
    int pos = 0;

    pos += snprintf(out + pos, 64 * 1024 - pos, "[");
    for (int i = 0; i < s_editor.project.fixture_count; i++)
    {
        const editor_fixture_t *f = &s_editor.project.fixtures[i];
        if (i > 0) pos += snprintf(out + pos, 64 * 1024 - pos, ",");
        pos += snprintf(out + pos, 64 * 1024 - pos,
            "{\"index\":%d,\"id\":\"%s\",\"name\":\"%s\","
            "\"start_address\":%d,\"channel_count\":%d,"
            "\"template\":\"%s\",\"copy_from\":\"%s\",\"channels\":[",
            i, f->id, f->name, f->start_address, f->channel_count,
            f->template_ref, f->copy_from);
        for (int j = 0; j < f->channel_count; j++)
        {
            if (j > 0) pos += snprintf(out + pos, 64 * 1024 - pos, ",");
            pos += snprintf(out + pos, 64 * 1024 - pos,
                "{\"name\":\"%s\",\"offset\":%d}",
                f->channels[j].name, f->channels[j].offset);
        }
        pos += snprintf(out + pos, 64 * 1024 - pos, "]}");
    }
    pos += snprintf(out + pos, 64 * 1024 - pos, "]");

    mg_http_reply(c, 200,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%.*s", pos, out);
    free(out);
}

static int s_parse_fixture_json(struct mg_str body, editor_fixture_t *fix)
{
    memset(fix, 0, sizeof(*fix));
    mg_json_get_str_buf(body, "$.id", fix->id, sizeof(fix->id));
    mg_json_get_str_buf(body, "$.name", fix->name, sizeof(fix->name));
    mg_json_get_str_buf(body, "$.template", fix->template_ref, sizeof(fix->template_ref));
    mg_json_get_str_buf(body, "$.copy_from", fix->copy_from, sizeof(fix->copy_from));

    double val;
    if (mg_json_get_num(body, "$.start_address", &val))
        fix->start_address = (uint16_t)val;
    if (mg_json_get_num(body, "$.channel_count", &val))
        fix->channel_count = (uint8_t)val;

    for (int i = 0; i < EDITOR_MAX_CHANNELS; i++)
    {
        char pname[64], poffset[64];
        snprintf(pname, sizeof(pname), "$.channels[%d].name", i);
        snprintf(poffset, sizeof(poffset), "$.channels[%d].offset", i);

        int n = mg_json_get_str_buf(body, pname, fix->channels[i].name, SPARK_MAX_NAME_SIZE);
        if (n <= 0) break;

        double ov;
        if (mg_json_get_num(body, poffset, &ov))
            fix->channels[i].offset = (uint8_t)ov;

        if (i + 1 > fix->channel_count)
            fix->channel_count = (uint8_t)(i + 1);
    }
    return (fix->id[0] != '\0') ? 0 : -1;
}

static void s_handle_fixture_add(struct mg_connection *c, struct mg_http_message *hm)
{
    editor_fixture_t fix;
    if (s_parse_fixture_json(hm->body, &fix) != 0)
    {
        mg_json_reply(c, 400, "{\"error\":\"invalid fixture data\"}");
        return;
    }
    if (editor_fixture_add(&s_editor, &fix) != 0)
    {
        mg_json_reply(c, 500, "{\"error\":\"add failed\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_fixture_update(struct mg_connection *c,
                                    struct mg_http_message *hm, int index)
{
    editor_fixture_t fix;
    if (s_parse_fixture_json(hm->body, &fix) != 0)
    {
        mg_json_reply(c, 400, "{\"error\":\"invalid fixture data\"}");
        return;
    }
    if (editor_fixture_update(&s_editor, index, &fix) != 0)
    {
        mg_json_reply(c, 404, "{\"error\":\"fixture not found\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_fixture_delete(struct mg_connection *c, int index)
{
    if (editor_fixture_remove(&s_editor, index) != 0)
    {
        mg_json_reply(c, 404, "{\"error\":\"fixture not found\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

/* ---- Banks ---- */

static void s_handle_banks_get(struct mg_connection *c)
{
    char *out = (char *)malloc(128 * 1024);
    int pos = 0;
    pos += snprintf(out + pos, 128 * 1024 - pos, "[");

    for (int bi = 0; bi < s_editor.bank_count; bi++)
    {
        const editor_bank_t *bank = &s_editor.banks[bi];
        char esc_path[EDITOR_PATH_MAX * 2];
        s_escape_json_str(bank->path, esc_path, sizeof(esc_path));
        if (bi > 0) pos += snprintf(out + pos, 128 * 1024 - pos, ",");
        pos += snprintf(out + pos, 128 * 1024 - pos,
            "{\"index\":%d,\"id\":\"%s\",\"path\":\"%s\","
            "\"version\":%d,\"dirty\":%s,\"fixtures\":[",
            bi, bank->id, esc_path, bank->version,
            bank->dirty ? "true" : "false");

        for (int fi = 0; fi < bank->fixture_count; fi++)
        {
            const editor_bank_fixture_t *f = &bank->fixtures[fi];
            if (fi > 0) pos += snprintf(out + pos, 128 * 1024 - pos, ",");
            pos += snprintf(out + pos, 128 * 1024 - pos,
                "{\"index\":%d,\"id\":\"%s\",\"name\":\"%s\","
                "\"channel_count\":%d,\"channels\":[",
                fi, f->id, f->name, f->channel_count);
            for (int j = 0; j < f->channel_count; j++)
            {
                if (j > 0) pos += snprintf(out + pos, 128 * 1024 - pos, ",");
                pos += snprintf(out + pos, 128 * 1024 - pos,
                    "{\"name\":\"%s\",\"offset\":%d}",
                    f->channels[j].name, f->channels[j].offset);
            }
            pos += snprintf(out + pos, 128 * 1024 - pos, "]}");
        }
        pos += snprintf(out + pos, 128 * 1024 - pos, "]}");
    }
    pos += snprintf(out + pos, 128 * 1024 - pos, "]");

    mg_http_reply(c, 200,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%.*s", pos, out);
    free(out);
}

static int s_parse_bank_fixture_json(struct mg_str body, editor_bank_fixture_t *fix)
{
    memset(fix, 0, sizeof(*fix));
    mg_json_get_str_buf(body, "$.id", fix->id, sizeof(fix->id));
    mg_json_get_str_buf(body, "$.name", fix->name, sizeof(fix->name));

    double val;
    if (mg_json_get_num(body, "$.channel_count", &val))
        fix->channel_count = (uint8_t)val;

    for (int i = 0; i < EDITOR_MAX_CHANNELS; i++)
    {
        char pname[64], poffset[64];
        snprintf(pname, sizeof(pname), "$.channels[%d].name", i);
        snprintf(poffset, sizeof(poffset), "$.channels[%d].offset", i);

        int n = mg_json_get_str_buf(body, pname, fix->channels[i].name, SPARK_MAX_NAME_SIZE);
        if (n <= 0) break;

        double ov;
        if (mg_json_get_num(body, poffset, &ov))
            fix->channels[i].offset = (uint8_t)ov;

        if (i + 1 > fix->channel_count)
            fix->channel_count = (uint8_t)(i + 1);
    }
    return (fix->id[0] != '\0') ? 0 : -1;
}

static void s_handle_bank_fixture_add(struct mg_connection *c,
                                      struct mg_http_message *hm, int bank_idx)
{
    editor_bank_fixture_t fix;
    if (s_parse_bank_fixture_json(hm->body, &fix) != 0)
    {
        mg_json_reply(c, 400, "{\"error\":\"invalid fixture data\"}");
        return;
    }
    if (editor_bank_fixture_add(&s_editor, bank_idx, &fix) != 0)
    {
        mg_json_reply(c, 500, "{\"error\":\"add failed\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_bank_fixture_update(struct mg_connection *c,
                                         struct mg_http_message *hm,
                                         int bank_idx, int fix_idx)
{
    editor_bank_fixture_t fix;
    if (s_parse_bank_fixture_json(hm->body, &fix) != 0)
    {
        mg_json_reply(c, 400, "{\"error\":\"invalid fixture data\"}");
        return;
    }
    if (editor_bank_fixture_update(&s_editor, bank_idx, fix_idx, &fix) != 0)
    {
        mg_json_reply(c, 404, "{\"error\":\"not found\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_bank_fixture_delete(struct mg_connection *c,
                                         int bank_idx, int fix_idx)
{
    if (editor_bank_fixture_remove(&s_editor, bank_idx, fix_idx) != 0)
    {
        mg_json_reply(c, 404, "{\"error\":\"not found\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_bank_save(struct mg_connection *c, int bank_idx)
{
    if (editor_save_bank(&s_editor, bank_idx) != 0)
    {
        mg_json_reply(c, 500, "{\"error\":\"save failed\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

/* ---- Bank directories ---- */

static void s_handle_bank_dirs(struct mg_connection *c)
{
    const char *paths = spark_env_get("SPARK_FIXTURE_BANK_PATH");
    char *out = (char *)malloc(8192);
    int pos = 0;
    pos += snprintf(out + pos, 8192 - pos, "[");

    if (paths && paths[0] != '\0')
    {
        char buf[4096];
        strncpy(buf, paths, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';

        char *saveptr = NULL;
        char *token = strtok_r(buf, ";", &saveptr);
        int first = 1;
        while (token)
        {
            while (*token == ' ') token++;
            size_t tlen = strlen(token);
            while (tlen > 0 && token[tlen - 1] == ' ') token[--tlen] = '\0';
            if (tlen > 0)
            {
                char esc[EDITOR_PATH_MAX * 2];
                s_escape_json_str(token, esc, sizeof(esc));
                if (!first) pos += snprintf(out + pos, 8192 - pos, ",");
                pos += snprintf(out + pos, 8192 - pos, "\"%s\"", esc);
                first = 0;
            }
            token = strtok_r(NULL, ";", &saveptr);
        }
    }

    pos += snprintf(out + pos, 8192 - pos, "]");
    mg_http_reply(c, 200,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%.*s", pos, out);
    free(out);
}

static void s_handle_bank_create(struct mg_connection *c, struct mg_http_message *hm)
{
    char id[SPARK_MAX_ID_SIZE] = {0};
    char directory[EDITOR_PATH_MAX] = {0};

    mg_json_get_str_buf(hm->body, "$.id", id, sizeof(id));
    mg_json_get_str_buf(hm->body, "$.directory", directory, sizeof(directory));

    if (id[0] == '\0' || directory[0] == '\0')
    {
        mg_json_reply(c, 400, "{\"error\":\"missing id or directory\"}");
        return;
    }

    char filepath[EDITOR_PATH_MAX + 128];
    snprintf(filepath, sizeof(filepath), "%s/%s.yaml", directory, id);

    FILE *f = fopen(filepath, "w");
    if (!f)
    {
        mg_json_reply(c, 500, "{\"error\":\"cannot create file\"}");
        return;
    }
    fprintf(f, "bank:\n  id: \"%s\"\n  version: 1\n\nfixtures: []\n", id);
    fclose(f);

    const char *paths = spark_env_get("SPARK_FIXTURE_BANK_PATH");
    editor_load_banks(&s_editor, paths);

    mg_json_reply(c, 200, "{\"ok\":true}");
}

/* ---- File browser ---- */

static void s_handle_browse_roots(struct mg_connection *c)
{
    editor_roots_t roots;
    editor_get_roots(&roots);

    char *out = (char *)malloc(16 * 1024);
    int pos = 0;
    pos += snprintf(out + pos, 16 * 1024 - pos, "{\"places\":[");
    for (uint8_t i = 0; i < roots.place_count; i++)
    {
        char esc_label[EDITOR_PLACE_LABEL_MAX * 2];
        char esc_path[EDITOR_PLACE_PATH_MAX * 2];
        s_escape_json_str(roots.places[i].label, esc_label, sizeof(esc_label));
        s_escape_json_str(roots.places[i].path, esc_path, sizeof(esc_path));
        if (i > 0) pos += snprintf(out + pos, 16 * 1024 - pos, ",");
        pos += snprintf(out + pos, 16 * 1024 - pos,
            "{\"label\":\"%s\",\"path\":\"%s\"}", esc_label, esc_path);
    }
    pos += snprintf(out + pos, 16 * 1024 - pos, "],\"drives\":[");
    for (uint8_t i = 0; i < roots.drive_count; i++)
    {
        char esc_label[EDITOR_PLACE_LABEL_MAX * 2];
        char esc_path[EDITOR_PLACE_PATH_MAX * 2];
        s_escape_json_str(roots.drives[i].label, esc_label, sizeof(esc_label));
        s_escape_json_str(roots.drives[i].path, esc_path, sizeof(esc_path));
        if (i > 0) pos += snprintf(out + pos, 16 * 1024 - pos, ",");
        pos += snprintf(out + pos, 16 * 1024 - pos,
            "{\"label\":\"%s\",\"path\":\"%s\"}", esc_label, esc_path);
    }
    pos += snprintf(out + pos, 16 * 1024 - pos, "]}");

    mg_http_reply(c, 200,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%.*s", pos, out);
    free(out);
}

static int s_is_hidden(const char *name)
{
    return name[0] == '.';
}

static int s_is_browsable(const char *name)
{
    size_t len = strlen(name);
    if (len >= 5 && strcmp(name + len - 5, ".yaml") == 0) return 1;
    if (len >= 4 && strcmp(name + len - 4, ".yml") == 0) return 1;
    return 0;
}

#ifdef _WIN32

static void s_handle_browse(struct mg_connection *c, struct mg_http_message *hm)
{
    char path[EDITOR_PATH_MAX] = {0};
    mg_http_get_var(&hm->query, "path", path, sizeof(path));

    if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0'))
    {
        _getcwd(path, sizeof(path));
    }
    else
    {
        char abs[EDITOR_PATH_MAX];
        if (_fullpath(abs, path, sizeof(abs)) != NULL)
            strncpy(path, abs, sizeof(path) - 1);
    }

    size_t plen = strlen(path);
    while (plen > 1 && (path[plen - 1] == '/' || path[plen - 1] == '\\'))
        path[--plen] = '\0';

    char pattern[EDITOR_PATH_MAX + 4];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);

    spark_log_debug("browse: path='%s' pattern='%s'", path, pattern);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        spark_log_debug("browse: FindFirstFileA failed for '%s'", pattern);
        mg_json_reply(c, 404, "{\"error\":\"cannot open directory\"}");
        return;
    }

    char escaped_path[EDITOR_PATH_MAX * 2];
    s_escape_json_str(path, escaped_path, sizeof(escaped_path));

    char *out = (char *)malloc(64 * 1024);
    int pos = 0;
    pos += snprintf(out + pos, 64 * 1024 - pos,
        "{\"path\":\"%s\",\"entries\":[", escaped_path);

    int first = 1;
    do {
        if (s_is_hidden(fd.cFileName)) continue;
        int is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (!is_dir && !s_is_browsable(fd.cFileName)) continue;

        if (!first) pos += snprintf(out + pos, 64 * 1024 - pos, ",");
        pos += snprintf(out + pos, 64 * 1024 - pos,
            "{\"name\":\"%s\",\"type\":\"%s\"}",
            fd.cFileName, is_dir ? "dir" : "file");
        first = 0;
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    pos += snprintf(out + pos, 64 * 1024 - pos, "]}");

    mg_http_reply(c, 200,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%.*s", pos, out);
    free(out);
}

#else

static void s_handle_browse(struct mg_connection *c, struct mg_http_message *hm)
{
    char path[EDITOR_PATH_MAX] = {0};
    mg_http_get_var(&hm->query, "path", path, sizeof(path));

    if (path[0] == '\0')
    {
        getcwd(path, sizeof(path));
    }
    else if (path[0] != '/')
    {
        char abs[EDITOR_PATH_MAX];
        if (realpath(path, abs) != NULL)
            strncpy(path, abs, sizeof(path) - 1);
    }

    DIR *dir = opendir(path);
    if (!dir)
    {
        mg_json_reply(c, 404, "{\"error\":\"cannot open directory\"}");
        return;
    }

    char *out = (char *)malloc(64 * 1024);
    int pos = 0;
    pos += snprintf(out + pos, 64 * 1024 - pos,
        "{\"path\":\"%s\",\"entries\":[", path);

    struct dirent *entry;
    int first = 1;
    while ((entry = readdir(dir)) != NULL)
    {
        if (s_is_hidden(entry->d_name)) continue;

        int is_dir = 0;
        if (entry->d_type == DT_DIR)
        {
            is_dir = 1;
        }
        else if (entry->d_type == DT_UNKNOWN)
        {
            char fullpath[EDITOR_PATH_MAX * 2];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
            struct stat st;
            if (stat(fullpath, &st) == 0 && S_ISDIR(st.st_mode))
                is_dir = 1;
        }

        if (!is_dir && !s_is_browsable(entry->d_name)) continue;

        if (!first) pos += snprintf(out + pos, 64 * 1024 - pos, ",");
        pos += snprintf(out + pos, 64 * 1024 - pos,
            "{\"name\":\"%s\",\"type\":\"%s\"}",
            entry->d_name, is_dir ? "dir" : "file");
        first = 0;
    }

    closedir(dir);
    pos += snprintf(out + pos, 64 * 1024 - pos, "]}");

    mg_http_reply(c, 200,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n",
        "%.*s", pos, out);
    free(out);
}

#endif

/* ---- Route dispatcher ---- */

bool editor_http_handle(struct mg_connection *c, struct mg_http_message *hm)
{
    if (mg_match(hm->uri, mg_str("/api/editor/status"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_status(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/open"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_open(c, hm);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/close"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_close(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/save"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_save(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/save-as"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_save_as(c, hm);
        return true;
    }

    /* Hardware config */
    if (mg_match(hm->uri, mg_str("/api/editor/hardware"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_hardware_get(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/hardware"), NULL) &&
        mg_method_is(hm, "PUT"))
    {
        s_handle_hardware_put(c, hm);
        return true;
    }

    /* Scenes */
    if (mg_match(hm->uri, mg_str("/api/editor/scenes"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_scenes_get(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/scenes"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_scene_add(c, hm);
        return true;
    }

    /* /api/editor/scenes/:index */
    if (mg_match(hm->uri, mg_str("/api/editor/scenes/#"), NULL))
    {
        struct mg_str tail = hm->uri;
        tail.buf += 20; /* skip "/api/editor/scenes/" */
        tail.len -= 20;
        int idx = (int)strtoul(tail.buf, NULL, 10);

        if (mg_method_is(hm, "PUT"))
        {
            s_handle_scene_update(c, hm, idx);
            return true;
        }
        if (mg_method_is(hm, "DELETE"))
        {
            s_handle_scene_delete(c, idx);
            return true;
        }
    }

    /* Bank dirs + create */
    if (mg_match(hm->uri, mg_str("/api/editor/bank-dirs"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_bank_dirs(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/banks/create"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_bank_create(c, hm);
        return true;
    }

    /* File browser */
    if (mg_match(hm->uri, mg_str("/api/editor/browse/roots"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_browse_roots(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/browse"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_browse(c, hm);
        return true;
    }

    /* Project fixtures */
    if (mg_match(hm->uri, mg_str("/api/editor/fixtures/sort"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_fixtures_sort(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/fixtures"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_fixtures_get(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/fixtures"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_fixture_add(c, hm);
        return true;
    }

    /* /api/editor/fixtures/:index */
    if (mg_match(hm->uri, mg_str("/api/editor/fixtures/#"), NULL))
    {
        struct mg_str tail = hm->uri;
        tail.buf += 21; /* skip "/api/editor/fixtures/" */
        tail.len -= 21;
        int idx = (int)strtoul(tail.buf, NULL, 10);

        if (mg_method_is(hm, "PUT"))
        {
            s_handle_fixture_update(c, hm, idx);
            return true;
        }
        if (mg_method_is(hm, "DELETE"))
        {
            s_handle_fixture_delete(c, idx);
            return true;
        }
    }

    /* Banks list */
    if (mg_match(hm->uri, mg_str("/api/editor/banks"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_banks_get(c);
        return true;
    }

    /* /api/editor/banks/:bank_idx/save */
    if (mg_match(hm->uri, mg_str("/api/editor/banks/#/save"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        struct mg_str tail = hm->uri;
        tail.buf += 18; /* "/api/editor/banks/" */
        tail.len -= 18;
        int bank_idx = (int)strtoul(tail.buf, NULL, 10);
        s_handle_bank_save(c, bank_idx);
        return true;
    }

    /* /api/editor/banks/:bank_idx/fixtures */
    if (mg_match(hm->uri, mg_str("/api/editor/banks/#/fixtures"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        struct mg_str tail = hm->uri;
        tail.buf += 18;
        tail.len -= 18;
        int bank_idx = (int)strtoul(tail.buf, NULL, 10);
        s_handle_bank_fixture_add(c, hm, bank_idx);
        return true;
    }

    /* /api/editor/banks/:bank_idx/fixtures/:fix_idx */
    if (mg_match(hm->uri, mg_str("/api/editor/banks/#/fixtures/#"), NULL))
    {
        struct mg_str tail = hm->uri;
        tail.buf += 18;
        tail.len -= 18;
        char *end;
        int bank_idx = (int)strtoul(tail.buf, &end, 10);
        const char *fix_part = end + 10; /* skip "/fixtures/" */
        int fix_idx = (int)strtoul(fix_part, NULL, 10);

        if (mg_method_is(hm, "PUT"))
        {
            s_handle_bank_fixture_update(c, hm, bank_idx, fix_idx);
            return true;
        }
        if (mg_method_is(hm, "DELETE"))
        {
            s_handle_bank_fixture_delete(c, bank_idx, fix_idx);
            return true;
        }
    }

    return false;
}
