#include "consts.h"
#include "log.h"
#include "engine.h"
#include "http.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#define MAIN_LOOP_PERIOD_MS 5

static volatile uint8_t s_should_keep_running = 1;

void signal_handler(int signum)
{
    (void)signum;
    s_should_keep_running = 0;
}

#define SPARK_HTTP_ADDR_STRLEN 128
#define SPARK_HTTP_DEFAULT_ADDR "http://127.0.0.1:7600"

typedef struct {
    spark_log_level_t log_level;
    char http_addr[SPARK_HTTP_ADDR_STRLEN];
    char project[SPARK_PROJECT_PATH_STRLEN];
    bool print_help;
    bool print_version;
    bool validate_only;
    bool auto_start;
} spark_args_t;

static void s_parse_cmdline_args(int argc, char **argv, spark_args_t *args)
{
    for (int i = 0; i < argc; i++)
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
        else if (strcmp("--http", argv[i]) == 0 && i + 1 < argc)
        {
            strcpy(args->http_addr, argv[i + 1]);
            i++;
        }
        else if (strcmp("--project", argv[i]) == 0 && i + 1 < argc)
        {
            strcpy(args->project, argv[i + 1]);
            i++;
        }
        else if (strcmp("--validate", argv[i]) == 0)
            args->validate_only = true;
        else if (strcmp("--auto", argv[i]) == 0)
            args->auto_start = true;
    }
}


int main(int argc, char **argv)
{
    spark_args_t args = {
        .print_help = false,
        .print_version = false,
        .log_level = SPARK_LOG_INFO,
        .http_addr = SPARK_HTTP_DEFAULT_ADDR,
        .project = "",
    };

    const char *env_addr = getenv("SPARK_HTTP_ADDR");
    if (env_addr)
        snprintf(args.http_addr, sizeof(args.http_addr), "%s", env_addr);

    const char *env_project = getenv("SPARK_PROJECT_PATH");
    if (env_project)
        snprintf(args.project, sizeof(args.project), "%s", env_project);

    s_parse_cmdline_args(argc, argv, &args);

    if (args.print_version)
    {
        printf("sparkd %s\n", SPARKD_VERSION);
        return 0;
    }

    if (args.print_help)
    {
        printf("sparkd\n");
        printf("---\n");
        printf("Usage: sparkd [--log-level LEVEL] [--http ADDR] [--project PATH] [--auto] [--validate]\n");
        printf("\n");
        printf("  sparkd --help        Print this help\n");
        printf("  sparkd --version     Print the version\n");
        printf("  --http ADDR          HTTP listen address (default: %s)\n", SPARK_HTTP_DEFAULT_ADDR);
        printf("  --project PATH       Project file to load (omit for hardcoded fallback)\n");
        printf("  --auto               Auto-start engine after loading project\n");
        printf("  --validate           Validate project and exit (requires --project)\n");
        printf("\n");
        return 0;
    }

    spark_log_init(args.log_level);

    if (args.validate_only && !args.project[0])
    {
        spark_log_error("--validate requires --project PATH");
        return 1;
    }

    int rc = spark_engine_init();
    if (rc != 0)
        return rc;

    const char *project_path = args.project[0] ? args.project : NULL;
    rc = spark_engine_load_project(project_path);
    if (rc != 0)
    {
        spark_engine_destroy();
        return rc;
    }

    if (args.validate_only)
    {
        spark_log_info("project valid: %s", project_path);
        spark_engine_destroy();
        return 0;
    }

    if (args.auto_start)
    {
        rc = spark_engine_start();
        if (rc != 0)
        {
            spark_engine_destroy();
            return rc;
        }
    }

    rc = spark_http_init(args.http_addr);
    if (rc != 0)
    {
        if (args.auto_start)
            spark_engine_stop();
        spark_engine_destroy();
        return rc;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    spark_log_info("sparkd running. Ctrl+C to stop...");

    while (s_should_keep_running)
    {
        spark_http_process_events(MAIN_LOOP_PERIOD_MS);
        spark_engine_process_events();
        spark_http_broadcast_scene_events();
    }

    spark_log_info("Shutting down");
    spark_http_destroy();
    spark_engine_stop();
    spark_engine_destroy();

    return 0;
}
