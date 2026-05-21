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

    static const spark_scene_def_t def = {
        .id = "test", .name = "Test Scene", .enabled = true,
        .trigger_mode = SPARK_SCENE_GATE,
    };
    spark_scene_t *scene = spark_scene_get(0, 60);
    scene->def = &def;
    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = values;
    scene->output.value_count = 1;

    spark_midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0,
        .note = 60,
        .velocity = TEST_VELOCITY,
    };
    spark_stage_apply_midi(&stage, &evt_on);

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    spark_stage_render(&stage, 1000, frame);

    uint8_t expected = (255 * TEST_VELOCITY) / 127;
    ASSERT_EQ(frame[0], expected);

    spark_midi_event_t evt_off = {
        .type = SPARK_MIDI_NOTE_OFF,
        .channel = 0,
        .note = 60,
    };
    spark_stage_apply_midi(&stage, &evt_off);
    spark_stage_render(&stage, 1000, frame);
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

    static const spark_scene_def_t def = {
        .id = "toggle-test", .name = "Toggle Test", .enabled = true,
        .trigger_mode = SPARK_SCENE_TOGGLE,
    };
    spark_scene_t *scene = spark_scene_get(0, 61);
    scene->def = &def;
    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = values;
    scene->output.value_count = 1;

    spark_midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0,
        .note = 61,
        .velocity = 127,
    };

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];

    spark_stage_apply_midi(&stage, &evt_on);
    spark_stage_render(&stage, 1000, frame);
    ASSERT_EQ(frame[1], 128);

    spark_stage_apply_midi(&stage, &evt_on);
    spark_stage_render(&stage, 1000, frame);
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

    static const spark_scene_def_t def = {
        .id = "blackout-test", .name = "Blackout Test", .enabled = true,
        .trigger_mode = SPARK_SCENE_GATE,
    };
    spark_scene_t *scene = spark_scene_get(0, 62);
    scene->def = &def;
    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = values;
    scene->output.value_count = 1;

    spark_midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0,
        .note = 62,
        .velocity = 127,
    };
    spark_stage_apply_midi(&stage, &evt_on);

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];

    spark_stage_set_blackout(&stage, true);
    spark_stage_render(&stage, 1000, frame);
    ASSERT_EQ(frame[0], 0);

    spark_stage_set_blackout(&stage, false);
    spark_stage_render(&stage, 1000, frame);
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

    static const spark_scene_def_t def = {
        .id = "disabled-test", .name = "Disabled Test", .enabled = false,
        .trigger_mode = SPARK_SCENE_GATE,
    };
    spark_scene_t *scene = spark_scene_get(0, 63);
    scene->def = &def;
    scene->output.mode = SPARK_SCENE_STATIC;
    scene->output.values = values;
    scene->output.value_count = 1;

    spark_midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0,
        .note = 63,
        .velocity = 127,
    };
    spark_stage_apply_midi(&stage, &evt_on);

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    spark_stage_render(&stage, 1000, frame);
    ASSERT_EQ(frame[0], 0);

    spark_stage_destroy(&stage);
}

void test_sequence_render(void)
{
    spark_stage_t stage;
    spark_stage_init(&stage);

    /* Two-step blink: step0=ON(80ms), step1=OFF(80ms), loop */
    spark_scene_value_t step0_vals[] = {
        { .dmx_index = 0, .value = 255, .velocity_scaling = false },
    };
    spark_scene_value_t step1_vals[] = {
        { .dmx_index = 0, .value = 0, .velocity_scaling = false },
    };
    spark_scene_step_t steps[] = {
        { .duration_ms = 80, .values = step0_vals, .value_count = 1 },
        { .duration_ms = 80, .values = step1_vals, .value_count = 1 },
    };

    static const spark_scene_def_t def = {
        .id = "blink", .name = "Blink", .enabled = true,
        .trigger_mode = SPARK_SCENE_GATE,
    };
    spark_scene_t *scene = spark_scene_get(0, 55);
    scene->def = &def;
    scene->output.mode = SPARK_SCENE_SEQUENCE;
    scene->output.steps = steps;
    scene->output.step_count = 2;
    scene->output.loop = true;

    spark_midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0, .note = 55, .velocity = 127,
    };
    spark_stage_apply_midi(&stage, &evt_on);

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    uint64_t activation_time = scene->start_time_ms;

    /* t=0: step 0 (ON) */
    spark_stage_render(&stage, activation_time, frame);
    ASSERT_EQ(frame[0], 255);

    /* t=40ms: still step 0 */
    spark_stage_render(&stage, activation_time + 40, frame);
    ASSERT_EQ(frame[0], 255);

    /* t=80ms: step 1 (OFF) */
    spark_stage_render(&stage, activation_time + 80, frame);
    ASSERT_EQ(frame[0], 0);

    /* t=120ms: still step 1 */
    spark_stage_render(&stage, activation_time + 120, frame);
    ASSERT_EQ(frame[0], 0);

    /* t=160ms: loop back to step 0 */
    spark_stage_render(&stage, activation_time + 160, frame);
    ASSERT_EQ(frame[0], 255);

    /* t=240ms: loop, step 1 again */
    spark_stage_render(&stage, activation_time + 240, frame);
    ASSERT_EQ(frame[0], 0);

    spark_stage_destroy(&stage);
}

