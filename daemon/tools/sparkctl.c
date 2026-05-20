#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_ADDR "http://127.0.0.1:7600"

typedef struct {
    const char *method;
    const char *path;
} command_t;

static const struct { const char *name; command_t cmd; } s_commands[] = {
    { "status",         { "GET",  "/api/engine/state" } },
    { "start",          { "POST", "/api/engine/start" } },
    { "stop",           { "POST", "/api/engine/stop" } },
    { "reconnect-midi", { "POST", "/api/engine/midi/reconnect" } },
    { "healthz",        { "GET",  "/healthz" } },
};

#define COMMAND_COUNT (sizeof(s_commands) / sizeof(s_commands[0]))

static void usage(void)
{
    fprintf(stderr, "sparkctl - control a running sparkd instance\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: sparkctl [--http ADDR] <command>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Commands:\n");
    fprintf(stderr, "  status           Get engine state\n");
    fprintf(stderr, "  start            Start the engine\n");
    fprintf(stderr, "  stop             Stop the engine\n");
    fprintf(stderr, "  reconnect-midi   Reconnect MIDI device\n");
    fprintf(stderr, "  healthz          Health check\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --http ADDR      Daemon address (default: %s)\n", DEFAULT_ADDR);
    fprintf(stderr, "\n");
    fprintf(stderr, "Environment:\n");
    fprintf(stderr, "  SPARK_HTTP_ADDR  Override default address\n");
}

typedef struct {
    const char *method;
    const char *path;
    const char *url;
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
        mg_printf(c,
            "%s %s HTTP/1.0\r\n"
            "Host: %.*s\r\n"
            "Connection: close\r\n"
            "\r\n",
            ctx->method, ctx->path,
            (int)host.len, host.buf);
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
        fprintf(stderr, "sparkctl: cannot connect to daemon (%s)\n", (char *)ev_data);
        ctx->status = -1;
        ctx->done = true;
    }
}

int main(int argc, char **argv)
{
    char addr[256];
    const char *env_addr = getenv("SPARK_HTTP_ADDR");
    snprintf(addr, sizeof(addr), "%s", env_addr ? env_addr : DEFAULT_ADDR);

    const char *cmd_name = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            usage();
            return 0;
        }
        else if (strcmp(argv[i], "--http") == 0 && i + 1 < argc)
            snprintf(addr, sizeof(addr), "%s", argv[++i]);
        else if (argv[i][0] != '-')
            cmd_name = argv[i];
        else
        {
            fprintf(stderr, "sparkctl: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!cmd_name)
    {
        usage();
        return 1;
    }

    const command_t *cmd = NULL;
    for (size_t i = 0; i < COMMAND_COUNT; i++)
    {
        if (strcmp(s_commands[i].name, cmd_name) == 0)
        {
            cmd = &s_commands[i].cmd;
            break;
        }
    }

    if (!cmd)
    {
        fprintf(stderr, "sparkctl: unknown command '%s'\n", cmd_name);
        return 1;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s%s", addr, cmd->path);

    mg_log_set(0);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    request_ctx_t ctx = {
        .method = cmd->method,
        .path = cmd->path,
        .url = url,
    };

    struct mg_connection *c = mg_http_connect(&mgr, url, s_ev_handler, &ctx);
    if (!c)
    {
        fprintf(stderr, "sparkctl: cannot connect to %s\n", addr);
        mg_mgr_free(&mgr);
        return 1;
    }

    while (!ctx.done)
        mg_mgr_poll(&mgr, 50);

    mg_mgr_free(&mgr);

    if (ctx.status < 0)
        return 1;

    printf("%s", ctx.body);
    return (ctx.status >= 200 && ctx.status < 300) ? 0 : 1;
}
