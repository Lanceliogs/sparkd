#include "mongoose.h"
#include "mg_helpers.h"
#include "editor.h"
#include "env.h"
#include "log.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <dirent.h>
#endif

#define DEFAULT_HTTP_ADDR "127.0.0.1:7601"
#define DEFAULT_DAEMON_ADDR "127.0.0.1:7600"
#define DEFAULT_UI_ROOT "ui/dist"

static volatile bool s_running = true;
static char s_daemon_addr[256] = DEFAULT_DAEMON_ADDR;
static char s_ui_root[1024] = DEFAULT_UI_ROOT;

static void s_signal_handler(int sig)
{
    (void)sig;
    s_running = false;
}

static struct mg_http_serve_opts s_serve_opts;

static void s_serve_static(struct mg_connection *c, struct mg_http_message *hm)
{
    s_serve_opts.root_dir = s_ui_root;
    s_serve_opts.ssi_pattern = NULL;
    s_serve_opts.extra_headers = "Cache-Control: no-cache\r\n";
    mg_http_serve_dir(c, hm, &s_serve_opts);
}

typedef struct {
    unsigned long client_id;
    struct mg_mgr *mgr;
} proxy_ctx_t;

static struct mg_connection *s_find_connection(struct mg_mgr *mgr, unsigned long id)
{
    for (struct mg_connection *c = mgr->conns; c; c = c->next)
        if (c->id == id) return c;
    return NULL;
}

static void s_proxy_ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    proxy_ctx_t *ctx = (proxy_ctx_t *)c->fn_data;
    if (!ctx) return;

    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        struct mg_connection *client = s_find_connection(ctx->mgr, ctx->client_id);
        if (client)
        {
            mg_http_reply(client, mg_http_status(hm),
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Type: application/json\r\n",
                "%.*s", (int)hm->body.len, hm->body.buf);
        }
        c->is_closing = 1;
        free(ctx);
        c->fn_data = NULL;
    }
    else if (ev == MG_EV_ERROR)
    {
        struct mg_connection *client = s_find_connection(ctx->mgr, ctx->client_id);
        if (client)
            mg_http_reply(client, 502, "", "{\"error\":\"daemon unreachable\"}\n");
        c->is_closing = 1;
        free(ctx);
        c->fn_data = NULL;
    }
    else if (ev == MG_EV_CLOSE)
    {
        if (c->fn_data) free(c->fn_data);
        c->fn_data = NULL;
    }
}

static void s_proxy_request(struct mg_mgr *mgr, struct mg_connection *client,
                            struct mg_http_message *hm)
{
    char url[512];
    snprintf(url, sizeof(url), "%s%.*s", s_daemon_addr, (int)hm->uri.len, hm->uri.buf);

    proxy_ctx_t *ctx = calloc(1, sizeof(proxy_ctx_t));
    ctx->client_id = client->id;
    ctx->mgr = mgr;

    struct mg_connection *pc = mg_http_connect(mgr, url, s_proxy_ev_handler, ctx);
    if (!pc)
    {
        mg_http_reply(client, 502, "", "{\"error\":\"cannot connect to daemon\"}\n");
        free(ctx);
        return;
    }

    struct mg_str host = mg_url_host(url);
    if (hm->body.len > 0)
    {
        mg_printf(pc,
            "%.*s %.*s HTTP/1.0\r\n"
            "Host: %.*s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %lu\r\n"
            "Connection: close\r\n"
            "\r\n",
            (int)hm->method.len, hm->method.buf,
            (int)hm->uri.len, hm->uri.buf,
            (int)host.len, host.buf,
            (unsigned long)hm->body.len);
        mg_send(pc, hm->body.buf, hm->body.len);
    }
    else
    {
        mg_printf(pc,
            "%.*s %.*s HTTP/1.0\r\n"
            "Host: %.*s\r\n"
            "Connection: close\r\n"
            "\r\n",
            (int)hm->method.len, hm->method.buf,
            (int)hm->uri.len, hm->uri.buf,
            (int)host.len, host.buf);
    }
}

