#include "fixture.h"
#include "consts.h"
#include "log.h"

#include <string.h>

static spark_fixture_t s_fixtures[SPARK_FIXTURES_MAX];
static uint16_t s_fixture_count = 0;

static spark_channel_def_t s_channel_arena[SPARK_CHANNEL_ARENA_SIZE];
static uint16_t s_channel_arena_used = 0;

static spark_channel_def_t *s_channel_arena_alloc(uint16_t count)
{
    if (s_channel_arena_used + count > SPARK_CHANNEL_ARENA_SIZE)
        return NULL;
    spark_channel_def_t *ptr = &s_channel_arena[s_channel_arena_used];
    s_channel_arena_used += count;
    return ptr;
}

int spark_fixture_add(const spark_fixture_t *fixture)
{
    if (s_fixture_count >= SPARK_FIXTURES_MAX)
    {
        spark_log_error("fixture: max fixtures reached (%d)", SPARK_FIXTURES_MAX);
        return -1;
    }

    spark_channel_def_t *ch = s_channel_arena_alloc(fixture->channel_count);
    if (!ch)
    {
        spark_log_error("fixture: channel arena exhausted");
        return -1;
    }

    memcpy(ch, fixture->channels, fixture->channel_count * sizeof(spark_channel_def_t));

    spark_fixture_t *dst = &s_fixtures[s_fixture_count++];
    *dst = *fixture;
    dst->channels = ch;

    spark_log_debug("fixture: added '%s' at address %u (%u channels)",
        dst->id, dst->start_address, dst->channel_count);
    return 0;
}

void spark_fixture_reset(void)
{
    s_fixture_count = 0;
    s_channel_arena_used = 0;
}

const spark_fixture_t *spark_fixture_find(const char *id)
{
    for (uint16_t i = 0; i < s_fixture_count; i++)
    {
        if (strcmp(s_fixtures[i].id, id) == 0)
            return &s_fixtures[i];
    }
    return NULL;
}

const spark_channel_def_t *spark_fixture_find_channel(
    const spark_fixture_t *fixture, const char *channel_name)
{
    for (uint8_t i = 0; i < fixture->channel_count; i++)
    {
        if (strcmp(fixture->channels[i].name, channel_name) == 0)
            return &fixture->channels[i];
    }
    return NULL;
}

uint16_t spark_fixture_resolve_channel(const spark_fixture_t *fixture, uint8_t channel_offset)
{
    return fixture->start_address - 1 + channel_offset;
}
