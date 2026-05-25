/*
 * midi.h - MIDI input management
 *
 * Wraps PortMidi to provide a multi-device MIDI input layer. Multiple
 * physical or virtual MIDI inputs can be opened simultaneously; polling
 * drains all open streams into a single event buffer.
 *
 * Device lifecycle:
 *   init     - call once at startup (initializes PortMidi)
 *   open     - open a device by ID or name pattern (substring match)
 *   poll     - drain all open streams into a spark_midi_event_t buffer
 *   close    - close a single device, close_all for bulk teardown
 *   destroy  - call once at shutdown (terminates PortMidi)
 *
 * Reconnection:
 *   reconnect performs a full Pm_Terminate / Pm_Initialize cycle and
 *   reopens all previously registered devices by their stored name
 *   patterns. This handles USB hot-unplug/replug on platforms where
 *   PortMidi does not report read errors on dead streams.
 *
 * Events are normalized into spark_midi_event_t (note-on, note-off, CC).
 * Velocity-zero note-on is automatically converted to note-off.
 */
#ifndef SPARK_MIDI_H
#define SPARK_MIDI_H

#include "consts.h"
#include "midi_event.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    SPARK_MIDI_MODE_NONE,
    SPARK_MIDI_MODE_OPEN_EXISTING,
    SPARK_MIDI_MODE_CREATE_VIRTUAL,
} spark_midi_mode_t;

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

int spark_midi_reconnect(void);

/* Status reporting */
typedef struct {
    char pattern[SPARK_MIDI_PORT_STRLEN];
    char device_name[SPARK_MIDI_PORT_STRLEN];
    bool connected;
    uint64_t last_activity_ms;
} spark_midi_port_status_t;

int spark_midi_get_status(spark_midi_port_status_t *out, int max);

#endif