/* WebSocket proxy: client <-> sparkd relay */
typedef struct {
    unsigned long peer_id;
    struct mg_mgr *mgr;
} ws_proxy_ctx_t;

static void s_ws_proxy_ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    ws_proxy_ctx_t *ctx = (ws_proxy_ctx_t *)c->fn_data;
    if (!ctx) return;

    if (ev == MG_EV_WS_OPEN)
    {
        /* Backend WS connected; nothing to do */
    }
    else if (ev == MG_EV_WS_MSG)
    {
        struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
        struct mg_connection *peer = s_find_connection(ctx->mgr, ctx->peer_id);
        if (peer)
            mg_ws_send(peer, wm->data.buf, wm->data.len, WEBSOCKET_OP_TEXT);
    }
    else if (ev == MG_EV_CLOSE)
    {
        struct mg_connection *peer = s_find_connection(ctx->mgr, ctx->peer_id);
        if (peer && peer->is_websocket)
            peer->is_closing = 1;
        free(ctx);
        c->fn_data = NULL;
    }
}

static void s_ws_proxy_connect(struct mg_mgr *mgr, struct mg_connection *client)
{
    char url[512];
    snprintf(url, sizeof(url), "ws%s/ws", s_daemon_addr + 4); /* http -> ws */

    ws_proxy_ctx_t *be_ctx = calloc(1, sizeof(ws_proxy_ctx_t));
    be_ctx->peer_id = client->id;
    be_ctx->mgr = mgr;

    struct mg_connection *be = mg_ws_connect(mgr, url, s_ws_proxy_ev_handler, be_ctx, NULL);
    if (!be)
    {
        free(be_ctx);
        client->is_closing = 1;
        return;
    }

    ws_proxy_ctx_t *cl_ctx = calloc(1, sizeof(ws_proxy_ctx_t));
    cl_ctx->peer_id = be->id;
    cl_ctx->mgr = mgr;
    client->fn_data = cl_ctx;
}

/* ---- Editor state and API handlers ---- */

static editor_state_t s_editor;

static void s_handle_editor_status(struct mg_connection *c)
{
    char buf[2048];
    snprintf(buf, sizeof(buf),
        "{\"project_loaded\":%s,\"project_path\":\"%s\",\"dirty\":%s,"
        "\"fixture_count\":%d,\"bank_count\":%d}",
        s_editor.project.loaded ? "true" : "false",
        s_editor.project.path,
        s_editor.project.dirty ? "true" : "false",
        s_editor.project.fixture_count,
        s_editor.bank_count);
    mg_json_reply(c, 200, buf);
}

static void s_handle_editor_open(struct mg_connection *c, struct mg_http_message *hm)
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

