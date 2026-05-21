#include "http.h"
#include "engine.h"
#include "scene.h"
#include "consts.h"
#include "log.h"
#include "clock.h"

#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#define spark_getpid() ((int)GetCurrentProcessId())
#else
#include <unistd.h>
#define spark_getpid() ((int)getpid())
#endif

static struct mg_mgr s_mgr;
static uint64_t s_start_time_ms;

static const char *s_json_content_type = "Content-Type: application/json\r\n";

/* Redirect mongoose logs through spark_log */
static char s_mg_log_buf[256];
static int s_mg_log_len = 0;

static void s_mg_log_fn(char ch, void *param)
{
    (void)param;
    if (ch == '\n' || s_mg_log_len >= (int)sizeof(s_mg_log_buf) - 1)
    {
        s_mg_log_buf[s_mg_log_len] = '\0';
        if (s_mg_log_len > 0)
            spark_log_debug("mongoose: %s", s_mg_log_buf);
        s_mg_log_len = 0;
    }
    else
    {
        s_mg_log_buf[s_mg_log_len++] = ch;
    }
}

static void s_handle_healthz(struct mg_connection *c)
{
    uint64_t uptime = spark_clock_monotonic_ms() - s_start_time_ms;
    mg_http_reply(c, 200, s_json_content_type,
        "{%m:%m,%m:%d,%m:%llu}\n",
        MG_ESC("version"), MG_ESC(SPARKD_VERSION),
        MG_ESC("pid"), spark_getpid(),
        MG_ESC("uptime_ms"), (unsigned long long)uptime);
}

static void s_handle_engine_state(struct mg_connection *c)
{
    bool is_running = spark_engine_is_running();
    bool blackout = spark_engine_get_blackout();
    const char *project_path = spark_engine_get_project_path();

    if (!is_running)
    {
        mg_http_reply(c, 200, s_json_content_type,
            "{%m:%s,%m:%s,%m:%m}\n",
            MG_ESC("running"), "false",
            MG_ESC("blackout"), blackout ? "true" : "false",
            MG_ESC("project"), MG_ESC(project_path ? project_path : ""));
        return;
    }

    const spark_project_config_t *cfg = spark_engine_get_config();
    const char *backend = "dummy";
    if (cfg->dmx_backend == SPARK_DMX_BACKEND_OPEN)
        backend = "open";

    uint16_t active_count;
    spark_scene_t **active = spark_scene_get_active(&active_count);

    char scenes_buf[1024] = "[";
    size_t pos = 1;
    for (uint16_t i = 0; i < active_count && pos < sizeof(scenes_buf) - 20; i++)
    {
        if (i > 0) scenes_buf[pos++] = ',';
        pos += snprintf(scenes_buf + pos, sizeof(scenes_buf) - pos,
            "\"%s\"", active[i]->def->id);
    }
    scenes_buf[pos++] = ']';
    scenes_buf[pos] = '\0';

    mg_http_reply(c, 200, s_json_content_type,
        "{%m:%s,%m:%s,%m:%m,%m:%m,%m:%m,%m:%m,%m:%s}\n",
        MG_ESC("running"), "true",
        MG_ESC("blackout"), blackout ? "true" : "false",
        MG_ESC("project"), MG_ESC(project_path ? project_path : ""),
        MG_ESC("dmx_backend"), MG_ESC(backend),
        MG_ESC("dmx_device"), MG_ESC(cfg->dmx_device),
        MG_ESC("midi_device"), MG_ESC(cfg->midi_device),
        MG_ESC("active_scenes"), scenes_buf);
}

static void s_handle_engine_start(struct mg_connection *c)
{
    if (spark_engine_is_running())
    {
        mg_http_reply(c, 409, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("engine already running"));
        return;
    }

    int rc = spark_engine_start();
    if (rc != 0)
    {
        mg_http_reply(c, 500, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("engine failed to start"));
        return;
    }

    mg_http_reply(c, 200, s_json_content_type,
        "{%m:%m}\n",
        MG_ESC("status"), MG_ESC("started"));
}

static void s_handle_engine_stop(struct mg_connection *c)
{
    if (!spark_engine_is_running())
    {
        mg_http_reply(c, 409, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("engine not running"));
        return;
    }

    spark_engine_stop();

    mg_http_reply(c, 200, s_json_content_type,
        "{%m:%m}\n",
        MG_ESC("status"), MG_ESC("stopped"));
}