void test_sequence_no_loop(void)
{
    spark_stage_t stage;
    spark_stage_init(&stage);

    spark_scene_value_t step0_vals[] = {
        { .dmx_index = 2, .value = 100, .velocity_scaling = false },
    };
    spark_scene_value_t step1_vals[] = {
        { .dmx_index = 2, .value = 200, .velocity_scaling = false },
    };
    spark_scene_step_t steps[] = {
        { .duration_ms = 50, .values = step0_vals, .value_count = 1 },
        { .duration_ms = 50, .values = step1_vals, .value_count = 1 },
    };

    static const spark_scene_def_t def = {
        .id = "fade", .name = "Fade", .enabled = true,
        .trigger_mode = SPARK_SCENE_GATE,
    };
    spark_scene_t *scene = spark_scene_get(0, 56);
    scene->def = &def;
    scene->output.mode = SPARK_SCENE_SEQUENCE;
    scene->output.steps = steps;
    scene->output.step_count = 2;
    scene->output.loop = false;

    spark_midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0, .note = 56, .velocity = 127,
    };
    spark_stage_apply_midi(&stage, &evt_on);

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    uint64_t t = scene->start_time_ms;

    /* step 0 */
    spark_stage_render(&stage, t, frame);
    ASSERT_EQ(frame[2], 100);

    /* step 1 */
    spark_stage_render(&stage, t + 50, frame);
    ASSERT_EQ(frame[2], 200);

    /* past end: hold last step */
    spark_stage_render(&stage, t + 200, frame);
    ASSERT_EQ(frame[2], 200);

    spark_stage_destroy(&stage);
}

void test_sequence_linear_transition(void)
{
    spark_stage_t stage;
    spark_stage_init(&stage);

    /* Two steps: 0->255 over 100ms (linear), then 255->0 over 100ms (linear), loop */
    spark_scene_value_t step0_vals[] = {
        { .dmx_index = 0, .value = 0, .velocity_scaling = false },
    };
    spark_scene_value_t step1_vals[] = {
        { .dmx_index = 0, .value = 200, .velocity_scaling = false },
    };
    spark_scene_step_t steps[] = {
        { .duration_ms = 100, .transition = SPARK_SCENE_LINEAR,
          .values = step0_vals, .value_count = 1 },
        { .duration_ms = 100, .transition = SPARK_SCENE_LINEAR,
          .values = step1_vals, .value_count = 1 },
    };

    static const spark_scene_def_t def = {
        .id = "lerp", .name = "Lerp", .enabled = true,
        .trigger_mode = SPARK_SCENE_GATE,
    };
    spark_scene_t *scene = spark_scene_get(0, 57);
    scene->def = &def;
    scene->output.mode = SPARK_SCENE_SEQUENCE;
    scene->output.steps = steps;
    scene->output.step_count = 2;
    scene->output.loop = true;

    spark_midi_event_t evt_on = {
        .type = SPARK_MIDI_NOTE_ON,
        .channel = 0, .note = 57, .velocity = 127,
    };
    spark_stage_apply_midi(&stage, &evt_on);

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    uint64_t t = scene->start_time_ms;

    /* step 0 transitions from val=0 toward next step val=200 */
    /* t=0: start of step 0, progress=0 -> value=0 */
    spark_stage_render(&stage, t, frame);
    ASSERT_EQ(frame[0], 0);

    /* t=50: halfway through step 0, progress=0.5 -> value=100 */
    spark_stage_render(&stage, t + 50, frame);
    ASSERT_EQ(frame[0], 100);

    /* t=99: nearly end of step 0 -> value~198 */
    spark_stage_render(&stage, t + 99, frame);
    ASSERT_EQ(frame[0], 198);

    /* t=100: start of step 1, transitions from 200 toward 0 */
    /* progress=0 -> value=200 */
    spark_stage_render(&stage, t + 100, frame);
    ASSERT_EQ(frame[0], 200);

    /* t=150: halfway through step 1 -> value=100 */
    spark_stage_render(&stage, t + 150, frame);
    ASSERT_EQ(frame[0], 100);

    /* t=199: nearly end of step 1 -> value~2 */
    spark_stage_render(&stage, t + 199, frame);
    ASSERT_EQ(frame[0], 2);

    /* t=200: loop back to step 0, value=0 */
    spark_stage_render(&stage, t + 200, frame);
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
    RUN_TEST(test_sequence_render);
    RUN_TEST(test_sequence_no_loop);
    RUN_TEST(test_sequence_linear_transition);
    TEST_END();
}
