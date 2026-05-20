#include "engine.h"
#include "project.h"
#include "midi.h"
#include "scene.h"
#include "stage.h"
#include "clock.h"
#include "dmx/dmx.h"
#include "dmx/dmx_out.h"
#include "log.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static spark_stage_t s_stage;
static spark_dmx_backend_t s_dmx_backend;
static spark_dmx_out_t s_dmx_out;
static spark_engine_config_t s_config;
static bool s_initialized = false;
static bool s_running = false;
static bool s_mapping_loaded = false;
static char s_project_path[SPARK_PROJECT_PATH_STRLEN] = "";

static int s_init_midi(void)
{
    if (s_config.midi_device[0] == '\0')
    {
        spark_log_debug("engine: no MIDI device configured");
        return 0;
    }

    int rc = spark_midi_open_by_name(s_config.midi_device);
    if (rc != 0)
    {
        spark_log_error("engine: failed to open MIDI device '%s'", s_config.midi_device);
        return rc;
    }
    spark_log_info("engine: MIDI device opened '%s'", s_config.midi_device);
    return 0;
}

static int s_init_dmx(void)
{
    switch (s_config.dmx_backend_type)
    {
    case SPARK_DMX_BACKEND_OPEN:
        spark_dmx_open_init(&s_dmx_backend, s_config.dmx_port);
        spark_log_info("engine: DMX backend Open DMX on '%s'", s_config.dmx_port);
        break;
    case SPARK_DMX_BACKEND_DUMMY:
    default:
        spark_dmx_dummy_init(&s_dmx_backend);
        spark_log_info("engine: DMX backend dummy");
        break;
    }

    spark_dmx_out_init(&s_dmx_out, &s_dmx_backend, &s_stage);

    int rc = spark_dmx_out_start(&s_dmx_out);
    if (rc != 0)
    {
        spark_log_error("engine: DMX output thread failed to start (%d)", rc);
        return rc;
    }
    spark_log_debug("engine: DMX thread started");
    return 0;
}

static void s_shutdown_dmx(void)
{
    int rc = spark_dmx_out_stop(&s_dmx_out);
    if (rc != 0)
        spark_log_error("engine: DMX thread stop error (%d)", rc);
    else
        spark_log_debug("engine: DMX thread stopped");

    spark_dmx_close(&s_dmx_backend);
    spark_log_debug("engine: DMX backend closed");
}

static void s_shutdown_midi(void)
{
    spark_midi_close_all();
    spark_log_debug("engine: MIDI streams closed");
}

static void s_shutdown_stage(void)
{
    spark_stage_destroy(&s_stage);
    spark_log_debug("engine: stage destroyed");
}

int spark_engine_load_project(const char *path)
{
    if (!s_initialized)
    {
        spark_log_error("engine: not initialized");
        return -1;
    }
    if (s_running)
    {
        spark_log_error("engine: cannot load project while running");
        return -1;
    }

    int rc = spark_project_load(path);
    if (rc != 0)
        return rc;

    if (path)
        snprintf(s_project_path, sizeof(s_project_path), "%s", path);
    else
        s_project_path[0] = '\0';

    s_mapping_loaded = true;
    spark_log_info("engine: project loaded");
    return 0;
}

int spark_engine_init(void)
{
    if (s_initialized)
    {
        spark_log_warn("engine: already initialized");
        return 0;
    }

    memset(&s_stage, 0, sizeof(s_stage));
    memset(&s_dmx_backend, 0, sizeof(s_dmx_backend));
    memset(&s_dmx_out, 0, sizeof(s_dmx_out));
    memset(&s_config, 0, sizeof(s_config));

    spark_stage_init(&s_stage);

    int rc = spark_midi_init();
    if (rc != 0)
    {
        spark_log_error("engine: MIDI init failed (%d)", rc);
        return rc;
    }

    s_initialized = true;
    spark_log_debug("engine: initialized");
    return 0;
}

int spark_engine_start(const spark_engine_config_t *cfg)
{
    if (!s_initialized)
    {
        spark_log_error("engine: not initialized");
        return -1;
    }
    if (s_running)
    {
        spark_log_warn("engine: already running");
        return 0;
    }
    if (!s_mapping_loaded)
    {
        spark_log_error("engine: no project loaded");
        return -1;
    }

    memcpy(&s_config, cfg, sizeof(s_config));

    int rc;

    rc = s_init_midi();
    if (rc != 0)
        spark_log_warn("engine: MIDI init failed, continuing without MIDI");

    rc = s_init_dmx();
    if (rc != 0)
        return rc;

    s_running = true;
    spark_log_info("engine: started");
    return 0;
}

void spark_engine_stop(void)
{
    if (!s_running)
        return;

    s_shutdown_dmx();
    s_shutdown_midi();

    s_running = false;
    spark_log_info("engine: stopped");
}

void spark_engine_destroy(void)
{
    if (s_running)
        spark_engine_stop();

    s_shutdown_stage();
    spark_midi_destroy();
    s_initialized = false;
    spark_log_debug("engine: destroyed");
}

void spark_engine_process_events(void)
{
    if (!s_running)
        return;

    spark_midi_event_t events[SPARK_MIDI_BUFFER_SIZE];
    int n = spark_midi_poll(events, SPARK_MIDI_BUFFER_SIZE);

    for (int i = 0; i < n; i++)
    {
        spark_log_debug("engine: MIDI ch=%d type=%d note=%d vel=%d cc=%d val=%d",
            events[i].channel, events[i].type,
            events[i].note, events[i].velocity,
            events[i].cc, events[i].value);
        spark_stage_apply_midi(&s_stage, &events[i]);
    }
}

int spark_engine_midi_reconnect(void)
{
    if (!s_initialized)
        return -1;

    spark_log_info("engine: MIDI reconnect requested");
    return spark_midi_reconnect();
}

bool spark_engine_is_running(void)
{
    return s_running;
}

const spark_engine_config_t *spark_engine_get_config(void)
{
    return s_running ? &s_config : NULL;
}

const spark_engine_config_t *spark_engine_get_last_config(void)
{
    return s_initialized ? &s_config : NULL;
}

const char *spark_engine_get_project_path(void)
{
    return s_project_path[0] ? s_project_path : NULL;
}
