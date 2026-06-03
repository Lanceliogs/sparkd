#include "mongoose.h"
#include "env.h"
#include "fs.h"
#include "log.h"
#include "clock.h"
#include "auth.h"
#include "consts.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#endif

#define DEFAULT_DAEMON_ADDR "127.0.0.1:7600"
#define DEFAULT_UI_ADDR     "127.0.0.1:7601"

#define HEALTHZ_POLL_INTERVAL_MS 200
#define HEALTHZ_POLL_MAX_ATTEMPTS 25

/* ---- HTTP one-shot request ---- */

typedef struct {
    const char *method;
    const char *path;
    const char *url;
    const char *req_body;
    bool done;
    int status;
    char body[4096];
} request_ctx_t;

static void s_ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    request_ctx_t *ctx = (request_ctx_t *)c->fn_data;

    if (ev == MG_EV_CONNECT)
    {
        struct mg_str host = mg_url_host(ctx->url);
        const char *token = spark_env_get("SPARK_AUTH_TOKEN");
        if (ctx->req_body)
        {
            size_t blen = strlen(ctx->req_body);
            if (token && token[0])
            {
                mg_printf(c,
                    "%s %s HTTP/1.0\r\n"
                    "Host: %.*s\r\n"
                    "Authorization: Bearer %s\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %lu\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "%s",
                    ctx->method, ctx->path,
                    (int)host.len, host.buf,
                    token,
                    (unsigned long)blen, ctx->req_body);
            }
            else
            {
                mg_printf(c,
                    "%s %s HTTP/1.0\r\n"
                    "Host: %.*s\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %lu\r\n"
                    "Connection: close\r\n"
                    "\r\n"
                    "%s",
                    ctx->method, ctx->path,
                    (int)host.len, host.buf,
                    (unsigned long)blen, ctx->req_body);
            }
        }
        else
        {
            if (token && token[0])
            {
                mg_printf(c,
                    "%s %s HTTP/1.0\r\n"
                    "Host: %.*s\r\n"
                    "Authorization: Bearer %s\r\n"
                    "Connection: close\r\n"
                    "\r\n",
                    ctx->method, ctx->path,
                    (int)host.len, host.buf,
                    token);
            }
            else
            {
                mg_printf(c,
                    "%s %s HTTP/1.0\r\n"
                    "Host: %.*s\r\n"
                    "Connection: close\r\n"
                    "\r\n",
                    ctx->method, ctx->path,
                    (int)host.len, host.buf);
            }
        }
    }
    else if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        ctx->status = mg_http_status(hm);
        size_t len = hm->body.len < sizeof(ctx->body) - 1
            ? hm->body.len : sizeof(ctx->body) - 1;
        memcpy(ctx->body, hm->body.buf, len);
        ctx->body[len] = '\0';
        ctx->done = true;
        c->is_closing = 1;
    }
    else if (ev == MG_EV_ERROR)
    {
        ctx->status = -1;
        ctx->done = true;
    }
}

static int s_http_request(const char *addr, const char *method,
                          const char *path, const char *body,
                          request_ctx_t *out)
{
    char url[512];
    snprintf(url, sizeof(url), "http://%s%s", addr, path);

    mg_log_set(0);
    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    request_ctx_t ctx = {
        .method = method,
        .path = path,
        .url = url,
        .req_body = body,
    };

    struct mg_connection *c = mg_http_connect(&mgr, url, s_ev_handler, &ctx);
    if (!c)
    {
        mg_mgr_free(&mgr);
        if (out) { out->status = -1; out->body[0] = '\0'; }
        return -1;
    }

    while (!ctx.done)
        mg_mgr_poll(&mgr, 50);

    mg_mgr_free(&mgr);

    if (out)
    {
        out->status = ctx.status;
        strncpy(out->body, ctx.body, sizeof(out->body) - 1);
        out->body[sizeof(out->body) - 1] = '\0';
    }
    return ctx.status;
}

/* ---- Binary discovery ---- */

static int s_find_binary(const char *name, char *out, size_t out_size)
{
    char exe_dir[1024];
    if (spark_fs_exe_dir(exe_dir, sizeof(exe_dir)) == 0)
    {
#ifdef _WIN32
        snprintf(out, out_size, "%s/%s.exe", exe_dir, name);
        if (spark_fs_file_exists(out)) return 0;
        snprintf(out, out_size, "%s/../%s.exe", exe_dir, name);
        if (spark_fs_file_exists(out)) return 0;
#else
        snprintf(out, out_size, "%s/%s", exe_dir, name);
        if (spark_fs_file_exists(out)) return 0;
        snprintf(out, out_size, "%s/../%s", exe_dir, name);
        if (spark_fs_file_exists(out)) return 0;
#endif
    }

    /* Fallback: assume it's on PATH */
    snprintf(out, out_size, "%s", name);
    return 0;
}

