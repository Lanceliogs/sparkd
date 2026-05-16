#include "consts.h"
#include "log.h"
#include "stage.h"
#include "clock.h"
#include "dmx/dmx.h"
#include "dmx/dmx_out.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#define SPARKD_VERSION "0.1.0"

#define PERIOD_MS (uint32_t)(500)

static volatile uint8_t should_keep_running = 1;

void signal_handler(int signum)
{
    (void)signum;
    should_keep_running = 0;
}

typedef struct {
    spark_log_level_t log_level;
    char port[SPARK_SERIAL_PORT_STRLEN];
    bool print_help;
    bool print_version;
} spark_args_t;

static void parse_cmdline_args(int argc, char **argv, spark_args_t *args)
{
    for (int i=0 ; i<argc ; i++)
    {
        if (strcmp("--help", argv[i]) == 0)
            args->print_help = true;
        else if (strcmp("--version", argv[i]) == 0)
            args->print_version = true;
        else if (strcmp("--log-level", argv[i]) == 0 && i + 1 < argc)
        {
            spark_log_level_from_string(argv[i + 1], &args->log_level);
            i++;
        }
        else if (strcmp("--port", argv[i]) == 0 && i + 1 < argc)
        {
            strcpy(args->port, argv[i + 1]);
            i++;
        }
    }
}


int main(int argc, char **argv)
{
    spark_args_t args = {
        .print_help = false,
        .print_version = false,
        .log_level = SPARK_LOG_INFO,
        .port = "COM3"
    };
    parse_cmdline_args(argc, argv, &args);

    if (args.print_version)
    {
        printf("sparkd %s\n", SPARKD_VERSION);
        return 0;
    }

    if (args.print_help)
    {
        printf("sparkd\n");
        printf("---\n");
        printf("Usage: sparkd [--log-level LEVEL]\n");
        printf("\n");
        printf("  sparkd --help        Print this help\n");
        printf("  sparkd --version     Print the version\n");
        printf("\n");
        return 0;
    }

    int rc;

    spark_log_init(args.log_level);

    spark_stage_t stage;
    spark_stage_init(&stage);
    spark_log_debug("Stage initialized!");

    spark_dmx_backend_t dmx_backend;
    spark_dmx_open_init(&dmx_backend, args.port);
    spark_log_debug("DMX backend initialized!");

    spark_dmx_out_t dmx_out;
    spark_dmx_out_init(&dmx_out, &dmx_backend, &stage);
    rc = spark_dmx_out_start(&dmx_out);
    if (rc != 0)
    {
        spark_log_error("DMX output thread failed to start (%d)", rc);
        return rc;
    }
    spark_log_debug("DMX thread started!");
    
    signal(SIGINT, signal_handler);   /* Ctrl+C */
    signal(SIGTERM, signal_handler);  /* kill command */

    bool toggle = false;

    midi_event_t evt_on = {
        .channel = 0,
        .note = 60,
        .type = SPARK_MIDI_NOTE_ON,
        .velocity = 127
    };
    midi_event_t evt_off = {
        .channel = 0,
        .note = 60,
        .type = SPARK_MIDI_NOTE_OFF
    };

    spark_scene_value_t scene_values[] = {
        { .dmx_index = 0, .value = 255, .velocity_scaling = false }, // par.dimmer
        { .dmx_index = 1, .value = 255, .velocity_scaling = false }, // par.red
        { .dmx_index = 5, .value = 0,   .velocity_scaling = false }, // par.mode
    };

    spark_scene_t *scene = spark_scene_get(0, 60);
    scene->enabled = true;
    scene->name = "Red Light Disctrict";
    scene->id = "red-light-disctrict";
    scene->trigger_mode = SPARK_SCENE_GATE;
    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = scene_values;
    scene->output.value_count = 3;

    spark_log_info("Ctrl+C to stop...");
    while (should_keep_running)
    {
        if (toggle)
        {
            spark_log_debug("main: sending NOTE_ON ch=0 note=60 vel=127");
            spark_stage_apply_midi(&stage, &evt_on);
        }
        else
        {
            spark_log_debug("main: sending NOTE_OFF ch=0 note=60");
            spark_stage_apply_midi(&stage, &evt_off);
        }
        toggle = !toggle;
        spark_clock_msleep(PERIOD_MS);
    }

    spark_log_info("Shutting down");

    rc = spark_dmx_out_stop(&dmx_out);
    if (rc != 0)
        spark_log_error("DMX out thread stop error (%d)", rc);
    else
        spark_log_debug("DMX out thread stopped!");

    spark_dmx_close(&dmx_backend);
    spark_log_debug("DMX backend closed!");

    spark_stage_destroy(&stage);
    spark_log_debug("Stage destroyed!");

    return 0;
}