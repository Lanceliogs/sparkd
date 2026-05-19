#include "test.h"
#include "log.h"
#include "fixture.h"
#include "consts.h"

#include <string.h>

static spark_channel_def_t par_channels[] = {
    { .name = "dimmer", .offset = 0 },
    { .name = "red",    .offset = 1 },
    { .name = "green",  .offset = 2 },
    { .name = "blue",   .offset = 3 },
    { .name = "white",  .offset = 4 },
    { .name = "strobe", .offset = 5 },
    { .name = "color",  .offset = 6 },
    { .name = "mode",   .offset = 7 },
};

static spark_fixture_t par_fixture = {
    .id = "par1",
    .name = "Stairville Quad Par",
    .start_address = 1,
    .channel_count = 8,
    .channels = par_channels,
};

void test_add_and_find(void)
{
    spark_fixture_reset();
    ASSERT_EQ(spark_fixture_add(&par_fixture), 0);

    const spark_fixture_t *f = spark_fixture_find("par1");
    ASSERT_TRUE(f != NULL);
    ASSERT_STR_EQ(f->id, "par1");
    ASSERT_EQ(f->start_address, 1);
    ASSERT_EQ(f->channel_count, 8);
}

void test_find_miss(void)
{
    spark_fixture_reset();
    spark_fixture_add(&par_fixture);

    const spark_fixture_t *f = spark_fixture_find("nonexistent");
    ASSERT_TRUE(f == NULL);
}

void test_find_channel(void)
{
    spark_fixture_reset();
    spark_fixture_add(&par_fixture);

    const spark_fixture_t *f = spark_fixture_find("par1");
    ASSERT_TRUE(f != NULL);

    const spark_channel_def_t *ch = spark_fixture_find_channel(f, "red");
    ASSERT_TRUE(ch != NULL);
    ASSERT_EQ(ch->offset, 1);

    ch = spark_fixture_find_channel(f, "blue");
    ASSERT_TRUE(ch != NULL);
    ASSERT_EQ(ch->offset, 3);
}

void test_find_channel_miss(void)
{
    spark_fixture_reset();
    spark_fixture_add(&par_fixture);

    const spark_fixture_t *f = spark_fixture_find("par1");
    const spark_channel_def_t *ch = spark_fixture_find_channel(f, "fog");
    ASSERT_TRUE(ch == NULL);
}

void test_resolve_channel(void)
{
    spark_fixture_reset();
    spark_fixture_add(&par_fixture);

    const spark_fixture_t *f = spark_fixture_find("par1");
    ASSERT_TRUE(f != NULL);

    const spark_channel_def_t *ch = spark_fixture_find_channel(f, "red");
    ASSERT_TRUE(ch != NULL);

    uint16_t dmx_index = spark_fixture_resolve_channel(f, ch->offset);
    ASSERT_EQ(dmx_index, 1);

    ch = spark_fixture_find_channel(f, "dimmer");
    dmx_index = spark_fixture_resolve_channel(f, ch->offset);
    ASSERT_EQ(dmx_index, 0);

    ch = spark_fixture_find_channel(f, "blue");
    dmx_index = spark_fixture_resolve_channel(f, ch->offset);
    ASSERT_EQ(dmx_index, 3);
}

void test_resolve_channel_offset_address(void)
{
    spark_fixture_reset();

    spark_fixture_t fixture_at_10 = par_fixture;
    strncpy(fixture_at_10.id, "par_at_10", SPARK_MAX_ID_SIZE - 1);
    fixture_at_10.start_address = 10;
    spark_fixture_add(&fixture_at_10);

    const spark_fixture_t *f = spark_fixture_find("par_at_10");
    const spark_channel_def_t *ch = spark_fixture_find_channel(f, "red");
    uint16_t dmx_index = spark_fixture_resolve_channel(f, ch->offset);
    ASSERT_EQ(dmx_index, 10);
}

void test_reset_clears(void)
{
    spark_fixture_reset();
    spark_fixture_add(&par_fixture);
    ASSERT_TRUE(spark_fixture_find("par1") != NULL);

    spark_fixture_reset();
    ASSERT_TRUE(spark_fixture_find("par1") == NULL);
}

void test_multiple_fixtures(void)
{
    spark_fixture_reset();

    spark_channel_def_t dimmer_ch[] = { { .name = "intensity", .offset = 0 } };
    spark_fixture_t dimmer = {
        .id = "dimmer1", .name = "Generic Dimmer",
        .start_address = 20, .channel_count = 1, .channels = dimmer_ch,
    };

    ASSERT_EQ(spark_fixture_add(&par_fixture), 0);
    ASSERT_EQ(spark_fixture_add(&dimmer), 0);

    ASSERT_TRUE(spark_fixture_find("par1") != NULL);
    ASSERT_TRUE(spark_fixture_find("dimmer1") != NULL);

    const spark_fixture_t *d = spark_fixture_find("dimmer1");
    ASSERT_EQ(d->start_address, 20);
    ASSERT_EQ(d->channel_count, 1);

    const spark_channel_def_t *ch = spark_fixture_find_channel(d, "intensity");
    ASSERT_TRUE(ch != NULL);
    ASSERT_EQ(spark_fixture_resolve_channel(d, ch->offset), 19);
}

void test_deep_copy(void)
{
    spark_fixture_reset();

    spark_channel_def_t local_ch[] = {
        { .name = "r", .offset = 0 },
        { .name = "g", .offset = 1 },
    };
    spark_fixture_t local = {
        .id = "local", .name = "Local",
        .start_address = 5, .channel_count = 2, .channels = local_ch,
    };
    spark_fixture_add(&local);

    local_ch[0].offset = 99;

    const spark_fixture_t *f = spark_fixture_find("local");
    const spark_channel_def_t *ch = spark_fixture_find_channel(f, "r");
    ASSERT_EQ(ch->offset, 0);
}

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);

    TEST_BEGIN();
    RUN_TEST(test_add_and_find);
    RUN_TEST(test_find_miss);
    RUN_TEST(test_find_channel);
    RUN_TEST(test_find_channel_miss);
    RUN_TEST(test_resolve_channel);
    RUN_TEST(test_resolve_channel_offset_address);
    RUN_TEST(test_reset_clears);
    RUN_TEST(test_multiple_fixtures);
    RUN_TEST(test_deep_copy);
    TEST_END();
}
