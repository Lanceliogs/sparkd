#include "http.h"
#include "engine.h"
#include "consts.h"
#include "log.h"
#include "clock.h"

#include "mongoose.h"

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
    const spark_engine_config_t *cfg = spark_engine_get_config();

    if (!is_running || !cfg)
    {
        mg_http_reply(c, 200, s_json_content_type,
            "{%m:%s}\n",
            MG_ESC("running"), "false");
        return;
    }

    const char *backend = "dummy";
    if (cfg->dmx_backend_type == SPARK_DMX_BACKEND_OPEN)
        backend = "open";

    mg_http_reply(c, 200, s_json_content_type,
        "{%m:%s,%m:%m,%m:%m,%m:%m}\n",
        MG_ESC("running"), "true",
        MG_ESC("dmx_backend"), MG_ESC(backend),
        MG_ESC("dmx_port"), MG_ESC(cfg->dmx_port),
        MG_ESC("midi_device"), MG_ESC(cfg->midi_device));
}

static void s_handle_engine_start(struct mg_connection *c, struct mg_http_message *hm)
{
    if (spark_engine_is_running())
    {
        mg_http_reply(c, 409, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("engine already running"));
        return;
    }

    spark_engine_config_t cfg;
    const spark_engine_config_t *last = spark_engine_get_last_config();
    if (last)
        memcpy(&cfg, last, sizeof(cfg));
    else
        memset(&cfg, 0, sizeof(cfg));

    if (hm->body.len > 0)
    {
        char *val;

        val = mg_json_get_str(hm->body, "$.dmx_port");
        if (val)
        {
            snprintf(cfg.dmx_port, SPARK_SERIAL_PORT_STRLEN, "%s", val);
            free(val);
        }

        val = mg_json_get_str(hm->body, "$.midi_device");
        if (val)
        {
            snprintf(cfg.midi_device, SPARK_MIDI_PORT_STRLEN, "%s", val);
            free(val);
        }

        val = mg_json_get_str(hm->body, "$.dmx_backend");
        if (val)
        {
            if (strcmp(val, "open") == 0)
                cfg.dmx_backend_type = SPARK_DMX_BACKEND_OPEN;
            else
                cfg.dmx_backend_type = SPARK_DMX_BACKEND_DUMMY;
            free(val);
        }
    }

    int rc = spark_engine_start(&cfg);
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

    if (mg_match(hm->uri, mg_str("/healthz"), NULL))
        s_handle_healthz(c);
    else if (mg_match(hm->uri, mg_str("/api/engine/state"), NULL) &&
             mg_match(hm->method, mg_str("GET"), NULL))
        s_handle_engine_state(c);
    else if (mg_match(hm->uri, mg_str("/api/engine/start"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
        s_handle_engine_start(c, hm);
    else if (mg_match(hm->uri, mg_str("/api/engine/stop"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
        s_handle_engine_stop(c);
    else if (mg_match(hm->uri, mg_str("/api/engine/midi/reconnect"), NULL) &&
             mg_match(hm->method, mg_str("POST"), NULL))
        s_handle_midi_reconnect(c);
    else
        mg_http_reply(c, 404, s_json_content_type,
            "{%m:%m}\n",
            MG_ESC("error"), MG_ESC("not found"));
}

int spark_http_init(const char *listen_addr)
{
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
