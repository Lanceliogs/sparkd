#include "test.h"
#include "scene.h"
#include "fixture.h"
#include "log.h"

static const spark_scene_def_t s_test_def = {
    .id = "test", .name = "test", .enabled = true,
    .trigger_mode = SPARK_SCENE_GATE,
};

void test_scene_activate_basic(void)
{
    spark_scene_reset();

    spark_scene_t *scene = spark_scene_get(0, 60);
    scene->def = &s_test_def;

    ASSERT_TRUE(!scene->active);
    spark_scene_activate(scene, 100);
    ASSERT_TRUE(scene->active);
    ASSERT_EQ(scene->velocity, 100);
    ASSERT_TRUE(scene->start_time_ms > 0);
}

void test_scene_activate_already_active(void)
{
    spark_scene_reset();

    spark_scene_t *scene = spark_scene_get(0, 61);
    scene->def = &s_test_def;

    spark_scene_activate(scene, 80);
    spark_scene_activate(scene, 127);

    ASSERT_EQ(scene->velocity, 80);

    uint16_t count;
    spark_scene_get_active(&count);
    ASSERT_EQ(count, 1);
}

void test_scene_deactivate_basic(void)
{
    spark_scene_reset();

    spark_scene_t *scene = spark_scene_get(0, 62);
    scene->def = &s_test_def;

    spark_scene_activate(scene, 100);
    ASSERT_TRUE(scene->active);

    spark_scene_deactivate(scene);
    ASSERT_TRUE(!scene->active);

    uint16_t count;
    spark_scene_get_active(&count);
    ASSERT_EQ(count, 0);
}

void test_scene_deactivate_not_active(void)
{
    spark_scene_reset();

    spark_scene_t *scene = spark_scene_get(0, 63);
    scene->def = &s_test_def;

    spark_scene_deactivate(scene);
    ASSERT_TRUE(!scene->active);

    uint16_t count;
    spark_scene_get_active(&count);
    ASSERT_EQ(count, 0);
}

void test_scene_toggle_on_off(void)
{
    spark_scene_reset();

    spark_scene_t *scene = spark_scene_get(0, 64);
    scene->def = &s_test_def;

    spark_scene_toggle(scene, 100);
    ASSERT_TRUE(scene->active);

    spark_scene_toggle(scene, 100);
    ASSERT_TRUE(!scene->active);

    uint16_t count;
    spark_scene_get_active(&count);
    ASSERT_EQ(count, 0);
}

void test_scene_multiple_active(void)
{
    spark_scene_reset();

    static const spark_scene_def_t d1 = { .id = "scene-1", .enabled = true };
    static const spark_scene_def_t d2 = { .id = "scene-2", .enabled = true };
    static const spark_scene_def_t d3 = { .id = "scene-3", .enabled = true };

    spark_scene_t *s1 = spark_scene_get(0, 70);
    s1->def = &d1;
    spark_scene_t *s2 = spark_scene_get(0, 71);
    s2->def = &d2;
    spark_scene_t *s3 = spark_scene_get(0, 72);
    s3->def = &d3;

    spark_scene_activate(s1, 127);
    spark_scene_activate(s2, 127);
    spark_scene_activate(s3, 127);

    uint16_t count;
    spark_scene_get_active(&count);
    ASSERT_EQ(count, 3);

    spark_scene_deactivate(s2);
    spark_scene_get_active(&count);
    ASSERT_EQ(count, 2);
    ASSERT_TRUE(!s2->active);
    ASSERT_TRUE(s1->active);
    ASSERT_TRUE(s3->active);
}

void test_scene_reset_clears_all(void)
{
    spark_scene_reset();

    spark_scene_t *scene = spark_scene_get(0, 80);
    scene->def = &s_test_def;
    spark_scene_activate(scene, 127);

    uint16_t count;
    spark_scene_get_active(&count);
    ASSERT_EQ(count, 1);

    spark_scene_reset();
    spark_scene_get_active(&count);
    ASSERT_EQ(count, 0);
}

