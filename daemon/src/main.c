#include "consts.h"
#include "log.h"
#include "midi.h"
#include "stage.h"
#include "clock.h"
#include "dmx/dmx.h"
#include "dmx/dmx_out.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#define SPARKD_VERSION "0.1.0"

#define MAIN_LOOP_PERIOD_MS 5

static volatile uint8_t should_keep_running = 1;

void signal_handler(int signum)
{
    (void)signum;
    should_keep_running = 0;
}

typedef struct {
    spark_log_level_t log_level;
    char port[SPARK_SERIAL_PORT_STRLEN];
    char midi_device[SPARK_MIDI_PORT_STRLEN];
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
        else if (strcmp("--midi", argv[i]) == 0 && i + 1 < argc)
        {
            strcpy(args->midi_device, argv[i + 1]);
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
        .port = "COM3",
        .midi_device = ""
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
        printf("Usage: sparkd [--log-level LEVEL] [--port PORT] [--midi DEVICE]\n");
        printf("\n");
        printf("  sparkd --help        Print this help\n");
        printf("  sparkd --version     Print the version\n");
        printf("\n");
        return 0;
    }

    int rc;

    spark_log_init(args.log_level);

    /* MIDI init */
    rc = spark_midi_init();
    if (rc != 0)
    {
        spark_log_error("MIDI init failed (%d)", rc);
        return rc;
    }
    spark_log_debug("MIDI initialized!");

    if (args.midi_device[0] != '\0')
    {
        rc = spark_midi_open_by_name(args.midi_device);
        if (rc == 0)
        {
            spark_log_info("MIDI device opened: %s", args.midi_device);
            spark_midi_set_heartbeat(args.midi_device, 5000);
        }
        else
            spark_log_error("MIDI device open failed: %s", args.midi_device);
    }

    /* Stage init */
    spark_stage_t stage;
    spark_stage_init(&stage);
    spark_log_debug("Stage initialized!");

    /* Scene setup */
    spark_scene_value_t scene_values[] = {
        { .dmx_index = 0, .value = 255, .velocity_scaling = false },
        { .dmx_index = 1, .value = 255, .velocity_scaling = false },
        { .dmx_index = 5, .value = 0,   .velocity_scaling = false },
    };

    spark_scene_t *scene = spark_scene_get(0, 60);
    scene->enabled = true;
    scene->name = "Red Light District";
    scene->id = "red-light-district";
    scene->trigger_mode = SPARK_SCENE_GATE;
    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = scene_values;
    scene->output.value_count = 3;

    /* DMX init */
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
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    spark_log_info("sparkd running. Ctrl+C to stop...");

    spark_midi_event_t midi_events[SPARK_MIDI_BUFFER_SIZE];
    while (should_keep_running)
    {
        int n = spark_midi_poll(midi_events, SPARK_MIDI_BUFFER_SIZE);
        for (int i = 0; i < n; i++)
        {
            spark_log_debug("main: MIDI ch=%d type=%d note=%d vel=%d cc=%d val=%d",
                midi_events[i].channel, midi_events[i].type,
                midi_events[i].note, midi_events[i].velocity,
                midi_events[i].cc, midi_events[i].value);
            spark_stage_apply_midi(&stage, &midi_events[i]);
        }

        if (spark_midi_check_heartbeat() > 0)
        {
            spark_log_warn("MIDI heartbeat lost, reconnecting...");
            spark_midi_reconnect();
        }

        spark_clock_msleep(MAIN_LOOP_PERIOD_MS);
    }

    spark_log_info("Shutting down");

    rc = spark_dmx_out_stop(&dmx_out);
    if (rc != 0)
        spark_log_error("DMX out thread stop error (%d)", rc);
    else
        spark_log_debug("DMX out thread stopped!");

    spark_dmx_close(&dmx_backend);
    spark_log_debug("DMX backend closed!");

    spark_midi_destroy();
    spark_log_debug("MIDI destroyed!");

    spark_stage_destroy(&stage);
    spark_log_debug("Stage destroyed!");

    return 0;
}