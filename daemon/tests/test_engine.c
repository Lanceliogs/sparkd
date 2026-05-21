#include "test.h"
#include "log.h"
#include "engine.h"
#include "scene.h"

#include <string.h>

#ifdef __linux__
#include <unistd.h>
#endif

static int portmidi_available = 0;

void test_init_destroy(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_engine_init(), 0);
    spark_engine_destroy();
}

void test_init_twice(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_engine_init(), 0);
    ASSERT_EQ(spark_engine_init(), 0);
    spark_engine_destroy();
}

void test_destroy_without_init(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    spark_engine_destroy();
    ASSERT_TRUE(1);
}

void test_start_stop_dummy(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_engine_init(), 0);
    ASSERT_EQ(spark_engine_load_project(NULL), 0);
    ASSERT_EQ(spark_engine_start(), 0);
    spark_engine_stop();
    spark_engine_destroy();
}

void test_start_without_init(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_engine_start(), -1);
}

void test_stop_without_start(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_engine_init(), 0);
    spark_engine_stop();
    spark_engine_destroy();
}

void test_double_start(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_engine_init(), 0);
    ASSERT_EQ(spark_engine_load_project(NULL), 0);
    ASSERT_EQ(spark_engine_start(), 0);
    ASSERT_EQ(spark_engine_start(), 0);
    spark_engine_stop();
    spark_engine_destroy();
}

void test_process_events_not_running(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_engine_init(), 0);
    spark_engine_process_events();
    spark_engine_destroy();
    ASSERT_TRUE(1);
}

void test_process_events_running(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_engine_init(), 0);
    ASSERT_EQ(spark_engine_load_project(NULL), 0);
    ASSERT_EQ(spark_engine_start(), 0);
    spark_engine_process_events();
    spark_engine_stop();
    spark_engine_destroy();
    ASSERT_TRUE(1);
}

void test_full_lifecycle(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_engine_init(), 0);
    ASSERT_EQ(spark_engine_load_project(NULL), 0);
    ASSERT_EQ(spark_engine_start(), 0);

    for (int i = 0; i < 5; i++)
        spark_engine_process_events();

    spark_engine_stop();
    spark_engine_destroy();
    ASSERT_TRUE(1);
}

void test_restart(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_engine_init(), 0);
    ASSERT_EQ(spark_engine_load_project(NULL), 0);
    ASSERT_EQ(spark_engine_start(), 0);
    spark_engine_stop();
    ASSERT_EQ(spark_engine_start(), 0);
    spark_engine_stop();
    spark_engine_destroy();
    ASSERT_TRUE(1);
}

void test_midi_reconnect_not_initialized(void)
{
    ASSERT_EQ(spark_engine_midi_reconnect(), -1);
}

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);

#ifdef __linux__
    portmidi_available = (access("/dev/snd/seq", F_OK) == 0);
#else
    portmidi_available = 1;
#endif

    TEST_BEGIN();
    RUN_TEST(test_init_destroy);
    RUN_TEST(test_init_twice);
    RUN_TEST(test_destroy_without_init);
    RUN_TEST(test_start_stop_dummy);
    RUN_TEST(test_start_without_init);
    RUN_TEST(test_stop_without_start);
    RUN_TEST(test_double_start);
    RUN_TEST(test_process_events_not_running);
    RUN_TEST(test_process_events_running);
    RUN_TEST(test_full_lifecycle);
    RUN_TEST(test_restart);
    RUN_TEST(test_midi_reconnect_not_initialized);
    TEST_END();
}