void test_resolve_static_raw(void)
{
    spark_scene_reset();

    spark_scene_value_def_t vals[] = {
        { .dmx_index = 0, .value = 255, .velocity_scaling = false },
        { .dmx_index = 3, .value = 128, .velocity_scaling = true },
    };

    spark_scene_def_t def = {
        .channel = 0, .note = 60,
        .id = "test-raw", .name = "Test Raw",
        .enabled = true,
        .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_STATIC,
        .values = vals, .value_count = 2,
    };

    ASSERT_EQ(spark_scene_add_def(&def), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);

    spark_scene_t *scene = spark_scene_get(0, 60);
    ASSERT_STR_EQ(scene->def->id, "test-raw");
    ASSERT_TRUE(scene->def->enabled);
    ASSERT_EQ(scene->def->trigger_mode, SPARK_SCENE_GATE);
    ASSERT_EQ(scene->output.mode, SPARK_SCENE_STATIC);
    ASSERT_EQ(scene->output.value_count, 2);
    ASSERT_EQ(scene->output.values[0].dmx_index, 0);
    ASSERT_EQ(scene->output.values[0].value, 255);
    ASSERT_EQ(scene->output.values[0].velocity_scaling, false);
    ASSERT_EQ(scene->output.values[1].dmx_index, 3);
    ASSERT_EQ(scene->output.values[1].value, 128);
    ASSERT_EQ(scene->output.values[1].velocity_scaling, true);
}

void test_resolve_multiple_scenes(void)
{
    spark_scene_reset();

    spark_scene_value_def_t vals_a[] = {
        { .dmx_index = 0, .value = 200 },
    };
    spark_scene_value_def_t vals_b[] = {
        { .dmx_index = 5, .value = 100 },
        { .dmx_index = 6, .value = 50 },
    };

    spark_scene_def_t def_a = {
        .channel = 0, .note = 60, .id = "a", .name = "A",
        .enabled = true, .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_STATIC,
        .values = vals_a, .value_count = 1,
    };
    spark_scene_def_t def_b = {
        .channel = 1, .note = 48, .id = "b", .name = "B",
        .enabled = true, .trigger_mode = SPARK_SCENE_TOGGLE,
        .output_mode = SPARK_SCENE_STATIC,
        .values = vals_b, .value_count = 2,
    };

    ASSERT_EQ(spark_scene_add_def(&def_a), 0);
    ASSERT_EQ(spark_scene_add_def(&def_b), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);

    spark_scene_t *sa = spark_scene_get(0, 60);
    ASSERT_STR_EQ(sa->def->id, "a");
    ASSERT_EQ(sa->output.value_count, 1);

    spark_scene_t *sb = spark_scene_get(1, 48);
    ASSERT_STR_EQ(sb->def->id, "b");
    ASSERT_EQ(sb->def->trigger_mode, SPARK_SCENE_TOGGLE);
    ASSERT_EQ(sb->output.value_count, 2);
    ASSERT_EQ(sb->output.values[1].dmx_index, 6);
}

void test_resolve_unresolved_skipped(void)
{
    spark_scene_reset();

    spark_scene_value_def_t vals[] = {
        { .dmx_index = 0, .value = 255 },
        { .fixture = "missing", .channel = "red", .value = 128 },
        { .dmx_index = 2, .value = 64 },
    };

    spark_scene_def_t def = {
        .channel = 0, .note = 70, .id = "partial", .name = "Partial",
        .enabled = true, .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_STATIC,
        .values = vals, .value_count = 3,
    };

    ASSERT_EQ(spark_scene_add_def(&def), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);

    spark_scene_t *scene = spark_scene_get(0, 70);
    ASSERT_EQ(scene->output.value_count, 2);
    ASSERT_EQ(scene->output.values[0].dmx_index, 0);
    ASSERT_EQ(scene->output.values[1].dmx_index, 2);
}

void test_resolve_empty_defs(void)
{
    spark_scene_reset();
    ASSERT_EQ(spark_scene_resolve(), 0);
}

void test_resolve_reset_clears_arena(void)
{
    spark_scene_reset();

    spark_scene_value_def_t vals[] = {
        { .dmx_index = 0, .value = 255 },
    };
    spark_scene_def_t def = {
        .channel = 0, .note = 60, .id = "x", .name = "X",
        .enabled = true, .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_STATIC,
        .values = vals, .value_count = 1,
    };

    ASSERT_EQ(spark_scene_add_def(&def), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);
    spark_scene_t *scene = spark_scene_get(0, 60);
    ASSERT_EQ(scene->output.value_count, 1);

    spark_scene_reset();
    scene = spark_scene_get(0, 60);
    ASSERT_EQ(scene->output.value_count, 0);

    ASSERT_EQ(spark_scene_add_def(&def), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);
    scene = spark_scene_get(0, 60);
    ASSERT_EQ(scene->output.value_count, 1);
}

