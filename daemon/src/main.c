#include "consts.h"
#include "log.h"
#include "engine.h"
#include "http.h"
#include "scene.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#define MAIN_LOOP_PERIOD_MS 5

static volatile uint8_t should_keep_running = 1;

void signal_handler(int signum)
{
    (void)signum;
    should_keep_running = 0;
}

#define SPARK_HTTP_ADDR_STRLEN 128
#define SPARK_HTTP_DEFAULT_ADDR "http://127.0.0.1:7600"

typedef struct {
    spark_log_level_t log_level;
    char port[SPARK_SERIAL_PORT_STRLEN];
    char midi_device[SPARK_MIDI_PORT_STRLEN];
    char http_addr[SPARK_HTTP_ADDR_STRLEN];
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
        else if (strcmp("--http", argv[i]) == 0 && i + 1 < argc)
        {
            strcpy(args->http_addr, argv[i + 1]);
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
        .midi_device = "",
        .http_addr = SPARK_HTTP_DEFAULT_ADDR
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
        printf("Usage: sparkd [--log-level LEVEL] [--port PORT] [--midi DEVICE] [--http ADDR]\n");
        printf("\n");
        printf("  sparkd --help        Print this help\n");
        printf("  sparkd --version     Print the version\n");
        printf("  --http ADDR          HTTP listen address (default: %s)\n", SPARK_HTTP_DEFAULT_ADDR);
        printf("\n");
        return 0;
    }

    spark_log_init(args.log_level);

    int rc = spark_engine_init();
    if (rc != 0)
        return rc;

    spark_engine_config_t cfg = {0};
    strncpy(cfg.dmx_port, args.port, SPARK_SERIAL_PORT_STRLEN - 1);
    strncpy(cfg.midi_device, args.midi_device, SPARK_MIDI_PORT_STRLEN - 1);
    cfg.dmx_backend_type = SPARK_DMX_BACKEND_OPEN;

    rc = spark_engine_start(&cfg);
    if (rc != 0)
    {
        spark_engine_destroy();
        return rc;
    }

    /* Temporary hardcoded scene */
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

    rc = spark_http_init(args.http_addr);
    if (rc != 0)
    {
        spark_engine_stop();
        spark_engine_destroy();
        return rc;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    spark_log_info("sparkd running. Ctrl+C to stop...");

    while (should_keep_running)
    {
        spark_http_process_events(MAIN_LOOP_PERIOD_MS);
        spark_engine_process_events();
    }

    spark_log_info("Shutting down");
    spark_http_destroy();
    spark_engine_stop();
    spark_engine_destroy();

    return 0;
}
