/*
 * engine.h - Core application engine (singleton)
 *
 * Owns and orchestrates the full sparkd runtime: MIDI input, stage
 * (scene state + rendering), and DMX output thread. Designed as a
 * singleton with static internal state -- one engine per process.
 *
 * Lifecycle:
 *   init         - initialize PortMidi. Call once.
 *   load_project - parse project, populate config. Requires stopped state.
 *   start        - start MIDI + DMX using config from loaded project.
 *   stop         - tear down DMX thread, close MIDI, destroy stage.
 *   destroy      - final cleanup, terminate PortMidi. Call once.
 */
#ifndef SPARK_ENGINE_H
#define SPARK_ENGINE_H

#include "project.h"

#include <stdbool.h>

int  spark_engine_init(void);
int  spark_engine_load_project(const char *path);
int  spark_engine_start(void);
void spark_engine_stop(void);
void spark_engine_destroy(void);
void spark_engine_process_events(void);
int  spark_engine_midi_reconnect(void);

bool spark_engine_is_running(void);
bool spark_engine_get_blackout(void);
void spark_engine_set_blackout(bool enabled);
const spark_project_config_t *spark_engine_get_config(void);
const char *spark_engine_get_project_path(void);

/* Forward declare to avoid pulling in dmx.h here */
struct spark_dmx_backend;
const struct spark_dmx_backend *spark_engine_get_dmx_backend(void);

#endif
