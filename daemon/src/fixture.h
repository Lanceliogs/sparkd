/*
 * fixture.h - Fixture and channel definitions
 *
 * A fixture represents one physical light with a start address and a set
 * of named channels at offsets within that address range.
 *
 * Target resolution: DMX index = (start_address - 1) + channel.offset
 * This conversion maps user-facing 1-based addresses to 0-based frame indexes.
 *
 * Fixtures are used at configuration time to resolve scene targets.
 * At runtime, scenes hold pre-resolved DMX indexes directly.
 */
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