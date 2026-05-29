#include "mongoose.h"
#include "mg_helpers.h"
#include "editor_http.h"
#include "env.h"
#include "log.h"

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <libgen.h>
#endif

#define DEFAULT_HTTP_ADDR "127.0.0.1:7601"
#define DEFAULT_DAEMON_ADDR "127.0.0.1:7600"

static volatile bool s_running = true;
static char s_daemon_addr[256] = DEFAULT_DAEMON_ADDR;
static char s_ui_root[1024] = {0};

/* Get directory containing the running executable */
static int s_get_exe_dir(char *buf, size_t buf_size)
{
#ifdef _WIN32
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return -1;
    /* Strip filename, keep directory */
    char *sep = strrchr(path, '\\');
    if (!sep) sep = strrchr(path, '/');
    if (sep) *sep = '\0';
    snprintf(buf, buf_size, "%s", path);
#else
    char path[1024];
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len <= 0) return -1;
    path[len] = '\0';
    snprintf(buf, buf_size, "%s", dirname(path));
#endif
    return 0;
}

/* Check if a directory contains index.html (valid UI root) */
static int s_is_ui_root(const char *dir)
{
    char path[1100];
    snprintf(path, sizeof(path), "%s/index.html", dir);
    FILE *f = fopen(path, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

/*
 * Resolve UI root path. Priority:
 *   1. --ui-root (already set in s_ui_root before calling this)
 *   2. SPARK_UI_ROOT env var
 *   3. <exe-dir>/ui/
 *   4. <exe-dir>/../share/sparkd/ui/
 *   5. ui/dist (CWD fallback for development)
 */
static int s_resolve_ui_root(void)
{
    /* Already set by --ui-root */
    if (s_ui_root[0] != '\0' && s_is_ui_root(s_ui_root))
        return 0;

    /* Env var */
    const char *env = spark_env_get("SPARK_UI_ROOT");
    if (env && env[0] != '\0')
    {
        snprintf(s_ui_root, sizeof(s_ui_root), "%s", env);
        if (s_is_ui_root(s_ui_root)) return 0;
    }

    /* Relative to executable */
    char exe_dir[1024];
    if (s_get_exe_dir(exe_dir, sizeof(exe_dir)) == 0)
    {
        snprintf(s_ui_root, sizeof(s_ui_root), "%s/ui", exe_dir);
        if (s_is_ui_root(s_ui_root)) return 0;

        snprintf(s_ui_root, sizeof(s_ui_root), "%s/../share/sparkd/ui", exe_dir);
        if (s_is_ui_root(s_ui_root)) return 0;
    }

    /* CWD fallback (development) */
    snprintf(s_ui_root, sizeof(s_ui_root), "ui/dist");
    if (s_is_ui_root(s_ui_root)) return 0;

    return -1;
}

static void s_signal_handler(int sig)
{
    (void)sig;
    s_running = false;
}

/* ---- Static file serving ---- */

static struct mg_http_serve_opts s_serve_opts;

static void s_serve_static(struct mg_connection *c, struct mg_http_message *hm)
{
    s_serve_opts.root_dir = s_ui_root;
    s_serve_opts.ssi_pattern = NULL;
    s_serve_opts.extra_headers = "Cache-Control: no-cache\r\n";
    mg_http_serve_dir(c, hm, &s_serve_opts);
}

/* ---- HTTP proxy to sparkd ---- */

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

/* ---- WebSocket proxy ---- */

typedef struct {
    unsigned long peer_id;
    struct mg_mgr *mgr;
} ws_proxy_ctx_t;

static void s_ws_proxy_ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    ws_proxy_ctx_t *ctx = (ws_proxy_ctx_t *)c->fn_data;
    if (!ctx) return;

    if (ev == MG_EV_WS_OPEN)
    {
        (void)ev_data;
    }
    else if (ev == MG_EV_WS_MSG)
    {
        struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
        struct mg_connection *peer = s_find_connection(ctx->mgr, ctx->peer_id);
        if (peer)
            mg_ws_send(peer, wm->data.buf, wm->data.len, WEBSOCKET_OP_TEXT);
    }
    else if (ev == MG_EV_CLOSE)
    {
        struct mg_connection *peer = s_find_connection(ctx->mgr, ctx->peer_id);
        if (peer && peer->is_websocket)
            peer->is_closing = 1;
        free(ctx);
        c->fn_data = NULL;
    }
}

static void s_ws_proxy_connect(struct mg_mgr *mgr, struct mg_connection *client)
{
    char url[512];
    snprintf(url, sizeof(url), "ws%s/ws", s_daemon_addr + 4); /* http -> ws */

    ws_proxy_ctx_t *be_ctx = calloc(1, sizeof(ws_proxy_ctx_t));
    be_ctx->peer_id = client->id;
    be_ctx->mgr = mgr;

    struct mg_connection *be = mg_ws_connect(mgr, url, s_ws_proxy_ev_handler, be_ctx, NULL);
    if (!be)
    {
        free(be_ctx);
        client->is_closing = 1;
        return;
    }

    ws_proxy_ctx_t *cl_ctx = calloc(1, sizeof(ws_proxy_ctx_t));
    cl_ctx->peer_id = be->id;
    cl_ctx->mgr = mgr;
    client->fn_data = cl_ctx;
}

/* ---- Main event handler ---- */

static void s_ev_handler(struct mg_connection *c, int ev, void *ev_data)
{
    if (ev == MG_EV_WS_MSG)
    {
        ws_proxy_ctx_t *ctx = (ws_proxy_ctx_t *)c->fn_data;
        if (ctx)
        {
            struct mg_ws_message *wm = (struct mg_ws_message *)ev_data;
            struct mg_connection *peer = s_find_connection(ctx->mgr, ctx->peer_id);
            if (peer)
                mg_ws_send(peer, wm->data.buf, wm->data.len, WEBSOCKET_OP_TEXT);
        }
        return;
    }

    if (ev == MG_EV_CLOSE && c->is_websocket)
    {
        ws_proxy_ctx_t *ctx = (ws_proxy_ctx_t *)c->fn_data;
        if (ctx)
        {
            struct mg_connection *peer = s_find_connection(ctx->mgr, ctx->peer_id);
            if (peer)
                peer->is_closing = 1;
            free(ctx);
            c->fn_data = NULL;
        }
        return;
    }

    if (ev != MG_EV_HTTP_MSG) return;

    struct mg_http_message *hm = (struct mg_http_message *)ev_data;
    struct mg_mgr *mgr = (struct mg_mgr *)c->fn_data;

    if (mg_match(hm->uri, mg_str("/ws"), NULL))
    {
        mg_ws_upgrade(c, hm, NULL);
        s_ws_proxy_connect(mgr, c);
        return;
    }

    /* CORS preflight for editor API */
    if (mg_method_is(hm, "OPTIONS") &&
        mg_match(hm->uri, mg_str("/api/editor/#"), NULL))
    {
        mg_cors_preflight(c);
        return;
    }

    if (mg_match(hm->uri, mg_str("/api/editor/#"), NULL))
    {
        if (editor_http_handle(c, hm))
            return;
    }

    if (mg_match(hm->uri, mg_str("/api/#"), NULL))
    {
        s_proxy_request(mgr, c, hm);
        return;
    }

    s_serve_static(c, hm);
}

/* ---- CLI ---- */

static void usage(void)
{
    fprintf(stderr, "spark-ui - web UI server for sparkd\n\n");
    fprintf(stderr, "Usage: spark-ui [OPTIONS]\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --http ADDR      Listen address (default: %s)\n", DEFAULT_HTTP_ADDR);
    fprintf(stderr, "  --daemon ADDR    sparkd address (default: %s)\n", DEFAULT_DAEMON_ADDR);
    fprintf(stderr, "  --ui-root PATH   Override UI assets directory (auto-detected)\n");
    fprintf(stderr, "  --open-browser   Open browser on startup\n");
    fprintf(stderr, "  --help           Show this help\n");
    fprintf(stderr, "\nUI root auto-detection order:\n");
    fprintf(stderr, "  1. --ui-root / SPARK_UI_ROOT env\n");
    fprintf(stderr, "  2. <exe-dir>/ui/\n");
    fprintf(stderr, "  3. <exe-dir>/../share/sparkd/ui/\n");
    fprintf(stderr, "  4. ./ui/dist (development fallback)\n");
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
    spark_env_load();

    char http_addr[256];
    snprintf(http_addr, sizeof(http_addr), "%s", DEFAULT_HTTP_ADDR);
    bool open_browser = false;

    const char *env;
    if ((env = spark_env_get("SPARK_UI_HTTP_ADDR")) != NULL)
        snprintf(http_addr, sizeof(http_addr), "%s", env);
    if ((env = spark_env_get("SPARK_UI_DAEMON_ADDR")) != NULL)
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

    if (s_resolve_ui_root() != 0)
    {
        fprintf(stderr, "spark-ui: cannot find UI assets (index.html)\n");
        fprintf(stderr, "  Searched: <exe>/ui/, <exe>/../share/sparkd/ui/, ./ui/dist\n");
        fprintf(stderr, "  Use --ui-root PATH or set SPARK_UI_ROOT\n");
        return 1;
    }

    editor_http_init(spark_env_get("SPARK_FIXTURE_BANK_PATH"));

    signal(SIGINT, s_signal_handler);
    signal(SIGTERM, s_signal_handler);

    char listen_url[270];
    snprintf(listen_url, sizeof(listen_url), "http://%s", http_addr);

    char daemon_full[270];
    snprintf(daemon_full, sizeof(daemon_full), "http://%s", s_daemon_addr);
    strncpy(s_daemon_addr, daemon_full, sizeof(s_daemon_addr) - 1);

    struct mg_mgr mgr;
    mg_mgr_init(&mgr);
    mg_log_set(0);

    struct mg_connection *c = mg_http_listen(&mgr, listen_url, s_ev_handler, &mgr);
    if (!c)
    {
        fprintf(stderr, "spark-ui: failed to listen on %s\n", http_addr);
        return 1;
    }

    printf("spark-ui: serving %s on %s\n", s_ui_root, http_addr);
    printf("spark-ui: proxying /api/* to %s\n", s_daemon_addr);

    if (open_browser)
        s_open_browser(listen_url);

    while (s_running)
        mg_mgr_poll(&mgr, 100);

    mg_mgr_free(&mgr);
    printf("spark-ui: stopped\n");
    return 0;
}
