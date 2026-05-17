/*
 * midi_event.h - Normalized MIDI event representation
 *
 * Backend-independent MIDI event used throughout the engine.
 * Channel is 0-15 internally (user-facing 1-16).
 * Note-on with velocity 0 is treated as note-off by the stage.
 */
#ifndef SPARK_MIDI_EVENT_H
#define SPARK_MIDI_EVENT_H

#include <stdint.h>

typedef enum {
    SPARK_MIDI_NOTE_ON,
    SPARK_MIDI_NOTE_OFF,
    SPARK_MIDI_CC,
} spark_midi_event_type_t;

typedef struct {
    spark_midi_event_type_t type;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
    uint8_t cc;
    uint8_t value;
} spark_midi_event_t;

#endif