/*
 * engine.h - Core application engine (singleton)
 *
 * Owns and orchestrates the full sparkd runtime: MIDI input, stage
 * (scene state + rendering), and DMX output thread. Designed as a
 * singleton with static internal state — one engine per process.
 *
 * Lifecycle:
 *   init    - zero-init all state, initialize PortMidi. Call once.
 *   start   - configure MIDI, stage, and DMX from a config struct,
 *             then launch the DMX output thread. Idempotent if
 *             already running.
 *   stop    - tear down DMX thread, close MIDI, destroy stage.
 *             Can be restarted with a new config via start().
 *   destroy - final cleanup, terminate PortMidi. Call once.
 *
 * The main loop calls process_events() each tick to poll MIDI and
 * dispatch events to the stage. The DMX output thread runs
 * independently, reading rendered frames from the stage.
 *
 * MIDI failure is non-fatal: the engine starts without MIDI and
 * midi_reconnect() can be triggered externally (e.g. via HTTP).
 */
#ifndef SPARK_ENGINE_H
#define SPARK_ENGINE_H

#include "consts.h"
#include "dmx/dmx.h"

#include <stdbool.h>

typedef struct {
    char dmx_port[SPARK_SERIAL_PORT_STRLEN];
    spark_dmx_backend_type_t dmx_backend_type;
    char midi_device[SPARK_MIDI_PORT_STRLEN];
} spark_engine_config_t;

int  spark_engine_init(void);
int  spark_engine_load_project(const char *path);
int  spark_engine_start(const spark_engine_config_t *cfg);
void spark_engine_stop(void);
void spark_engine_destroy(void);
void spark_engine_process_events(void);
int  spark_engine_midi_reconnect(void);

bool spark_engine_is_running(void);
const spark_engine_config_t *spark_engine_get_config(void);
const spark_engine_config_t *spark_engine_get_last_config(void);
const char *spark_engine_get_project_path(void);

#endif
