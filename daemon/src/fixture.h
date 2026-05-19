/*
 * fixture.h - Fixture and channel definitions
 *
 * A fixture represents one physical light with a start address and a set
 * of named channels at offsets within that address range.
 *
 * Target resolution: DMX index = (start_address - 1) + channel.offset
 * This conversion maps user-facing 1-based addresses to 0-based frame indexes.
 *
 * Fixtures are stored in a module-level static array. At configuration
 * time, fixtures are added via spark_fixture_add() which deep-copies
 * the fixture struct and its channel array into static storage (channel
 * arena). The resolver looks up fixtures and channels by name to
 * compute DMX indices for scene values.
 *
 * At runtime, scenes hold pre-resolved DMX indexes directly.
 */
#ifndef SPARK_FIXTURE_H
#define SPARK_FIXTURE_H

#include <stdint.h>

typedef struct {
    const char *name;
    uint8_t offset;
} spark_channel_def_t;

typedef struct {
    const char *id;
    const char *name;
    uint16_t start_address;
    uint8_t channel_count;
    spark_channel_def_t *channels;
} spark_fixture_t;

/* Storage */
int  spark_fixture_add(const spark_fixture_t *fixture);
void spark_fixture_reset(void);

/* Lookup */
const spark_fixture_t *spark_fixture_find(const char *id);
const spark_channel_def_t *spark_fixture_find_channel(
    const spark_fixture_t *fixture, const char *channel_name);

/* Resolve channel offset to 0-based DMX index */
uint16_t spark_fixture_resolve_channel(const spark_fixture_t *fixture, uint8_t channel_offset);

#endif