/* ---- Process spawning ---- */

#ifdef _WIN32
static int s_spawn_detached(const char *binary, char *const args[], int argc)
{
    (void)binary;
    char cmdline[4096] = {0};
    for (int i = 0; i < argc; i++)
    {
        if (i > 0) strcat(cmdline, " ");
        strcat(cmdline, args[i]);
    }

    STARTUPINFOA si = { .cb = sizeof(si) };
    PROCESS_INFORMATION pi = {0};

    if (!CreateProcessA(NULL, cmdline, NULL, NULL, FALSE,
                        DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
                        NULL, NULL, &si, &pi))
        return -1;

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
#else
static int s_spawn_detached(const char *binary, char *const args[],
                            int argc)
{
    (void)argc;
    pid_t pid = fork();
    if (pid < 0) return -1;

    if (pid == 0)
    {
        setsid();

        /* Redirect stdout/stderr to log file */
        char home[512];
        char logdir[512], logpath[600];
        if (spark_fs_home(home, sizeof(home)) == 0)
            spark_fs_path_join(logdir, sizeof(logdir), home, ".spark");
        else
            snprintf(logdir, sizeof(logdir), "/tmp");

        spark_fs_mkdir_p(logdir);

        const char *basename_str = strrchr(binary, '/');
        basename_str = basename_str ? basename_str + 1 : binary;
        snprintf(logpath, sizeof(logpath), "%s/%s.log", logdir, basename_str);

        FILE *log = fopen(logpath, "a");
        if (log)
        {
            int fd = fileno(log);
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            fclose(log);
        }

        close(STDIN_FILENO);
        execvp(binary, args);
        _exit(127);
    }

    return 0;
}
#endif

/* ---- Healthz polling ---- */

static int s_poll_healthz(const char *addr, int max_attempts, int interval_ms)
{
    for (int i = 0; i < max_attempts; i++)
    {
        spark_clock_msleep((uint32_t)interval_ms);
        request_ctx_t resp = {0};
        int status = s_http_request(addr, "GET", "/healthz", NULL, &resp);
        if (status == 200)
            return 0;
    }
    return -1;
}

/* ---- Subcommand: daemon / ui ---- */

static int s_cmd_service_up(const char *service_name, const char *binary_name,
                            const char *addr, int passthrough_argc,
                            char **passthrough_argv)
{
    /* Check if already running */
    request_ctx_t resp = {0};
    int status = s_http_request(addr, "GET", "/healthz", NULL, &resp);
    if (status == 200)
    {
        struct mg_str b = mg_str(resp.body);
        long pid = mg_json_get_long(b, "$.pid", 0);
        printf("%s: already running (pid %ld)\n", service_name, pid);
        return 0;
    }

    char binary[1024];
    s_find_binary(binary_name, binary, sizeof(binary));

    /* Build args: binary + passthrough */
    int total_args = 1 + passthrough_argc + 1;
    char **args = calloc((size_t)total_args, sizeof(char *));
    args[0] = binary;
    for (int i = 0; i < passthrough_argc; i++)
        args[1 + i] = passthrough_argv[i];
    args[total_args - 1] = NULL;

    if (s_spawn_detached(binary, args, total_args - 1) != 0)
    {
        fprintf(stderr, "%s: failed to spawn %s\n", service_name, binary);
        free(args);
        return 1;
    }
    free(args);

    printf("%s: starting...\n", service_name);

    if (s_poll_healthz(addr, HEALTHZ_POLL_MAX_ATTEMPTS,
                       HEALTHZ_POLL_INTERVAL_MS) != 0)
    {
        fprintf(stderr, "%s: failed to start (no healthz response after %dms)\n",
            service_name,
            HEALTHZ_POLL_MAX_ATTEMPTS * HEALTHZ_POLL_INTERVAL_MS);
        return 1;
    }

    /* Read PID from healthz */
    status = s_http_request(addr, "GET", "/healthz", NULL, &resp);
    if (status == 200)
    {
        struct mg_str b = mg_str(resp.body);
        long pid = mg_json_get_long(b, "$.pid", 0);
        printf("%s: running (pid %ld)\n", service_name, pid);
    }
    else
    {
        printf("%s: running\n", service_name);
    }

    return 0;
}

static int s_cmd_service_down(const char *service_name, const char *addr)
{
    request_ctx_t resp = {0};
    int status = s_http_request(addr, "POST", "/shutdown", NULL, &resp);

    if (status == 200)
    {
        printf("%s: stopped\n", service_name);
        return 0;
    }
    else if (status < 0)
    {
        printf("%s: not running\n", service_name);
        return 0;
    }
    else
    {
        fprintf(stderr, "%s: shutdown failed (HTTP %d)\n", service_name, status);
        return 1;
    }
}

static int s_cmd_service_status(const char *service_name, const char *addr)
{
    request_ctx_t resp = {0};
    int status = s_http_request(addr, "GET", "/healthz", NULL, &resp);

    if (status == 200)
    {
        struct mg_str b = mg_str(resp.body);
        long pid = mg_json_get_long(b, "$.pid", 0);
        long uptime = mg_json_get_long(b, "$.uptime_ms", 0);
        printf("%s: running (pid %ld, uptime %lds)\n",
            service_name, pid, uptime / 1000);
        return 0;
    }
    else
    {
        printf("%s: not running\n", service_name);
        return 1;
    }
}

static void s_normalize_addr(char *addr, size_t size)
{
    if (strncmp(addr, "0.0.0.0", 7) == 0)
    {
        char port_part[64] = "";
        if (addr[7] == ':')
            snprintf(port_part, sizeof(port_part), "%s", addr + 7);
        snprintf(addr, size, "127.0.0.1%s", port_part);
    }
}

static int s_handle_service_subcommand(const char *service_name,
                                       const char *binary_name,
                                       const char *default_addr,
                                       const char *env_var,
                                       int argc, char **argv)
{
    char addr[256];
    const char *env_addr = spark_env_get(env_var);
    snprintf(addr, sizeof(addr), "%s", env_addr ? env_addr : default_addr);
    s_normalize_addr(addr, sizeof(addr));

    /* Parse: [--http ADDR] <action> [passthrough...] */
    const char *action = NULL;
    int passthrough_start = -1;

    for (int i = 0; i < argc; i++)
    {
        if (strcmp(argv[i], "--http") == 0 && i + 1 < argc)
        {
            snprintf(addr, sizeof(addr), "%s", argv[++i]);
        }
        else if (!action)
        {
            action = argv[i];
            passthrough_start = i + 1;
        }
        else
        {
            break;
        }
    }

    if (!action)
    {
        fprintf(stderr, "sparkctl %s: missing action (up|down|status)\n",
            service_name);
        return 1;
    }

    if (strcmp(action, "up") == 0)
    {
        int pt_argc = (passthrough_start >= 0) ? argc - passthrough_start : 0;
        char **pt_argv = (pt_argc > 0) ? &argv[passthrough_start] : NULL;
        return s_cmd_service_up(service_name, binary_name, addr,
                                pt_argc, pt_argv);
    }
    else if (strcmp(action, "down") == 0)
    {
        return s_cmd_service_down(service_name, addr);
    }
    else if (strcmp(action, "status") == 0)
    {
        return s_cmd_service_status(service_name, addr);
    }
    else
    {
        fprintf(stderr, "sparkctl %s: unknown action '%s'\n",
            service_name, action);
        return 1;
    }
}

/* ---- Legacy flat commands (engine control) ---- */

typedef struct {
    const char *name;
    const char *method;
    const char *path;
} flat_command_t;

static const flat_command_t s_flat_commands[] = {
    { "status",         "GET",  "/api/engine/state"          },
    { "start",          "POST", "/api/engine/start"          },
    { "stop",           "POST", "/api/engine/stop"           },
    { "set-blackout",   "POST", "/api/engine/blackout"       },
    { "clear-blackout", "POST", "/api/engine/blackout"       },
    { "reconnect-midi", "POST", "/api/engine/midi/reconnect" },
    { "reload",         "POST", "/api/project/reload"        },
    { "healthz",        "GET",  "/healthz"                   },
};

#define FLAT_COMMAND_COUNT (sizeof(s_flat_commands) / sizeof(s_flat_commands[0]))

static void s_print_engine_status(const char *body)
{
    struct mg_str b = mg_str(body);
    bool running = false;
    bool blackout = false;
    mg_json_get_bool(b, "$.running", &running);
    mg_json_get_bool(b, "$.blackout", &blackout);

    char *project = mg_json_get_str(b, "$.project");

    printf("running:  %s\n", running ? "yes" : "no");
    printf("blackout: %s\n", blackout ? "yes" : "no");
    printf("project:  %s\n", project ? project : "");
    free(project);
}

/* ---- Usage ---- */

static void usage(void)
{
    fprintf(stderr, "sparkctl - control sparkd and spark-ui\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: sparkctl [--http ADDR] <command> [args...]\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Lifecycle commands:\n");
    fprintf(stderr, "  daemon up [OPTS]   Start sparkd in background (OPTS passed to sparkd)\n");
    fprintf(stderr, "  daemon down        Stop sparkd gracefully\n");
    fprintf(stderr, "  daemon status      Check if sparkd is running\n");
    fprintf(stderr, "  ui up [OPTS]       Start spark-ui in background (OPTS passed to spark-ui)\n");
    fprintf(stderr, "  ui down            Stop spark-ui gracefully\n");
    fprintf(stderr, "  ui status          Check if spark-ui is running\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Engine commands:\n");
    fprintf(stderr, "  status             Get engine state\n");
    fprintf(stderr, "  start              Start the engine\n");
    fprintf(stderr, "  stop               Stop the engine\n");
    fprintf(stderr, "  set-blackout       Enable blackout\n");
    fprintf(stderr, "  clear-blackout     Disable blackout\n");
    fprintf(stderr, "  reload [PATH]      Reload project (engine must be stopped)\n");
    fprintf(stderr, "  reconnect-midi     Reconnect MIDI device\n");
    fprintf(stderr, "  healthz            Health check\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --http ADDR        Daemon address (default: %s)\n", DEFAULT_DAEMON_ADDR);
    fprintf(stderr, "\n");
    fprintf(stderr, "Environment:\n");
    fprintf(stderr, "  SPARK_HTTP_ADDR    Override default daemon address\n");
    fprintf(stderr, "  SPARK_UI_HTTP_ADDR Override default UI address\n");
}

/* ---- Main ---- */

int main(int argc, char **argv)
{
    spark_env_load();

    char daemon_addr[256];
    const char *env_addr = spark_env_get("SPARK_HTTP_ADDR");
    snprintf(daemon_addr, sizeof(daemon_addr), "%s",
        env_addr ? env_addr : DEFAULT_DAEMON_ADDR);
    s_normalize_addr(daemon_addr, sizeof(daemon_addr));

    const char *cmd_name = NULL;
    const char *cmd_arg = NULL;
    int cmd_pos = -1;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--version") == 0)
        {
            printf("sparkctl, from sparkd v%s\n", SPARKD_VERSION);
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            usage();
            return 0;
        }
        else if (strcmp(argv[i], "--http") == 0 && i + 1 < argc)
        {
            snprintf(daemon_addr, sizeof(daemon_addr), "%s", argv[++i]);
        }
        else if (!cmd_name)
        {
            cmd_name = argv[i];
            cmd_pos = i;
            break;
        }
    }

    if (!cmd_name)
    {
        usage();
        return 1;
    }

    /* Service subcommands */
    if (strcmp(cmd_name, "daemon") == 0)
    {
        return s_handle_service_subcommand(
            "sparkd", "sparkd", daemon_addr, "SPARK_HTTP_ADDR",
            argc - cmd_pos - 1, &argv[cmd_pos + 1]);
    }

    if (strcmp(cmd_name, "ui") == 0)
    {
        return s_handle_service_subcommand(
            "spark-ui", "spark-ui", DEFAULT_UI_ADDR, "SPARK_UI_HTTP_ADDR",
            argc - cmd_pos - 1, &argv[cmd_pos + 1]);
    }

    /* Flat engine commands */
    const flat_command_t *cmd = NULL;
    for (size_t i = 0; i < FLAT_COMMAND_COUNT; i++)
    {
        if (strcmp(s_flat_commands[i].name, cmd_name) == 0)
        {
            cmd = &s_flat_commands[i];
            break;
        }
    }

    if (!cmd)
    {
        fprintf(stderr, "sparkctl: unknown command '%s'\n", cmd_name);
        return 1;
    }

    /* Collect optional argument (e.g., reload PATH) */
    for (int i = cmd_pos + 1; i < argc; i++)
    {
        if (argv[i][0] != '-' && !cmd_arg)
            cmd_arg = argv[i];
    }

    char req_body[1024] = "";
    if (strcmp(cmd_name, "set-blackout") == 0)
        snprintf(req_body, sizeof(req_body), "{\"enabled\":true}");
    else if (strcmp(cmd_name, "clear-blackout") == 0)
        snprintf(req_body, sizeof(req_body), "{\"enabled\":false}");
    else if (cmd_arg)
        snprintf(req_body, sizeof(req_body), "{\"path\":\"%s\"}", cmd_arg);

    request_ctx_t resp = {0};
    int status = s_http_request(daemon_addr, cmd->method, cmd->path,
                                req_body[0] ? req_body : NULL, &resp);

    if (status < 0)
    {
        fprintf(stderr, "sparkctl: cannot connect to daemon (%s)\n", daemon_addr);
        return 1;
    }

    if (strcmp(cmd_name, "status") == 0)
        s_print_engine_status(resp.body);
    else
        printf("%s", resp.body);

    return (status >= 200 && status < 300) ? 0 : 1;
}