static void s_handle_editor_close(struct mg_connection *c)
{
    editor_close_project(&s_editor);
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_editor_save(struct mg_connection *c)
{
    if (editor_save_project(&s_editor) != 0)
    {
        mg_json_reply(c, 500, "{\"error\":\"save failed\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_editor_fixtures_get(struct mg_connection *c)
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

static void s_handle_editor_fixture_add(struct mg_connection *c, struct mg_http_message *hm)
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

static void s_handle_editor_fixture_update(struct mg_connection *c,
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

static void s_handle_editor_fixture_delete(struct mg_connection *c, int index)
{
    if (editor_fixture_remove(&s_editor, index) != 0)
    {
        mg_json_reply(c, 404, "{\"error\":\"fixture not found\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

/* Bank endpoints */

static void s_handle_editor_banks_get(struct mg_connection *c)
{
    char *out = (char *)malloc(128 * 1024);
    int pos = 0;
    pos += snprintf(out + pos, 128 * 1024 - pos, "[");

    for (int bi = 0; bi < s_editor.bank_count; bi++)
    {
        const editor_bank_t *bank = &s_editor.banks[bi];
        if (bi > 0) pos += snprintf(out + pos, 128 * 1024 - pos, ",");
        pos += snprintf(out + pos, 128 * 1024 - pos,
            "{\"index\":%d,\"id\":\"%s\",\"path\":\"%s\","
            "\"version\":%d,\"dirty\":%s,\"fixtures\":[",
            bi, bank->id, bank->path, bank->version,
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

static void s_handle_editor_bank_fixture_add(struct mg_connection *c,
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

static void s_handle_editor_bank_fixture_update(struct mg_connection *c,
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

static void s_handle_editor_bank_fixture_delete(struct mg_connection *c,
                                                int bank_idx, int fix_idx)
{
    if (editor_bank_fixture_remove(&s_editor, bank_idx, fix_idx) != 0)
    {
        mg_json_reply(c, 404, "{\"error\":\"not found\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

static void s_handle_editor_bank_save(struct mg_connection *c, int bank_idx)
{
    if (editor_save_bank(&s_editor, bank_idx) != 0)
    {
        mg_json_reply(c, 500, "{\"error\":\"save failed\"}");
        return;
    }
    mg_json_reply(c, 200, "{\"ok\":true}");
}

/* Bank directories from SPARK_FIXTURE_BANK_PATH */

static void s_handle_editor_bank_dirs(struct mg_connection *c)
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
                if (!first) pos += snprintf(out + pos, 8192 - pos, ",");
                pos += snprintf(out + pos, 8192 - pos, "\"%s\"", token);
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

static void s_handle_editor_bank_create(struct mg_connection *c, struct mg_http_message *hm)
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

    /* Write minimal bank YAML */
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

    /* Reload banks */
    const char *paths = spark_env_get("SPARK_FIXTURE_BANK_PATH");
    editor_load_banks(&s_editor, paths);

    mg_json_reply(c, 200, "{\"ok\":true}");
}

/* File/directory browser */

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

static void s_handle_editor_browse(struct mg_connection *c, struct mg_http_message *hm)
{
    char path[EDITOR_PATH_MAX] = {0};
    struct mg_str qpath = mg_http_var(hm->query, mg_str("path"));
    if (qpath.len > 0 && qpath.len < sizeof(path))
        memcpy(path, qpath.buf, qpath.len);

    if (path[0] == '\0')
    {
        char *cwd = _getcwd(NULL, 0);
        if (cwd) { strncpy(path, cwd, sizeof(path) - 1); free(cwd); }
    }

    char pattern[EDITOR_PATH_MAX + 4];
    snprintf(pattern, sizeof(pattern), "%s/*", path);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
    {
        mg_json_reply(c, 404, "{\"error\":\"cannot open directory\"}");
        return;
    }

    char *out = (char *)malloc(64 * 1024);
    int pos = 0;
    pos += snprintf(out + pos, 64 * 1024 - pos,
        "{\"path\":\"%s\",\"entries\":[", path);

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

static void s_handle_editor_browse(struct mg_connection *c, struct mg_http_message *hm)
{
    char path[EDITOR_PATH_MAX] = {0};
    struct mg_str qpath = mg_http_var(hm->query, mg_str("path"));
    if (qpath.len > 0 && qpath.len < sizeof(path))
        memcpy(path, qpath.buf, qpath.len);

    if (path[0] == '\0')
    {
        char *cwd = getcwd(NULL, 0);
        if (cwd) { strncpy(path, cwd, sizeof(path) - 1); free(cwd); }
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
        int is_dir = (entry->d_type == DT_DIR);
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

static bool s_handle_editor_routes(struct mg_connection *c, struct mg_http_message *hm)
{
    if (mg_match(hm->uri, mg_str("/api/editor/status"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_editor_status(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/open"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_editor_open(c, hm);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/close"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_editor_close(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/save"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_editor_save(c);
        return true;
    }

    /* Bank dirs + create */
    if (mg_match(hm->uri, mg_str("/api/editor/bank-dirs"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_editor_bank_dirs(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/banks/create"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_editor_bank_create(c, hm);
        return true;
    }

    /* File browser */
    if (mg_match(hm->uri, mg_str("/api/editor/browse"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_editor_browse(c, hm);
        return true;
    }

    /* Project fixtures */
    if (mg_match(hm->uri, mg_str("/api/editor/fixtures"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_editor_fixtures_get(c);
        return true;
    }
    if (mg_match(hm->uri, mg_str("/api/editor/fixtures"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        s_handle_editor_fixture_add(c, hm);
        return true;
    }

    /* /api/editor/fixtures/:index */
    if (mg_match(hm->uri, mg_str("/api/editor/fixtures/#"), NULL))
    {
        struct mg_str tail = hm->uri;
        tail.buf += 22; /* skip "/api/editor/fixtures/" */
        tail.len -= 22;
        int idx = (int)strtoul(tail.buf, NULL, 10);

        if (mg_method_is(hm, "PUT"))
        {
            s_handle_editor_fixture_update(c, hm, idx);
            return true;
        }
        if (mg_method_is(hm, "DELETE"))
        {
            s_handle_editor_fixture_delete(c, idx);
            return true;
        }
    }

    /* Banks list */
    if (mg_match(hm->uri, mg_str("/api/editor/banks"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        s_handle_editor_banks_get(c);
        return true;
    }

    /* /api/editor/banks/:bank_idx/save */
    if (mg_match(hm->uri, mg_str("/api/editor/banks/#/save"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        struct mg_str tail = hm->uri;
        tail.buf += 19; /* "/api/editor/banks/" */
        tail.len -= 19;
        int bank_idx = (int)strtoul(tail.buf, NULL, 10);
        s_handle_editor_bank_save(c, bank_idx);
        return true;
    }

    /* /api/editor/banks/:bank_idx/fixtures */
    if (mg_match(hm->uri, mg_str("/api/editor/banks/#/fixtures"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        struct mg_str tail = hm->uri;
        tail.buf += 19;
        tail.len -= 19;
        int bank_idx = (int)strtoul(tail.buf, NULL, 10);
        s_handle_editor_bank_fixture_add(c, hm, bank_idx);
        return true;
    }

    /* /api/editor/banks/:bank_idx/fixtures/:fix_idx */
    if (mg_match(hm->uri, mg_str("/api/editor/banks/#/fixtures/#"), NULL))
    {
        struct mg_str tail = hm->uri;
        tail.buf += 19;
        tail.len -= 19;
        char *end;
        int bank_idx = (int)strtoul(tail.buf, &end, 10);
        const char *fix_part = end + 10; /* skip "/fixtures/" */
        int fix_idx = (int)strtoul(fix_part, NULL, 10);

        if (mg_method_is(hm, "PUT"))
        {
            s_handle_editor_bank_fixture_update(c, hm, bank_idx, fix_idx);
            return true;
        }
        if (mg_method_is(hm, "DELETE"))
        {
            s_handle_editor_bank_fixture_delete(c, bank_idx, fix_idx);
            return true;
        }
    }

    return false;
}

static void s_ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_WS_MSG)
    {
        /* Relay client -> backend */
        ws_proxy_ctx_t *ctx = (ws_proxy_ctx_t *)c->fn_data;
        if (ctx)
        {
            struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
            struct mg_connection *peer = s_find_connection(ctx->mgr, ctx->peer_id);
            if (peer)
                mg_ws_send(peer, wm->data.buf, wm->data.len, WEBSOCKET_OP_TEXT);
        }
        return;
    }

    if (ev == MG_EV_CLOSE && c->is_websocket)
    {
        ws_proxy_ctx_t *ctx = (ws_proxy_ctx_t *)c->fn_data;
        if (ctx)
        {
            struct mg_connection *peer = s_find_connection(ctx->mgr, ctx->peer_id);
            if (peer)
                peer->is_closing = 1;
            free(ctx);
            c->fn_data = NULL;
        }
        return;
    }

    if (ev != MG_EV_HTTP_MSG) return;

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    struct mg_mgr *mgr = (struct mg_mgr *)c->fn_data;

    if (mg_match(hm->uri, mg_str("/ws"), NULL))
    {
        mg_ws_upgrade(c, hm, NULL);
        s_ws_proxy_connect(mgr, c);
        return;
    }

    /* CORS preflight for editor API */
    if (mg_method_is(hm, "OPTIONS") &&
        mg_match(hm->uri, mg_str("/api/editor/#"), NULL))
    {
        mg_cors_preflight(c);
        return;
    }

    if (mg_match(hm->uri, mg_str("/api/editor/#"), NULL))
    {
        if (s_handle_editor_routes(c, hm))
            return;
    }

    if (mg_match(hm->uri, mg_str("/api/#"), NULL))
    {
        s_proxy_request(mgr, c, hm);
        return;
    }

    s_serve_static(c, hm);
}

static void usage(void)
{
    fprintf(stderr, "spark-ui - web UI server for sparkd\n\n");
    fprintf(stderr, "Usage: spark-ui [OPTIONS]\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --http ADDR      Listen address (default: %s)\n", DEFAULT_HTTP_ADDR);
    fprintf(stderr, "  --daemon ADDR    sparkd address (default: %s)\n", DEFAULT_DAEMON_ADDR);
    fprintf(stderr, "  --ui-root PATH   Built UI directory (default: %s)\n", DEFAULT_UI_ROOT);
    fprintf(stderr, "  --open-browser   Open browser on startup\n");
    fprintf(stderr, "  --help           Show this help\n");
}

static void s_open_browser(const char *addr)
{
#ifdef _WIN32
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "start %s", addr);
    system(cmd);
#elif __APPLE__
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "open %s", addr);
    system(cmd);
#else
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "xdg-open %s >/dev/null 2>&1 &", addr);
    system(cmd);
#endif
}

int main(int argc, char **argv)
{
    spark_env_load();

    char http_addr[256];
    snprintf(http_addr, sizeof(http_addr), "%s", DEFAULT_HTTP_ADDR);
    bool open_browser = false;

    const char *env;
    if ((env = spark_env_get("SPARK_UI_HTTP_ADDR")) != NULL)
        snprintf(http_addr, sizeof(http_addr), "%s", env);
    if ((env = spark_env_get("SPARK_UI_DAEMON_ADDR")) != NULL)
        snprintf(s_daemon_addr, sizeof(s_daemon_addr), "%s", env);

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            usage();
            return 0;
        }
        else if (strcmp(argv[i], "--http") == 0 && i + 1 < argc)
            snprintf(http_addr, sizeof(http_addr), "%s", argv[++i]);
        else if (strcmp(argv[i], "--daemon") == 0 && i + 1 < argc)
            snprintf(s_daemon_addr, sizeof(s_daemon_addr), "%s", argv[++i]);
        else if (strcmp(argv[i], "--ui-root") == 0 && i + 1 < argc)
            snprintf(s_ui_root, sizeof(s_ui_root), "%s", argv[++i]);
        else if (strcmp(argv[i], "--open-browser") == 0)
            open_browser = true;
        else
        {
            fprintf(stderr, "spark-ui: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    /* Load fixture banks for the editor */
    const char *bank_paths = spark_env_get("SPARK_FIXTURE_BANK_PATH");
    editor_load_banks(&s_editor, bank_paths);

    signal(SIGINT, s_signal_handler);
    signal(SIGTERM, s_signal_handler);

    /* Build full URLs with protocol prefix */
    char listen_url[270];
    snprintf(listen_url, sizeof(listen_url), "http://%s", http_addr);

    char daemon_full[270];
    snprintf(daemon_full, sizeof(daemon_full), "http://%s", s_daemon_addr);
    strncpy(s_daemon_addr, daemon_full, sizeof(s_daemon_addr) - 1);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_log_set(0);

    struct mg_connection *c = mg_http_listen(&mgr, listen_url, s_ev_handler, &mgr);
    if (!c)
    {
        fprintf(stderr, "spark-ui: failed to listen on %s\n", http_addr);
        return 1;
    }

    printf("spark-ui: serving %s on %s\n", s_ui_root, http_addr);
    printf("spark-ui: proxying /api/* to %s\n", s_daemon_addr);

    if (open_browser)
        s_open_browser(listen_url);

    while (s_running)
        mg_mgr_poll(&mgr, 100);

    mg_mgr_free(&mgr);
    printf("spark-ui: stopped\n");
    return 0;
}
