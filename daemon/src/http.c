#include "http.h"
#include "engine.h"
#include "midi.h"
#include "scene.h"
#include "fixture_bank.h"
#include "dmx/dmx.h"
#include "spark_atomic.h"
#include "env.h"
#include "consts.h"
#include "log.h"
#include "clock.h"

#include "mongoose.h"
#include "mg_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

static void s_ws_broadcast(const char *msg, size_t len)
{
    for (struct mg_connection *c = s_mgr.conns; c; c = c->next)
    {
        if (c->is_websocket)
            mg_ws_send(c, msg, len, WEBSOCKET_OP_TEXT);
    }
}

static void s_ws_send_state(struct mg_connection *c)
{
    bool running = spark_engine_is_running();
    bool blackout = spark_engine_get_blackout();
    const char *project = spark_engine_get_project_path();

    uint16_t active_count;
    spark_scene_t **active = spark_scene_get_active(&active_count);

    char buf[2048];
    size_t pos = snprintf(buf, sizeof(buf),
        "{\"type\":\"state\",\"running\":%s,\"blackout\":%s,\"project\":\"%s\",\"active_scenes\":[",
        running ? "true" : "false",
        blackout ? "true" : "false",
        project ? project : "");

    for (uint16_t i = 0; i < active_count && pos < sizeof(buf) - 80; i++)
    {
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, sizeof(buf) - pos, "\"%s\"", active[i]->def->id);
    }
    pos += snprintf(buf + pos, sizeof(buf) - pos, "]}");

    mg_ws_send(c, buf, pos, WEBSOCKET_OP_TEXT);
}

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
    s_ws_broadcast("{\"type\":\"started\"}", 18);
    s_ws_broadcast("{\"type\":\"blackout\",\"enabled\":false}", 35);
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
    s_ws_broadcast("{\"type\":\"stopped\"}", 18);
    s_ws_broadcast("{\"type\":\"blackout\",\"enabled\":true}", 34);
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

static void s_handle_fixture_bank_reload(struct mg_connection *c)
{
    const char *paths = spark_env_get("SPARK_FIXTURE_BANK_PATH");
    spark_fixture_bank_reload(paths);
    mg_http_reply(c, 200, s_json_content_type,
        "{%m:%m}\n",
        MG_ESC("status"), MG_ESC("bank reloaded"));
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

    char buf[64];
    int len = snprintf(buf, sizeof(buf),
        "{\"type\":\"blackout\",\"enabled\":%s}", enabled ? "true" : "false");
    s_ws_broadcast(buf, len);
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
    if (rc < 0)
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

static void s_handle_scenes_list(struct mg_connection *c)
{
    uint16_t count;
    const spark_scene_def_t *defs = spark_scene_get_defs(&count);

    /* The big ass format in snprintf has a size of 96, so at least max scene defs times 128 */
    char buf[SPARK_SCENE_DEFS_MAX * 128] = "[";
    size_t pos = 1;

    for (uint16_t i = 0; i < count && pos < sizeof(buf) - 200; i++)
    {
        const spark_scene_def_t *d = &defs[i];
        const char *type = d->output_mode == SPARK_SCENE_STATIC ? "static" : "sequence";
        const char *trigger = d->trigger_mode == SPARK_SCENE_GATE ? "gate" : "toggle";
        if (i > 0) buf[pos++] = ',';
        pos += snprintf(buf + pos, sizeof(buf) - pos,
            "{\"id\":\"%s\",\"name\":\"%s\",\"type\":\"%s\",\"trigger_mode\":\"%s\","
            "\"channel\":%d,\"note\":%d,\"enabled\":%s}",
            d->id, d->name, type, trigger,
            d->channel + 1, d->note,
            d->enabled ? "true" : "false");
    }
    buf[pos++] = ']';
    buf[pos] = '\0';

    mg_http_reply(c, 200, s_json_content_type, "%s\n", buf);
}

static void s_handle_scene_activate(struct mg_connection *c, struct mg_str scene_id)
{
    char id[64];
    size_t len = scene_id.len < sizeof(id) - 1 ? scene_id.len : sizeof(id) - 1;
    memcpy(id, scene_id.buf, len);
    id[len] = '\0';

    uint16_t count;
    const spark_scene_def_t *defs = spark_scene_get_defs(&count);

    for (uint16_t i = 0; i < count; i++)
    {
        if (strcmp(defs[i].id, id) == 0)
        {
            spark_scene_t *scene = spark_scene_get(defs[i].channel, defs[i].note);
            if (scene->def && !scene->active)
                spark_scene_activate(scene, 127);
            mg_http_reply(c, 200, s_json_content_type,
                "{%m:%m}\n", MG_ESC("status"), MG_ESC("activated"));
            return;
        }
    }

    mg_http_reply(c, 404, s_json_content_type,
        "{%m:%m}\n", MG_ESC("error"), MG_ESC("scene not found"));
}

static void s_handle_scene_release(struct mg_connection *c, struct mg_str scene_id)
{
    char id[64];
    size_t len = scene_id.len < sizeof(id) - 1 ? scene_id.len : sizeof(id) - 1;
    memcpy(id, scene_id.buf, len);
    id[len] = '\0';

    uint16_t count;
    const spark_scene_def_t *defs = spark_scene_get_defs(&count);

    for (uint16_t i = 0; i < count; i++)
    {
        if (strcmp(defs[i].id, id) == 0)
        {
            spark_scene_t *scene = spark_scene_get(defs[i].channel, defs[i].note);
            if (scene->def && scene->active)
                spark_scene_deactivate(scene);
            mg_http_reply(c, 200, s_json_content_type,
                "{%m:%m}\n", MG_ESC("status"), MG_ESC("released"));
            return;
        }
    }

    mg_http_reply(c, 404, s_json_content_type,
        "{%m:%m}\n", MG_ESC("error"), MG_ESC("scene not found"));
}

static void s_handle_midi_status(struct mg_connection *c)
{
    spark_midi_port_status_t ports[SPARK_MIDI_MAX_DEVICES];
    int count = spark_midi_get_status(ports, SPARK_MIDI_MAX_DEVICES);

    char buf[2048];
    size_t pos = snprintf(buf, sizeof(buf), "{\"port_count\":%d,\"ports\":[", count);

    for (int i = 0; i < count && pos < sizeof(buf) - 128; i++)
    {
        if (i > 0) buf[pos++] = ',';
        pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos,
            "{\"pattern\":\"%s\",\"connected\":%s,\"last_activity_ms\":%llu}",
            ports[i].pattern,
            ports[i].connected ? "true" : "false",
            (unsigned long long)ports[i].last_activity_ms);
    }

    pos += (size_t)snprintf(buf + pos, sizeof(buf) - pos, "]}\n");
    mg_http_reply(c, 200, s_json_content_type, "%s", buf);
}

