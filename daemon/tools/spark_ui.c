#include "mongoose.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_HTTP_ADDR "http://127.0.0.1:7601"
#define DEFAULT_DAEMON_ADDR "http://127.0.0.1:7600"
#define DEFAULT_UI_ROOT "ui/dist"

static volatile bool s_running = true;
static char s_daemon_addr[256] = DEFAULT_DAEMON_ADDR;
static char s_ui_root[1024] = DEFAULT_UI_ROOT;

static void s_signal_handler(int sig)
{
    (void)sig;
    s_running = false;
}

static struct mg_http_serve_opts s_serve_opts;

static void s_serve_static(struct mg_connection *c, struct mg_http_message *hm)
{
    s_serve_opts.root_dir = s_ui_root;
    s_serve_opts.ssi_pattern = NULL;
    s_serve_opts.extra_headers = "Cache-Control: no-cache\r\n";
    mg_http_serve_dir(c, hm, &s_serve_opts);
}

typedef struct {
    unsigned long client_id;
    struct mg_mgr *mgr;
} proxy_ctx_t;

static struct mg_connection *s_find_connection(struct mg_mgr *mgr, unsigned long id)
{
    for (struct mg_connection *c = mgr->conns; c; c = c->next)
        if (c->id == id) return c;
    return NULL;
}

static void s_proxy_ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    proxy_ctx_t *ctx = (proxy_ctx_t *)c->fn_data;
    if (!ctx) return;

    if (ev == MG_EV_HTTP_MSG)
    {
        struct mg_http_message *hm = (struct mg_http_message *)ev_data;
        struct mg_connection *client = s_find_connection(ctx->mgr, ctx->client_id);
        if (client)
        {
            mg_http_reply(client, mg_http_status(hm),
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Type: application/json\r\n",
                "%.*s", (int)hm->body.len, hm->body.buf);
        }
        c->is_closing = 1;
        free(ctx);
        c->fn_data = NULL;
    }
    else if (ev == MG_EV_ERROR)
    {
        struct mg_connection *client = s_find_connection(ctx->mgr, ctx->client_id);
        if (client)
            mg_http_reply(client, 502, "", "{\"error\":\"daemon unreachable\"}\n");
        c->is_closing = 1;
        free(ctx);
        c->fn_data = NULL;
    }
    else if (ev == MG_EV_CLOSE)
    {
        if (c->fn_data) free(c->fn_data);
        c->fn_data = NULL;
    }
}

static void s_proxy_request(struct mg_mgr *mgr, struct mg_connection *client,
                            struct mg_http_message *hm)
{
    char url[512];
    snprintf(url, sizeof(url), "%s%.*s", s_daemon_addr, (int)hm->uri.len, hm->uri.buf);

    proxy_ctx_t *ctx = calloc(1, sizeof(proxy_ctx_t));
    ctx->client_id = client->id;
    ctx->mgr = mgr;

    struct mg_connection *pc = mg_http_connect(mgr, url, s_proxy_ev_handler, ctx);
    if (!pc)
    {
        mg_http_reply(client, 502, "", "{\"error\":\"cannot connect to daemon\"}\n");
        free(ctx);
        return;
    }

    struct mg_str host = mg_url_host(url);
    if (hm->body.len > 0)
    {
        mg_printf(pc,
            "%.*s %.*s HTTP/1.0\r\n"
            "Host: %.*s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %lu\r\n"
            "Connection: close\r\n"
            "\r\n",
            (int)hm->method.len, hm->method.buf,
            (int)hm->uri.len, hm->uri.buf,
            (int)host.len, host.buf,
            (unsigned long)hm->body.len);
        mg_send(pc, hm->body.buf, hm->body.len);
    }
    else
    {
        mg_printf(pc,
            "%.*s %.*s HTTP/1.0\r\n"
            "Host: %.*s\r\n"
            "Connection: close\r\n"
            "\r\n",
            (int)hm->method.len, hm->method.buf,
            (int)hm->uri.len, hm->uri.buf,
            (int)host.len, host.buf);
    }
}

static void s_ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev != MG_EV_HTTP_MSG) return;

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    struct mg_mgr *mgr = (struct mg_mgr *)c->fn_data;

    if (mg_match(hm->uri, mg_str("/api/#"), NULL))
    {
        s_proxy_request(mgr, c, hm);
        return;
    }

    s_serve_static(c, hm);
}

static void usage(void)
{
    fprintf(stderr, "spark-ui - web UI server for sparkd\n\n");
    fprintf(stderr, "Usage: spark-ui [OPTIONS]\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --http ADDR      Listen address (default: %s)\n", DEFAULT_HTTP_ADDR);
    fprintf(stderr, "  --daemon ADDR    sparkd address (default: %s)\n", DEFAULT_DAEMON_ADDR);
    fprintf(stderr, "  --ui-root PATH   Built UI directory (default: %s)\n", DEFAULT_UI_ROOT);
    fprintf(stderr, "  --open-browser   Open browser on startup\n");
    fprintf(stderr, "  --help           Show this help\n");
}

static void s_open_browser(const char *addr)
{
#ifdef _WIN32
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "start %s", addr);
    system(cmd);
#elif __APPLE__
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "open %s", addr);
    system(cmd);
#else
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "xdg-open %s >/dev/null 2>&1 &", addr);
    system(cmd);
#endif
}

int main(int argc, char **argv)
{
    char http_addr[256];
    snprintf(http_addr, sizeof(http_addr), "%s", DEFAULT_HTTP_ADDR);
    bool open_browser = false;

    const char *env;
    if ((env = getenv("SPARK_UI_HTTP_ADDR")) != NULL)
        snprintf(http_addr, sizeof(http_addr), "%s", env);
    if ((env = getenv("SPARK_UI_DAEMON_ADDR")) != NULL)
        snprintf(s_daemon_addr, sizeof(s_daemon_addr), "%s", env);

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            usage();
            return 0;
        }
        else if (strcmp(argv[i], "--http") == 0 && i + 1 < argc)
            snprintf(http_addr, sizeof(http_addr), "%s", argv[++i]);
        else if (strcmp(argv[i], "--daemon") == 0 && i + 1 < argc)
            snprintf(s_daemon_addr, sizeof(s_daemon_addr), "%s", argv[++i]);
        else if (strcmp(argv[i], "--ui-root") == 0 && i + 1 < argc)
            snprintf(s_ui_root, sizeof(s_ui_root), "%s", argv[++i]);
        else if (strcmp(argv[i], "--open-browser") == 0)
            open_browser = true;
        else
        {
            fprintf(stderr, "spark-ui: unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    signal(SIGINT, s_signal_handler);
    signal(SIGTERM, s_signal_handler);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_log_set(0);

    struct mg_connection *c = mg_http_listen(&mgr, http_addr, s_ev_handler, &mgr);
    if (!c)
    {
        fprintf(stderr, "spark-ui: failed to listen on %s\n", http_addr);
        return 1;
    }

    printf("spark-ui: serving %s on %s\n", s_ui_root, http_addr);
    printf("spark-ui: proxying /api/* to %s\n", s_daemon_addr);

    if (open_browser)
        s_open_browser(http_addr);

    while (s_running)
        mg_mgr_poll(&mgr, 100);

    mg_mgr_free(&mgr);
    printf("spark-ui: stopped\n");
    return 0;
}
