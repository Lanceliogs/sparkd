/*
 * http.h - Embedded HTTP server for sparkd
 *
 * Provides a lightweight REST API for controlling and inspecting the
 * engine at runtime. Built on mongoose (single-file embedded HTTP
 * server, vendored).
 *
 * Endpoints:
 *   GET  /healthz                  - version, pid, uptime
 *   GET  /api/engine/state         - engine running state and config
 *   POST /api/engine/start         - start engine with JSON config
 *   POST /api/engine/stop          - stop engine
 *   POST /api/engine/midi/reconnect - trigger MIDI reconnect cycle
 *
 * The server is non-blocking: process_events() wraps mg_mgr_poll()
 * and should be called from the main loop each tick. The timeout
 * parameter doubles as the loop sleep, replacing spark_clock_msleep.
 */
#ifndef SPARK_HTTP_H
#define SPARK_HTTP_H

int  spark_http_init(const char *listen_addr);
void spark_http_process_events(int timeout_ms);
void spark_http_broadcast_scene_events(void);
void spark_http_destroy(void);

#endif