static const char *s_dmx_state_str(spark_dmx_state_t state)
{
    switch (state)
    {
    case SPARK_DMX_DISCONNECTED: return "disconnected";
    case SPARK_DMX_CONNECTING:   return "connecting";
    case SPARK_DMX_CONNECTED:    return "connected";
    case SPARK_DMX_ERROR:        return "error";
    default:                     return "unknown";
    }
}

static void s_handle_dmx_status(struct mg_connection *c)
{
    const spark_dmx_backend_t *backend = spark_engine_get_dmx_backend();

    if (!backend)
    {
        mg_http_reply(c, 200, s_json_content_type,
            "{\"backend\":\"none\",\"state\":\"disconnected\","
            "\"stats\":{\"frames_sent\":0,\"write_errors\":0,\"reconnects\":0}}\n");
        return;
    }

    const spark_project_config_t *cfg = spark_engine_get_config();
    const char *backend_name = cfg->dmx_backend == SPARK_DMX_BACKEND_OPEN ? "open" : "dummy";
    spark_dmx_state_t state = spark_atomic_load(&backend->state);
    uint64_t frames = spark_atomic_load_u64(&backend->frames_sent);
    uint64_t errors = spark_atomic_load_u64(&backend->write_errors);
    uint64_t reconn = spark_atomic_load_u64(&backend->reconnects);

    mg_http_reply(c, 200, s_json_content_type,
        "{\"backend\":\"%s\",\"state\":\"%s\","
        "\"stats\":{\"frames_sent\":%llu,\"write_errors\":%llu,\"reconnects\":%llu}}\n",
        backend_name, s_dmx_state_str(state),
        (unsigned long long)frames,
        (unsigned long long)errors,
        (unsigned long long)reconn);
}

static void s_ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_WS_OPEN)
    {
        spark_log_info("ws: client connected");
        s_ws_send_state(c);
        return;
    }

    if (ev == MG_EV_WS_MSG)
        return;

    if (ev != MG_EV_HTTP_MSG)
        return;

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;

    if (mg_match(hm->uri, mg_str("/ws"), NULL))
    {
        mg_ws_upgrade(c, hm, NULL);
        return;
    }

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
    else if (mg_match(hm->uri, mg_str("/api/fixtures/bank/reload"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
        s_handle_fixture_bank_reload(c);
    else if (mg_match(hm->uri, mg_str("/api/midi/status"), NULL) &&
             mg_match(hm->method, mg_str("GET"), NULL))
        s_handle_midi_status(c);
    else if (mg_match(hm->uri, mg_str("/api/dmx/status"), NULL) &&
             mg_match(hm->method, mg_str("GET"), NULL))
        s_handle_dmx_status(c);
    else if (mg_match(hm->uri, mg_str("/api/scenes"), NULL) &&
             mg_match(hm->method, mg_str("GET"), NULL))
        s_handle_scenes_list(c);
    else if (mg_match(hm->uri, mg_str("/api/scenes/*/activate"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
    {
        struct mg_str caps[1];
        mg_match(hm->uri, mg_str("/api/scenes/*/activate"), caps);
        s_handle_scene_activate(c, caps[0]);
    }
    else if (mg_match(hm->uri, mg_str("/api/scenes/*/release"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
    {
        struct mg_str caps[1];
        mg_match(hm->uri, mg_str("/api/scenes/*/release"), caps);
        s_handle_scene_release(c, caps[0]);
    }
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

void spark_http_broadcast_scene_events(void)
{
    uint16_t count;
    const spark_scene_event_t *events = spark_scene_get_events(&count);

    for (uint16_t i = 0; i < count; i++)
    {
        char buf[256];
        int len;
        if (events[i].active)
            len = snprintf(buf, sizeof(buf),
                "{\"type\":\"scene_on\",\"id\":\"%s\",\"velocity\":%u}",
                events[i].id, events[i].velocity);
        else
            len = snprintf(buf, sizeof(buf),
                "{\"type\":\"scene_off\",\"id\":\"%s\"}",
                events[i].id);
        s_ws_broadcast(buf, len);
    }

    spark_scene_clear_events();
}

void spark_http_destroy(void)
{
    mg_mgr_free(&s_mgr);
    spark_log_debug("http: destroyed");
}
