#include "mongoose.h"
#include "mg_helpers.h"
#include "editor_http.h"
#include "env.h"
#include "fs.h"
#include "log.h"
#include "clock.h"
#include "consts.h"
#include "auth.h"


#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <unistd.h>
#include <libgen.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#define DEFAULT_HTTP_ADDR "127.0.0.1:7601"
#define DEFAULT_DAEMON_ADDR "127.0.0.1:7600"
#define SPARK_UI_VERSION SPARKD_VERSION

static volatile bool s_running = true;
static uint64_t s_start_time_ms = 0;
static char s_listen_port[16] = "7601";
static char s_daemon_addr[256] = DEFAULT_DAEMON_ADDR;
static char s_ui_root[1088] = {0};

/* Check if a directory contains index.html (valid UI root) */
static int s_is_ui_root(const char *dir)
{
    char path[1100];
    spark_fs_path_join(path, sizeof(path), dir, "index.html");
    return spark_fs_file_exists(path);
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
    if (spark_fs_exe_dir(exe_dir, sizeof(exe_dir)) == 0)
    {
        snprintf(s_ui_root, sizeof(s_ui_root), "%s/ui", exe_dir);
        if (s_is_ui_root(s_ui_root)) return 0;

        snprintf(s_ui_root, sizeof(s_ui_root), "%s/../ui", exe_dir);
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
    const char *static_token = spark_auth_enabled() ? spark_auth_get_static_token() : NULL;

    if (hm->body.len > 0)
    {
        mg_printf(pc,
            "%.*s %.*s HTTP/1.0\r\n"
            "Host: %.*s\r\n"
            "%s%s%s"
            "Content-Type: application/json\r\n"
            "Content-Length: %lu\r\n"
            "Connection: close\r\n"
            "\r\n",
            (int)hm->method.len, hm->method.buf,
            (int)hm->uri.len, hm->uri.buf,
            (int)host.len, host.buf,
            static_token ? "Authorization: Bearer " : "",
            static_token ? static_token : "",
            static_token ? "\r\n" : "",
            (unsigned long)hm->body.len);
        mg_send(pc, hm->body.buf, hm->body.len);
    }
    else
    {
        mg_printf(pc,
            "%.*s %.*s HTTP/1.0\r\n"
            "Host: %.*s\r\n"
            "%s%s%s"
            "Connection: close\r\n"
            "\r\n",
            (int)hm->method.len, hm->method.buf,
            (int)hm->uri.len, hm->uri.buf,
            (int)host.len, host.buf,
            static_token ? "Authorization: Bearer " : "",
            static_token ? static_token : "",
            static_token ? "\r\n" : "");
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

    const char *static_token = spark_auth_enabled() ? spark_auth_get_static_token() : NULL;
    char extra_headers[256] = "";
    if (static_token)
        snprintf(extra_headers, sizeof(extra_headers), "Authorization: Bearer %s\r\n", static_token);

    struct mg_connection *be = mg_ws_connect(mgr, url, s_ws_proxy_ev_handler, be_ctx,
        extra_headers[0] ? extra_headers : NULL);
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

/* ---- Auth helpers ---- */

static int s_check_static_token(struct mg_http_message *hm)
{
    if (!spark_auth_enabled()) return 1;
    struct mg_str *auth = mg_http_get_header(hm, "Authorization");
    if (auth && auth->len > 7 && memcmp(auth->buf, "Bearer ", 7) == 0)
        return spark_auth_check_static(auth->buf + 7, auth->len - 7);
    return 0;
}

static spark_role_t s_check_any_token(struct mg_http_message *hm)
{
    struct mg_str *auth = mg_http_get_header(hm, "Authorization");
    if (auth && auth->len > 7 && memcmp(auth->buf, "Bearer ", 7) == 0)
        return spark_auth_check_any(auth->buf + 7, auth->len - 7);
    return SPARK_ROLE_NONE;
}

static int s_route_requires_admin(struct mg_http_message *hm)
{
    if (mg_match(hm->uri, mg_str("/api/editor/#"), NULL))
        return 1;
    if (mg_match(hm->uri, mg_str("/api/project/reload"), NULL) &&
        mg_method_is(hm, "POST") && hm->body.len > 2)
        return 1;
    return 0;
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

    /* --- Layer 1: Management routes (require static SPARK_AUTH_TOKEN) --- */

    if (mg_match(hm->uri, mg_str("/healthz"), NULL))
    {
        if (!s_check_static_token(hm))
        {
            mg_http_reply(c, 401, "Content-Type: application/json\r\n",
                "{%m:%m}\n", MG_ESC("error"), MG_ESC("unauthorized"));
            return;
        }
        uint64_t uptime = spark_clock_monotonic_ms() - s_start_time_ms;
#ifdef _WIN32
        int pid = (int)GetCurrentProcessId();
#else
        int pid = (int)getpid();
#endif
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
            "{%m:%m,%m:%d,%m:%llu}\n",
            MG_ESC("version"), MG_ESC(SPARK_UI_VERSION),
            MG_ESC("pid"), pid,
            MG_ESC("uptime_ms"), (unsigned long long)uptime);
        return;
    }

    if (mg_match(hm->uri, mg_str("/shutdown"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        if (!s_check_static_token(hm))
        {
            mg_http_reply(c, 401, "Content-Type: application/json\r\n",
                "{%m:%m}\n", MG_ESC("error"), MG_ESC("unauthorized"));
            return;
        }
        mg_http_reply(c, 200, "Content-Type: application/json\r\n",
            "{%m:%s}\n", MG_ESC("ok"), "true");
        s_running = false;
        return;
    }

    /* --- Auth endpoints --- */

    if (mg_match(hm->uri, mg_str("/api/auth/info"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        char lan_ip[64] = "127.0.0.1";
        const char *env_iface = spark_env_get("SPARK_LAN_IFACE");

#ifndef _WIN32
        struct ifaddrs *ifaddr, *ifa;
        if (getifaddrs(&ifaddr) == 0)
        {
            if (env_iface && env_iface[0])
            {
                /* Parse CIDR: e.g. "192.168.0.0/24" */
                char net_str[64];
                int prefix = 24;
                snprintf(net_str, sizeof(net_str), "%s", env_iface);
                char *slash = strchr(net_str, '/');
                if (slash) { prefix = atoi(slash + 1); *slash = '\0'; }

                struct in_addr net_addr;
                inet_pton(AF_INET, net_str, &net_addr);
                uint32_t net = ntohl(net_addr.s_addr);
                uint32_t mask = prefix ? (0xFFFFFFFFU << (32 - prefix)) : 0;

                for (ifa = ifaddr; ifa; ifa = ifa->ifa_next)
                {
                    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                        continue;
                    struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                    uint32_t ip = ntohl(sa->sin_addr.s_addr);
                    if ((ip & mask) == (net & mask))
                    {
                        inet_ntop(AF_INET, &sa->sin_addr, lan_ip, sizeof(lan_ip));
                        break;
                    }
                }
            }
            else
            {
                /* Auto-detect: prefer RFC1918 private IPs */
                int best_score = 0;
                for (ifa = ifaddr; ifa; ifa = ifa->ifa_next)
                {
                    if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                        continue;
                    struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                    uint32_t ip = ntohl(sa->sin_addr.s_addr);

                    if ((ip >> 24) == 127) continue;
                    if ((ip >> 16) == 0xA9FE) continue;

                    int score = 0;
                    if ((ip >> 16) == 0xC0A8) score = 3;
                    else if ((ip >> 24) == 10) score = 2;
                    else if ((ip >> 20) == 0xAC1) score = 1;

                    if (score > best_score)
                    {
                        best_score = score;
                        inet_ntop(AF_INET, &sa->sin_addr, lan_ip, sizeof(lan_ip));
                    }
                }
                if (best_score == 0)
                {
                    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next)
                    {
                        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET)
                            continue;
                        struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                        uint32_t ip = ntohl(sa->sin_addr.s_addr);
                        if ((ip >> 24) == 127) continue;
                        if ((ip >> 16) == 0xA9FE) continue;
                        inet_ntop(AF_INET, &sa->sin_addr, lan_ip, sizeof(lan_ip));
                        break;
                    }
                }
            }
            freeifaddrs(ifaddr);
        }
#else
        if (env_iface && env_iface[0])
        {
            /* On Windows, parse CIDR and match against local addresses */
            char net_str[64];
            int prefix = 24;
            snprintf(net_str, sizeof(net_str), "%s", env_iface);
            char *slash = strchr(net_str, '/');
            if (slash) { prefix = atoi(slash + 1); *slash = '\0'; }

            struct in_addr net_addr;
            inet_pton(AF_INET, net_str, &net_addr);
            uint32_t net = ntohl(net_addr.s_addr);
            uint32_t mask = prefix ? (0xFFFFFFFFU << (32 - prefix)) : 0;

            char hostname[256];
            if (gethostname(hostname, sizeof(hostname)) == 0)
            {
                struct addrinfo hints = {0}, *res, *rp;
                hints.ai_family = AF_INET;
                if (getaddrinfo(hostname, NULL, &hints, &res) == 0)
                {
                    for (rp = res; rp; rp = rp->ai_next)
                    {
                        struct sockaddr_in *sa = (struct sockaddr_in *)rp->ai_addr;
                        uint32_t ip = ntohl(sa->sin_addr.s_addr);
                        if ((ip & mask) == (net & mask))
                        {
                            inet_ntop(AF_INET, &sa->sin_addr, lan_ip, sizeof(lan_ip));
                            break;
                        }
                    }
                    freeaddrinfo(res);
                }
            }
        }
#endif
        mg_http_reply(c, 200,
            MG_CORS_HEADERS "Content-Type: application/json\r\n",
            "{%m:%m,%m:%s}\n",
            MG_ESC("lan_ip"), MG_ESC(lan_ip),
            MG_ESC("port"), s_listen_port);
        return;
    }

    if (mg_match(hm->uri, mg_str("/api/auth/role"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        spark_role_t role = s_check_any_token(hm);
        if (role == SPARK_ROLE_NONE)
        {
            mg_http_reply(c, 401,
                MG_CORS_HEADERS "Content-Type: application/json\r\n",
                "{%m:%m}\n", MG_ESC("error"), MG_ESC("unauthorized"));
            return;
        }
        const char *role_str = (role == SPARK_ROLE_ADMIN) ? "admin" : "live";
        mg_http_reply(c, 200,
            MG_CORS_HEADERS "Content-Type: application/json\r\n",
            "{%m:%m}\n", MG_ESC("role"), MG_ESC(role_str));
        return;
    }

    if (mg_match(hm->uri, mg_str("/api/auth/tokens"), NULL) &&
        mg_method_is(hm, "GET"))
    {
        spark_role_t role = s_check_any_token(hm);
        if (role != SPARK_ROLE_ADMIN)
        {
            int code = (role == SPARK_ROLE_NONE) ? 401 : 403;
            const char *msg = (role == SPARK_ROLE_NONE) ? "unauthorized" : "forbidden";
            mg_http_reply(c, code,
                MG_CORS_HEADERS "Content-Type: application/json\r\n",
                "{%m:%m}\n", MG_ESC("error"), MG_ESC(msg));
            return;
        }
        mg_http_reply(c, 200,
            MG_CORS_HEADERS "Content-Type: application/json\r\n",
            "{%m:%m}\n",
            MG_ESC("share_token"), MG_ESC(spark_auth_ui_token()));
        return;
    }

    if (mg_match(hm->uri, mg_str("/api/auth/rotate"), NULL) &&
        mg_method_is(hm, "POST"))
    {
        spark_role_t role = s_check_any_token(hm);
        if (role != SPARK_ROLE_ADMIN)
        {
            int code = (role == SPARK_ROLE_NONE) ? 401 : 403;
            const char *msg = (role == SPARK_ROLE_NONE) ? "unauthorized" : "forbidden";
            mg_http_reply(c, code,
                MG_CORS_HEADERS "Content-Type: application/json\r\n",
                "{%m:%m}\n", MG_ESC("error"), MG_ESC(msg));
            return;
        }

        spark_auth_ui_rotate();

        mg_http_reply(c, 200,
            MG_CORS_HEADERS "Content-Type: application/json\r\n",
            "{%m:%m}\n",
            MG_ESC("share_token"), MG_ESC(spark_auth_ui_token()));
        return;
    }

    /* --- Layer 2: Browser routes (require dynamic UI token) --- */

    if (mg_match(hm->uri, mg_str("/ws"), NULL))
    {
        char token_buf[SPARK_AUTH_TOKEN_MAX_LEN];
        int len = mg_http_get_var(&hm->query, "token", token_buf, sizeof(token_buf));
        if (len <= 0 || spark_auth_check_any(token_buf, (size_t)len) == SPARK_ROLE_NONE)
        {
            mg_http_reply(c, 401,
                MG_CORS_HEADERS "Content-Type: application/json\r\n",
                "{%m:%m}\n", MG_ESC("error"), MG_ESC("unauthorized"));
            return;
        }
        mg_ws_upgrade(c, hm, NULL);
        s_ws_proxy_connect(mgr, c);
        return;
    }

    /* CORS preflight (no auth) */
    if (mg_method_is(hm, "OPTIONS") &&
        mg_match(hm->uri, mg_str("/api/#"), NULL))
    {
        mg_cors_preflight(c);
        return;
    }

    if (mg_match(hm->uri, mg_str("/api/#"), NULL))
    {
        spark_role_t role = s_check_any_token(hm);
        if (role == SPARK_ROLE_NONE)
        {
            mg_http_reply(c, 401,
                MG_CORS_HEADERS "Content-Type: application/json\r\n",
                "{%m:%m}\n", MG_ESC("error"), MG_ESC("unauthorized"));
            return;
        }
        if (s_route_requires_admin(hm) && role != SPARK_ROLE_ADMIN)
        {
            mg_http_reply(c, 403,
                MG_CORS_HEADERS "Content-Type: application/json\r\n",
                "{%m:%m}\n", MG_ESC("error"), MG_ESC("forbidden"));
            return;
        }

        if (mg_match(hm->uri, mg_str("/api/editor/#"), NULL))
        {
            if (editor_http_handle(c, hm))
                return;
        }

        s_proxy_request(mgr, c, hm);
        return;
    }

    /* Localhost meta injection for index.html (only when auth is configured) */
    if (spark_auth_enabled() &&
        !c->rem.is_ip6 && c->rem.addr.ip4 == mg_htonl(0x7F000001) &&
        (mg_match(hm->uri, mg_str("/"), NULL) ||
         mg_match(hm->uri, mg_str("/index.html"), NULL)))
    {
        const char *src_html = NULL;
        size_t src_len = 0;

        {
            char path[1100];
            snprintf(path, sizeof(path), "%s/index.html", s_ui_root);
            FILE *f = fopen(path, "r");
            if (f)
            {
                fseek(f, 0, SEEK_END);
                src_len = (size_t)ftell(f);
                fseek(f, 0, SEEK_SET);
                char *buf = malloc(src_len + 1);
                if (buf)
                {
                    src_len = fread(buf, 1, src_len, f);
                    buf[src_len] = '\0';
                    src_html = buf;
                }
                fclose(f);
            }
        }

        if (src_html)
        {
            char *html = malloc(src_len + 512);
            if (html)
            {
                memcpy(html, src_html, src_len);
                html[src_len] = '\0';

                free((void *)src_html);

                char meta[256];
                snprintf(meta, sizeof(meta),
                    "<meta name=\"spark-token\" content=\"%s\">\n"
                    "  <meta name=\"spark-role\" content=\"admin\">\n  ",
                    spark_auth_get_static_token());

                char *head = strstr(html, "<head>");
                if (head)
                {
                    char *insert = head + 6;
                    size_t meta_len = strlen(meta);
                    size_t tail_len = strlen(insert);
                    memmove(insert + meta_len, insert, tail_len + 1);
                    memcpy(insert, meta, meta_len);
                }

                mg_http_reply(c, 200,
                    "Content-Type: text/html\r\n"
                    "Cache-Control: no-cache\r\n",
                    "%s", html);
                free(html);
                return;
            }
            free((void *)src_html);
        }
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
    spark_auth_init();
    spark_auth_ui_init();

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
        if (strcmp(argv[i], "--version") == 0)
        {
            printf("spark-ui, from sparkd v%s\n", SPARKD_VERSION);
            return 0;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
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

    s_start_time_ms = spark_clock_monotonic_ms();
    const char *colon = strrchr(http_addr, ':');
    if (colon) snprintf(s_listen_port, sizeof(s_listen_port), "%s", colon + 1);

    printf("spark-ui: serving %s on %s\n", s_ui_root, http_addr);
    printf("spark-ui: proxying /api/* to %s\n", s_daemon_addr);
    printf("spark-ui: Share token: %s\n", spark_auth_ui_token());
    fflush(stdout);

    if (open_browser)
        s_open_browser(listen_url);

    while (s_running)
        mg_mgr_poll(&mgr, 100);

    mg_mgr_poll(&mgr, 10);
    mg_mgr_free(&mgr);
    printf("spark-ui: stopped\n");
    return 0;
}
