#include "log.h"
#include "stage.h"
#include "clock.h"
#include "dmx/dmx.h"
#include "dmx/dmx_out.h"

#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#define SPARKD_VERSION "0.0.0"

#define PERIOD_MS (uint32_t)(50)

static volatile uint8_t should_keep_running = 1;

void signal_handler(int signum)
{
    (void)signum;
    should_keep_running = 0;
}

typedef struct {
    spark_log_level_t log_level;
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
    }
}

int main(int argc, char **argv)
{
    spark_args_t args = {
        .print_help = false,
        .print_version = false,
        .log_level = SPARK_LOG_INFO
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
    spark_dmx_dummy_init(&dmx_backend);
    spark_log_debug("Dummy DMX backend initialized!");

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

    spark_log_info("Ctrl+C to stop...");
    while (should_keep_running)
    {
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