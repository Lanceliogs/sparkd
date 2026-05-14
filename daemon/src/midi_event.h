#ifndef SPARK_MIDI_EVENT_H
#define SPARK_MIDI_EVENT_H

#include <stdint.h>

typedef enum {
    SPARK_MIDI_NOTE_ON,
    SPARK_MIDI_NOTE_OFF,
    SPARK_MIDI_CC,
} midi_event_type_t;

typedef struct {
    midi_event_type_t type;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    uint8_t cc;
    uint8_t value;
} midi_event_t;

#endif