/*
 * project.h - Project loading and configuration
 *
 * The project defines the complete fixture patch, scene mapping, and
 * runtime configuration (MIDI mode, DMX backend).
 *
 * spark_project_load() resets all fixture/scene state, parses the project
 * file, populates the config struct, and resolves scenes.
 *
 * The config struct is owned by the engine module. The loader writes into
 * it via the config_out pointer.
 */
#ifndef SPARK_PROJECT_H
#define SPARK_PROJECT_H

#include "consts.h"
#include "midi.h"
#include "dmx/dmx.h"

typedef struct {
    spark_midi_mode_t midi_mode;
    char midi_device[SPARK_MIDI_PORT_STRLEN];
    spark_dmx_backend_type_t dmx_backend;
    char dmx_device[SPARK_SERIAL_PORT_STRLEN];
} spark_project_config_t;

int spark_project_load(const char *path, spark_project_config_t *config_out);

#endif
