#include "mongoose.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_ADDR "http://127.0.0.1:7600"

typedef struct {
    const char *method;
    const char *path;
    bool needs_arg;
} command_t;

static const struct { const char *name; command_t cmd; } s_commands[] = {
    { "status",         { "GET",  "/api/engine/state",          false } },
    { "start",          { "POST", "/api/engine/start",          false } },
    { "stop",           { "POST", "/api/engine/stop",           false } },
    { "reconnect-midi", { "POST", "/api/engine/midi/reconnect", false } },
    { "reload",         { "POST", "/api/project/reload",        false } },
    { "healthz",        { "GET",  "/healthz",                   false } },
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
    fprintf(stderr, "  reload [PATH]    Reload project (engine must be stopped)\n");
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
        if (ctx->req_body)
        {
            size_t blen = strlen(ctx->req_body);
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
    const char *cmd_arg = NULL;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            usage();
            return 0;
        }
        else if (strcmp(argv[i], "--http") == 0 && i + 1 < argc)
            snprintf(addr, sizeof(addr), "%s", argv[++i]);
        else if (argv[i][0] != '-' && !cmd_name)
            cmd_name = argv[i];
        else if (argv[i][0] != '-' && cmd_name && !cmd_arg)
            cmd_arg = argv[i];
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

    if (cmd->needs_arg && !cmd_arg)
    {
        fprintf(stderr, "sparkctl: '%s' requires an argument\n", cmd_name);
        return 1;
    }

    char url[512];
    snprintf(url, sizeof(url), "%s%s", addr, cmd->path);

    char req_body[1024] = "";
    if (cmd_arg)
        snprintf(req_body, sizeof(req_body), "{\"path\":\"%s\"}", cmd_arg);

    mg_log_set(0);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);

    request_ctx_t ctx = {
        .method = cmd->method,
        .path = cmd->path,
        .url = url,
        .req_body = req_body[0] ? req_body : NULL,
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
