#ifndef SPARK_MIDI_H
#define SPARK_MIDI_H

#include "consts.h"
#include "midi_event.h"

#include <stdint.h>

/* Basically a portmidi device info */
typedef struct {
    int id;
    char name[SPARK_MIDI_PORT_STRLEN];
} spark_midi_device_t;

int spark_midi_init(void); // Pm_Initialize
void spark_midi_destroy(void); // Pm_Close + Pm_Terminate

int spark_midi_list_devices(spark_midi_device_t *out, int max); // scan inputs
int spark_midi_find_device(const char *pattern); // returns id or -1

int spark_midi_open(int device_id); // Pm_OpenInput
int spark_midi_create_virtual(const char *name);

void spark_midi_close(int device_id); // Pm_Close
void spark_midi_close_all(void);

int spark_midi_poll(midi_event_t *out, int max); // Pm_Poll + Pm_Read + decode

#endif