void test_resolve_fixture_channel(void)
{
    spark_scene_reset();
    spark_fixture_reset();

    spark_channel_def_t channels[] = {
        { .name = "dimmer", .offset = 0 },
        { .name = "red",    .offset = 1 },
        { .name = "green",  .offset = 2 },
        { .name = "blue",   .offset = 3 },
    };
    spark_fixture_t fix = {
        .id = "par1", .name = "Par",
        .start_address = 10, .channel_count = 4, .channels = channels,
    };
    spark_fixture_add(&fix);

    spark_scene_value_def_t vals[] = {
        { .fixture = "par1", .channel = "red",   .value = 255 },
        { .fixture = "par1", .channel = "blue",  .value = 128 },
    };

    spark_scene_def_t def = {
        .channel = 0, .note = 60, .id = "fix-test", .name = "Fixture Test",
        .enabled = true, .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_STATIC,
        .values = vals, .value_count = 2,
    };

    ASSERT_EQ(spark_scene_add_def(&def), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);

    spark_scene_t *scene = spark_scene_get(0, 60);
    ASSERT_EQ(scene->output.value_count, 2);
    ASSERT_EQ(scene->output.values[0].dmx_index, 10);
    ASSERT_EQ(scene->output.values[0].value, 255);
    ASSERT_EQ(scene->output.values[1].dmx_index, 12);
    ASSERT_EQ(scene->output.values[1].value, 128);
}

void test_resolve_fixture_not_found(void)
{
    spark_scene_reset();
    spark_fixture_reset();

    spark_scene_value_def_t vals[] = {
        { .fixture = "nope", .channel = "red", .value = 255 },
    };

    spark_scene_def_t def = {
        .channel = 0, .note = 61, .id = "miss", .name = "Miss",
        .enabled = true, .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_STATIC,
        .values = vals, .value_count = 1,
    };

    ASSERT_EQ(spark_scene_add_def(&def), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);

    spark_scene_t *scene = spark_scene_get(0, 61);
    ASSERT_EQ(scene->output.value_count, 0);
}

void test_resolve_channel_not_found(void)
{
    spark_scene_reset();
    spark_fixture_reset();

    spark_channel_def_t channels[] = {
        { .name = "dimmer", .offset = 0 },
    };
    spark_fixture_t fix = {
        .id = "par1", .name = "Par",
        .start_address = 1, .channel_count = 1, .channels = channels,
    };
    spark_fixture_add(&fix);

    spark_scene_value_def_t vals[] = {
        { .fixture = "par1", .channel = "fog", .value = 255 },
    };

    spark_scene_def_t def = {
        .channel = 0, .note = 62, .id = "bad-ch", .name = "Bad Ch",
        .enabled = true, .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_STATIC,
        .values = vals, .value_count = 1,
    };

    ASSERT_EQ(spark_scene_add_def(&def), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);

    spark_scene_t *scene = spark_scene_get(0, 62);
    ASSERT_EQ(scene->output.value_count, 0);
}

void test_resolve_mixed_raw_and_fixture(void)
{
    spark_scene_reset();
    spark_fixture_reset();

    spark_channel_def_t channels[] = {
        { .name = "dimmer", .offset = 0 },
        { .name = "red",    .offset = 1 },
    };
    spark_fixture_t fix = {
        .id = "par1", .name = "Par",
        .start_address = 20, .channel_count = 2, .channels = channels,
    };
    spark_fixture_add(&fix);

    spark_scene_value_def_t vals[] = {
        { .dmx_index = 0, .value = 100 },
        { .fixture = "par1", .channel = "red", .value = 200 },
        { .dmx_index = 5, .value = 50 },
    };

    spark_scene_def_t def = {
        .channel = 0, .note = 63, .id = "mixed", .name = "Mixed",
        .enabled = true, .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_STATIC,
        .values = vals, .value_count = 3,
    };

    ASSERT_EQ(spark_scene_add_def(&def), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);

    spark_scene_t *scene = spark_scene_get(0, 63);
    ASSERT_EQ(scene->output.value_count, 3);
    ASSERT_EQ(scene->output.values[0].dmx_index, 0);
    ASSERT_EQ(scene->output.values[0].value, 100);
    ASSERT_EQ(scene->output.values[1].dmx_index, 20);
    ASSERT_EQ(scene->output.values[1].value, 200);
    ASSERT_EQ(scene->output.values[2].dmx_index, 5);
    ASSERT_EQ(scene->output.values[2].value, 50);
}

