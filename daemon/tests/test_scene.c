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
    TEST_END();
}
