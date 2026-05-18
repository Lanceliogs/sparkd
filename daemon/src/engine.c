#include "engine.h"
#include "midi.h"
#include "stage.h"
#include "clock.h"
#include "dmx/dmx.h"
#include "dmx/dmx_out.h"
#include "log.h"

#include <stdbool.h>
#include <string.h>

static spark_stage_t stage;
static spark_dmx_backend_t dmx_backend;
static spark_dmx_out_t dmx_out;
static spark_engine_config_t config;
static bool initialized = false;
static bool running = false;

static int init_midi(void)
{
    if (config.midi_device[0] == '\0')
    {
        spark_log_debug("engine: no MIDI device configured");
        return 0;
    }

    int rc = spark_midi_open_by_name(config.midi_device);
    if (rc != 0)
    {
        spark_log_error("engine: failed to open MIDI device '%s'", config.midi_device);
        return rc;
    }
    spark_log_info("engine: MIDI device opened '%s'", config.midi_device);
    return 0;
}

static int init_stage(void)
{
    spark_stage_init(&stage);
    spark_log_debug("engine: stage initialized");
    return 0;
}

static int init_dmx(void)
{
    switch (config.dmx_backend_type)
    {
    case SPARK_DMX_BACKEND_OPEN:
        spark_dmx_open_init(&dmx_backend, config.dmx_port);
        spark_log_info("engine: DMX backend Open DMX on '%s'", config.dmx_port);
        break;
    case SPARK_DMX_BACKEND_DUMMY:
    default:
        spark_dmx_dummy_init(&dmx_backend);
        spark_log_info("engine: DMX backend dummy");
        break;
    }

    spark_dmx_out_init(&dmx_out, &dmx_backend, &stage);

    int rc = spark_dmx_out_start(&dmx_out);
    if (rc != 0)
    {
        spark_log_error("engine: DMX output thread failed to start (%d)", rc);
        return rc;
    }
    spark_log_debug("engine: DMX thread started");
    return 0;
}

static void shutdown_dmx(void)
{
    int rc = spark_dmx_out_stop(&dmx_out);
    if (rc != 0)
        spark_log_error("engine: DMX thread stop error (%d)", rc);
    else
        spark_log_debug("engine: DMX thread stopped");

    spark_dmx_close(&dmx_backend);
    spark_log_debug("engine: DMX backend closed");
}

static void shutdown_midi(void)
{
    spark_midi_close_all();
    spark_log_debug("engine: MIDI streams closed");
}

static void shutdown_stage(void)
{
    spark_stage_destroy(&stage);
    spark_log_debug("engine: stage destroyed");
}

int spark_engine_init(void)
{
    if (initialized)
    {
        spark_log_warn("engine: already initialized");
        return 0;
    }

    memset(&stage, 0, sizeof(stage));
    memset(&dmx_backend, 0, sizeof(dmx_backend));
    memset(&dmx_out, 0, sizeof(dmx_out));
    memset(&config, 0, sizeof(config));

    int rc = spark_midi_init();
    if (rc != 0)
    {
        spark_log_error("engine: MIDI init failed (%d)", rc);
        return rc;
    }

    initialized = true;
    spark_log_debug("engine: initialized");
    return 0;
}

int spark_engine_start(const spark_engine_config_t *cfg)
{
    if (!initialized)
    {
        spark_log_error("engine: not initialized");
        return -1;
    }
    if (running)
    {
        spark_log_warn("engine: already running");
        return 0;
    }

    memcpy(&config, cfg, sizeof(config));

    int rc;

    rc = init_midi();
    if (rc != 0)
        spark_log_warn("engine: MIDI init failed, continuing without MIDI");

    rc = init_stage();
    if (rc != 0)
        return rc;

    rc = init_dmx();
    if (rc != 0)
        return rc;

    running = true;
    spark_log_info("engine: started");
    return 0;
}

void spark_engine_stop(void)
{
    if (!running)
        return;

    shutdown_dmx();
    shutdown_midi();
    shutdown_stage();

    running = false;
    spark_log_info("engine: stopped");
}

void spark_engine_destroy(void)
{
    if (running)
        spark_engine_stop();

    spark_midi_destroy();
    initialized = false;
    spark_log_debug("engine: destroyed");
}

void spark_engine_process_events(void)
{
    if (!running)
        return;

    spark_midi_event_t events[SPARK_MIDI_BUFFER_SIZE];
    int n = spark_midi_poll(events, SPARK_MIDI_BUFFER_SIZE);

    for (int i = 0; i < n; i++)
    {
        spark_log_debug("engine: MIDI ch=%d type=%d note=%d vel=%d cc=%d val=%d",
            events[i].channel, events[i].type,
            events[i].note, events[i].velocity,
            events[i].cc, events[i].value);
        spark_stage_apply_midi(&stage, &events[i]);
    }
}

int spark_engine_midi_reconnect(void)
{
    if (!initialized)
        return -1;

    spark_log_info("engine: MIDI reconnect requested");
    return spark_midi_reconnect();
}

bool spark_engine_is_running(void)
{
    return running;
}

const spark_engine_config_t *spark_engine_get_config(void)
{
    return running ? &config : NULL;
}