void test_resolve_sequence_basic(void)
{
    spark_scene_reset();

    spark_scene_value_def_t step0_vals[] = {
        { .dmx_index = 0, .value = 255 },
        { .dmx_index = 1, .value = 255 },
    };
    spark_scene_value_def_t step1_vals[] = {
        { .dmx_index = 0, .value = 0 },
        { .dmx_index = 1, .value = 0 },
    };

    spark_scene_step_def_t steps[] = {
        { .duration_ms = 80, .values = step0_vals, .value_count = 2 },
        { .duration_ms = 80, .values = step1_vals, .value_count = 2 },
    };

    spark_scene_def_t def = {
        .channel = 0, .note = 50,
        .id = "blink", .name = "Blink",
        .enabled = true,
        .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_SEQUENCE,
        .steps = steps, .step_count = 2,
        .loop = true,
    };

    ASSERT_EQ(spark_scene_add_def(&def), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);

    spark_scene_t *scene = spark_scene_get(0, 50);
    ASSERT_STR_EQ(scene->def->id, "blink");
    ASSERT_EQ(scene->output.mode, SPARK_SCENE_SEQUENCE);
    ASSERT_EQ(scene->output.step_count, 2);
    ASSERT_TRUE(scene->output.loop);
    ASSERT_EQ(scene->output.steps[0].duration_ms, 80);
    ASSERT_EQ(scene->output.steps[0].value_count, 2);
    ASSERT_EQ(scene->output.steps[0].values[0].dmx_index, 0);
    ASSERT_EQ(scene->output.steps[0].values[0].value, 255);
    ASSERT_EQ(scene->output.steps[1].duration_ms, 80);
    ASSERT_EQ(scene->output.steps[1].values[0].value, 0);
}

void test_resolve_sequence_fixture(void)
{
    spark_scene_reset();
    spark_fixture_reset();

    spark_channel_def_t channels[] = {
        { .name = "dimmer", .offset = 0 },
        { .name = "red",    .offset = 1 },
    };
    spark_fixture_t fix = {
        .id = "par1", .name = "Par",
        .start_address = 5, .channel_count = 2, .channels = channels,
    };
    spark_fixture_add(&fix);

    spark_scene_value_def_t step0_vals[] = {
        { .fixture = "par1", .channel = "dimmer", .value = 255 },
        { .fixture = "par1", .channel = "red",    .value = 200 },
    };
    spark_scene_value_def_t step1_vals[] = {
        { .fixture = "par1", .channel = "dimmer", .value = 0 },
    };

    spark_scene_step_def_t steps[] = {
        { .duration_ms = 100, .values = step0_vals, .value_count = 2 },
        { .duration_ms = 100, .values = step1_vals, .value_count = 1 },
    };

    spark_scene_def_t def = {
        .channel = 0, .note = 51,
        .id = "seq-fix", .name = "Seq Fixture",
        .enabled = true,
        .trigger_mode = SPARK_SCENE_GATE,
        .output_mode = SPARK_SCENE_SEQUENCE,
        .steps = steps, .step_count = 2,
        .loop = false,
    };

    ASSERT_EQ(spark_scene_add_def(&def), 0);
    ASSERT_EQ(spark_scene_resolve(), 0);

    spark_scene_t *scene = spark_scene_get(0, 51);
    ASSERT_EQ(scene->output.step_count, 2);
    ASSERT_EQ(scene->output.steps[0].value_count, 2);
    ASSERT_EQ(scene->output.steps[0].values[0].dmx_index, 4);
    ASSERT_EQ(scene->output.steps[0].values[1].dmx_index, 5);
    ASSERT_EQ(scene->output.steps[1].value_count, 1);
    ASSERT_EQ(scene->output.steps[1].values[0].dmx_index, 4);
}

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);
    TEST_BEGIN();
    RUN_TEST(test_scene_activate_basic);
    RUN_TEST(test_scene_activate_already_active);
    RUN_TEST(test_scene_deactivate_basic);
    RUN_TEST(test_scene_deactivate_not_active);
    RUN_TEST(test_scene_toggle_on_off);
    RUN_TEST(test_scene_multiple_active);
    RUN_TEST(test_scene_reset_clears_all);
    RUN_TEST(test_resolve_static_raw);
    RUN_TEST(test_resolve_multiple_scenes);
    RUN_TEST(test_resolve_unresolved_skipped);
    RUN_TEST(test_resolve_empty_defs);
    RUN_TEST(test_resolve_reset_clears_arena);
    RUN_TEST(test_resolve_fixture_channel);
    RUN_TEST(test_resolve_fixture_not_found);
    RUN_TEST(test_resolve_channel_not_found);
    RUN_TEST(test_resolve_mixed_raw_and_fixture);
    RUN_TEST(test_resolve_sequence_basic);
    RUN_TEST(test_resolve_sequence_fixture);
    TEST_END();
}