static void s_handle_project_reload(struct mg_connection *c, struct mg_http_message *hm)
{
    if (spark_engine_is_running())
    {
        mg_http_reply(c, 409, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("engine must be stopped before reload"));
        return;
    }

    char *path = NULL;
    if (hm->body.len > 0)
        path = mg_json_get_str(hm->body, "$.path");

    const char *load_path = path;
    if (!load_path || load_path[0] == '\0')
    {
        free(path);
        path = NULL;
        load_path = spark_engine_get_project_path();
    }

    if (!load_path)
    {
        mg_http_reply(c, 400, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("no path given and no previous project loaded"));
        return;
    }

    int rc = spark_engine_load_project(load_path);
    free(path);

    if (rc != 0)
    {
        mg_http_reply(c, 422, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("project load failed"));
        return;
    }

    mg_http_reply(c, 200, s_json_content_type,
        "{%m:%m}\n",
        MG_ESC("status"), MG_ESC("reloaded"));
}

static void s_handle_blackout_get(struct mg_connection *c)
{
    bool blackout = spark_engine_get_blackout();
    mg_http_reply(c, 200, s_json_content_type,
        "{%m:%s}\n",
        MG_ESC("enabled"), blackout ? "true" : "false");
}

static void s_handle_blackout_set(struct mg_connection *c, struct mg_http_message *hm)
{
    bool enabled = false;
    if (hm->body.len > 0)
        mg_json_get_bool(hm->body, "$.enabled", &enabled);

    spark_engine_set_blackout(enabled);
    mg_http_reply(c, 200, s_json_content_type,
        "{%m:%s}\n",
        MG_ESC("enabled"), enabled ? "true" : "false");
}

static void s_handle_midi_reconnect(struct mg_connection *c)
{
    if (!spark_engine_is_running())
    {
        mg_http_reply(c, 409, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("engine not running"));
        return;
    }

    int rc = spark_engine_midi_reconnect();
    if (rc != 0)
    {
        mg_http_reply(c, 500, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("midi reconnect failed"));
        return;
    }

    mg_http_reply(c, 200, s_json_content_type,
        "{%m:%m}\n",
        MG_ESC("status"), MG_ESC("reconnected"));
}

static void s_ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev != MG_EV_HTTP_MSG)
        return;

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    spark_log_info("http: %.*s %.*s",
        (int)hm->method.len, hm->method.buf,
        (int)hm->uri.len, hm->uri.buf);

    if (mg_match(hm->uri, mg_str("/healthz"), NULL))
        s_handle_healthz(c);
    else if (mg_match(hm->uri, mg_str("/api/engine/state"), NULL) &&
             mg_match(hm->method, mg_str("GET"), NULL))
        s_handle_engine_state(c);
    else if (mg_match(hm->uri, mg_str("/api/engine/start"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
        s_handle_engine_start(c);
    else if (mg_match(hm->uri, mg_str("/api/engine/stop"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
        s_handle_engine_stop(c);
    else if (mg_match(hm->uri, mg_str("/api/engine/blackout"), NULL) &&
             mg_match(hm->method, mg_str("GET"), NULL))
        s_handle_blackout_get(c);
    else if (mg_match(hm->uri, mg_str("/api/engine/blackout"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
        s_handle_blackout_set(c, hm);
    else if (mg_match(hm->uri, mg_str("/api/engine/midi/reconnect"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
        s_handle_midi_reconnect(c);
    else if (mg_match(hm->uri, mg_str("/api/project/reload"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
        s_handle_project_reload(c, hm);
    else
    {
        spark_log_warn("http: 404 %.*s", (int)hm->uri.len, hm->uri.buf);
        mg_http_reply(c, 404, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("not found"));
    }
}

int spark_http_init(const char *listen_addr)
{
    mg_log_set_fn(s_mg_log_fn, NULL);
    mg_log_set(MG_LL_INFO);

    mg_mgr_init(&s_mgr);
    s_start_time_ms = spark_clock_monotonic_ms();

    struct mg_connection *c = mg_http_listen(&s_mgr, listen_addr, s_ev_handler, NULL);
    if (!c)
    {
        spark_log_error("http: failed to listen on %s", listen_addr);
        return -1;
    }

    spark_log_info("http: listening on %s", listen_addr);
    return 0;
}

void spark_http_process_events(int timeout_ms)
{
    mg_mgr_poll(&s_mgr, timeout_ms);
}

void spark_http_destroy(void)
{
    mg_mgr_free(&s_mgr);
    spark_log_debug("http: destroyed");
}
