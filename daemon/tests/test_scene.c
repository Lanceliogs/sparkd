#include "test.h"
#include "scene.h"
#include "log.h"

void test_scene_activate_basic(void)
{
    spark_scene_reset();

    spark_scene_t *scene = spark_scene_get(0, 60);
    scene->id = "test";
    scene->enabled = true;

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
    scene->id = "test";
    scene->enabled = true;

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
    scene->id = "test";
    scene->enabled = true;

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
    scene->id = "test";
    scene->enabled = true;

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
    scene->id = "test";
    scene->enabled = true;

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

    spark_scene_t *s1 = spark_scene_get(0, 70);
    s1->id = "scene-1";
    s1->enabled = true;

    spark_scene_t *s2 = spark_scene_get(0, 71);
    s2->id = "scene-2";
    s2->enabled = true;

    spark_scene_t *s3 = spark_scene_get(0, 72);
    s3->id = "scene-3";
    s3->enabled = true;

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
    scene->id = "test";
    scene->enabled = true;
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

    spark_scene_def_t defs[] = {
        {
            .channel = 0, .note = 60,
            .id = "test-raw", .name = "Test Raw",
            .enabled = true,
            .trigger_mode = SPARK_SCENE_GATE,
            .output_mode = SPARK_SCENE_STATIC,
            .values = vals, .value_count = 2,
        },
    };

    ASSERT_EQ(spark_scene_resolve(defs, 1), 0);

    spark_scene_t *scene = spark_scene_get(0, 60);
    ASSERT_STR_EQ(scene->id, "test-raw");
    ASSERT_TRUE(scene->enabled);
    ASSERT_EQ(scene->trigger_mode, SPARK_SCENE_GATE);
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

    spark_scene_def_t defs[] = {
        {
            .channel = 0, .note = 60, .id = "a", .name = "A",
            .enabled = true, .trigger_mode = SPARK_SCENE_GATE,
            .output_mode = SPARK_SCENE_STATIC,
            .values = vals_a, .value_count = 1,
        },
        {
            .channel = 1, .note = 48, .id = "b", .name = "B",
            .enabled = true, .trigger_mode = SPARK_SCENE_TOGGLE,
            .output_mode = SPARK_SCENE_STATIC,
            .values = vals_b, .value_count = 2,
        },
    };

    ASSERT_EQ(spark_scene_resolve(defs, 2), 0);

    spark_scene_t *sa = spark_scene_get(0, 60);
    ASSERT_STR_EQ(sa->id, "a");
    ASSERT_EQ(sa->output.value_count, 1);

    spark_scene_t *sb = spark_scene_get(1, 48);
    ASSERT_STR_EQ(sb->id, "b");
    ASSERT_EQ(sb->trigger_mode, SPARK_SCENE_TOGGLE);
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

    spark_scene_def_t defs[] = {
        {
            .channel = 0, .note = 70, .id = "partial", .name = "Partial",
            .enabled = true, .trigger_mode = SPARK_SCENE_GATE,
            .output_mode = SPARK_SCENE_STATIC,
            .values = vals, .value_count = 3,
        },
    };

    ASSERT_EQ(spark_scene_resolve(defs, 1), 0);

    spark_scene_t *scene = spark_scene_get(0, 70);
    ASSERT_EQ(scene->output.value_count, 2);
    ASSERT_EQ(scene->output.values[0].dmx_index, 0);
    ASSERT_EQ(scene->output.values[1].dmx_index, 2);
}

void test_resolve_empty_defs(void)
{
    spark_scene_reset();
    ASSERT_EQ(spark_scene_resolve(NULL, 0), 0);
}

void test_resolve_reset_clears_arena(void)
{
    spark_scene_reset();

    spark_scene_value_def_t vals[] = {
        { .dmx_index = 0, .value = 255 },
    };
    spark_scene_def_t defs[] = {
        {
            .channel = 0, .note = 60, .id = "x", .name = "X",
            .enabled = true, .trigger_mode = SPARK_SCENE_GATE,
            .output_mode = SPARK_SCENE_STATIC,
            .values = vals, .value_count = 1,
        },
    };

    ASSERT_EQ(spark_scene_resolve(defs, 1), 0);
    spark_scene_t *scene = spark_scene_get(0, 60);
    ASSERT_EQ(scene->output.value_count, 1);

    spark_scene_reset();
    scene = spark_scene_get(0, 60);
    ASSERT_EQ(scene->output.value_count, 0);

    ASSERT_EQ(spark_scene_resolve(defs, 1), 0);
    scene = spark_scene_get(0, 60);
    ASSERT_EQ(scene->output.value_count, 1);
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
    TEST_END();
}
