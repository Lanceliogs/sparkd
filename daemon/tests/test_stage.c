#include "test.h"
#include "stage.h"
#include "scene.h"
#include "log.h"

#define TEST_VELOCITY 100

void test_scene_gate(void)
{
    spark_stage_t stage;
    spark_stage_init(&stage);

    spark_scene_value_t values[] = {
        { .dmx_index = 0, .value = 255, .velocity_scaling = true },
    };

    spark_scene_t *scene = spark_scene_get(0, 60);
    scene->id = "test";
    scene->name = "Test Scene";
    scene->enabled = true;
    scene->trigger_mode = SPARK_SCENE_GATE;
    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = values;
    scene->output.value_count = 1;

    midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0,
        .note = 60,
        .velocity = TEST_VELOCITY,
    };
    spark_stage_apply_midi(&stage, &evt_on);

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    spark_stage_render(&stage, frame);

    uint8_t expected = (255 * TEST_VELOCITY) / 127;
    ASSERT_EQ(frame[0], expected);

    midi_event_t evt_off = {
        .type = SPARK_MIDI_NOTE_OFF,
        .channel = 0,
        .note = 60,
    };
    spark_stage_apply_midi(&stage, &evt_off);
    spark_stage_render(&stage, frame);
    ASSERT_EQ(frame[0], 0);

    spark_stage_destroy(&stage);
}

void test_scene_toggle(void)
{
    spark_stage_t stage;
    spark_stage_init(&stage);

    spark_scene_value_t values[] = {
        { .dmx_index = 1, .value = 128, .velocity_scaling = false },
    };

    spark_scene_t *scene = spark_scene_get(0, 61);
    scene->id = "toggle-test";
    scene->name = "Toggle Test";
    scene->enabled = true;
    scene->trigger_mode = SPARK_SCENE_TOGGLE;
    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = values;
    scene->output.value_count = 1;

    midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0,
        .note = 61,
        .velocity = 127,
    };

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];

    spark_stage_apply_midi(&stage, &evt_on);
    spark_stage_render(&stage, frame);
    ASSERT_EQ(frame[1], 128);

    spark_stage_apply_midi(&stage, &evt_on);
    spark_stage_render(&stage, frame);
    ASSERT_EQ(frame[1], 0);

    spark_stage_destroy(&stage);
}

void test_scene_blackout(void)
{
    spark_stage_t stage;
    spark_stage_init(&stage);

    spark_scene_value_t values[] = {
        { .dmx_index = 0, .value = 200, .velocity_scaling = false },
    };

    spark_scene_t *scene = spark_scene_get(0, 62);
    scene->id = "blackout-test";
    scene->name = "Blackout Test";
    scene->enabled = true;
    scene->trigger_mode = SPARK_SCENE_GATE;
    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = values;
    scene->output.value_count = 1;

    midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0,
        .note = 62,
        .velocity = 127,
    };
    spark_stage_apply_midi(&stage, &evt_on);

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];

    spark_stage_set_blackout(&stage, true);
    spark_stage_render(&stage, frame);
    ASSERT_EQ(frame[0], 0);

    spark_stage_set_blackout(&stage, false);
    spark_stage_render(&stage, frame);
    ASSERT_EQ(frame[0], 200);

    spark_stage_destroy(&stage);
}

void test_scene_disabled(void)
{
    spark_stage_t stage;
    spark_stage_init(&stage);

    spark_scene_value_t values[] = {
        { .dmx_index = 0, .value = 255, .velocity_scaling = false },
    };

    spark_scene_t *scene = spark_scene_get(0, 63);
    scene->id = "disabled-test";
    scene->name = "Disabled Test";
    scene->enabled = false;
    scene->trigger_mode = SPARK_SCENE_GATE;
    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = values;
    scene->output.value_count = 1;

    midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0,
        .note = 63,
        .velocity = 127,
    };
    spark_stage_apply_midi(&stage, &evt_on);

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    spark_stage_render(&stage, frame);
    ASSERT_EQ(frame[0], 0);

    spark_stage_destroy(&stage);
}

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);
    TEST_BEGIN();
    RUN_TEST(test_scene_gate);
    RUN_TEST(test_scene_toggle);
    RUN_TEST(test_scene_blackout);
    RUN_TEST(test_scene_disabled);
    TEST_END();
}
