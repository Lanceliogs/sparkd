#ifndef SPARK_MIDI_H
#define SPARK_MIDI_H

#include "consts.h"
#include "midi_event.h"

#include <stdint.h>

typedef struct {
    int id;
    char name[SPARK_MIDI_PORT_STRLEN];
} spark_midi_device_t;

int spark_midi_init(void);
void spark_midi_destroy(void);

int spark_midi_list_devices(spark_midi_device_t *out, int max);
int spark_midi_find_device(const char *pattern);

int spark_midi_open(int device_id);
int spark_midi_open_by_name(const char *pattern);
int spark_midi_create_virtual(const char *name);

void spark_midi_close(int device_id);
void spark_midi_close_all(void);

int spark_midi_poll(spark_midi_event_t *out, int max);

void spark_midi_set_heartbeat(const char *pattern, uint32_t timeout_ms);
void spark_midi_disable_heartbeat(const char *pattern);
int spark_midi_check_heartbeat(void);
int spark_midi_reconnect(void);

#endif