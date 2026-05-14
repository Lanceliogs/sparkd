#include "test.h"
#include "stage.h"

#define TEST_VELOCITY 100

void test_hardcoded_stage(void)
{
    /* Test props */
    uint8_t frame_null[SPARK_DMX_UNIVERSE_SIZE];
    uint8_t frame_modified[SPARK_DMX_UNIVERSE_SIZE];
    memset(frame_null, 0, SPARK_DMX_UNIVERSE_SIZE);
    memset(frame_modified, 0, SPARK_DMX_UNIVERSE_SIZE);
    frame_modified[0] = TEST_VELOCITY * 255 / 127;

    spark_stage_t stage;
    spark_stage_init(&stage);
    ASSERT_EQ(memcmp(frame_null, stage.frame, SPARK_DMX_UNIVERSE_SIZE), 0);

    midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0,
        .note = 60,
        .velocity = TEST_VELOCITY,
    };
    spark_stage_apply_midi(&stage, &evt_on);

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    spark_stage_render(&stage, frame);
    ASSERT_EQ(memcmp(frame_modified, frame, SPARK_DMX_UNIVERSE_SIZE), 0);

    midi_event_t evt_off = {
        .type = SPARK_MIDI_NOTE_OFF,
        .channel = 0,
        .note = 60,
    };
    spark_stage_apply_midi(&stage, &evt_off);
    spark_stage_render(&stage, frame);
    ASSERT_EQ(memcmp(frame_null, frame, SPARK_DMX_UNIVERSE_SIZE), 0);

    spark_stage_destroy(&stage);
}

int main(void)
{
    TEST_BEGIN();
    RUN_TEST(test_hardcoded_stage);
    TEST_END();
}