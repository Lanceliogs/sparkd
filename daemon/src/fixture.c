#include "fixture.h"

uint16_t spark_fixture_resolve_channel(const spark_fixture_t *fixture, uint8_t channel_offset)
{
    return fixture->start_address - 1 + channel_offset;
}
