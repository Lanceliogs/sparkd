#ifndef SPARK_FIXTURE_H
#define SPARK_FIXTURE_H

#include <stdint.h>

typedef struct {
    const char *name; // e.g. "dimmer", "red"
    uint8_t offset; // 0-based within fixture
    // kind, merge, safety-class can come later
} spark_channel_def_t;         

typedef struct {
    const char *id; // e.g. "stairville"
    const char *name; // e.g. "Stairville Quad Par"
    uint16_t start_address; // 1-based user facing, stored as 0-based internally
    uint8_t channel_count;
    spark_channel_def_t *channels; // array of channel_count entries
} spark_fixture_t;

/* Resolve channel using fixture address and channel offset */
uint16_t spark_fixture_resolve_channel(const spark_fixture_t *fixture, uint8_t channel_offset);

#endif