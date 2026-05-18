#ifndef SPARK_ENGINE_H
#define SPARK_ENGINE_H

#include "consts.h"
#include "dmx/dmx.h"

typedef struct {
    char dmx_port[SPARK_SERIAL_PORT_STRLEN];
    spark_dmx_backend_type_t dmx_backend_type;
    char midi_device[SPARK_MIDI_PORT_STRLEN];
} spark_engine_config_t;

int  spark_engine_init(void);
int  spark_engine_start(const spark_engine_config_t *cfg);
void spark_engine_stop(void);
void spark_engine_destroy(void);
void spark_engine_process_events(void);
int  spark_engine_midi_reconnect(void);

#endif
