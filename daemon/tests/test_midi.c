#include "test.h"
#include "log.h"
#include "midi.h"
#include "midi_event.h"
#include "portmidi.h"

#include <unistd.h>

/* Defined in midi.c — not part of public API, tested here for coverage */
extern void spark_midi_decode_pm_event(PmEvent *event, spark_midi_event_t *out);

static int portmidi_available = 0;

void test_init_destroy(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    int rc = spark_midi_init();
    if (rc != 0) { ASSERT_TRUE(1); return; }
    spark_midi_destroy();
    ASSERT_TRUE(1);
}

void test_init_twice(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    if (spark_midi_init() != 0) { ASSERT_TRUE(1); return; }
    ASSERT_EQ(spark_midi_init(), 0);
    spark_midi_destroy();
}

void test_destroy_without_init(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    spark_midi_destroy();
    ASSERT_TRUE(1);
}

void test_find_device_garbage(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    if (spark_midi_init() != 0) { ASSERT_TRUE(1); return; }
    int id = spark_midi_find_device("__nonexistent_device_sparkd_test__");
    ASSERT_EQ(id, -1);
    spark_midi_destroy();
}

void test_open_invalid_device(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    if (spark_midi_init() != 0) { ASSERT_TRUE(1); return; }
    int rc = spark_midi_open(9999);
    ASSERT_TRUE(rc < 0);
    spark_midi_destroy();
}

void test_close_without_open(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    if (spark_midi_init() != 0) { ASSERT_TRUE(1); return; }
    spark_midi_close(9999);
    ASSERT_TRUE(1);
    spark_midi_destroy();
}

void test_close_all_empty(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    if (spark_midi_init() != 0) { ASSERT_TRUE(1); return; }
    spark_midi_close_all();
    ASSERT_TRUE(1);
    spark_midi_destroy();
}

void test_poll_no_streams(void)
{
    if (!portmidi_available) { ASSERT_TRUE(1); return; }
    if (spark_midi_init() != 0) { ASSERT_TRUE(1); return; }
    spark_midi_event_t events[8];
    int count = spark_midi_poll(events, 8);
    ASSERT_EQ(count, 0);
    spark_midi_destroy();
}

void test_decode_note_on(void)
{
    PmEvent ev = { .message = Pm_Message(0x93, 60, 100), .timestamp = 0 };
    spark_midi_event_t out = {0};
    spark_midi_decode_pm_event(&ev, &out);
    ASSERT_EQ(out.type, SPARK_MIDI_NOTE_ON);
    ASSERT_EQ(out.channel, 3);
    ASSERT_EQ(out.note, 60);
    ASSERT_EQ(out.velocity, 100);
}

void test_decode_note_on_velocity_zero(void)
{
    PmEvent ev = { .message = Pm_Message(0x90, 64, 0), .timestamp = 0 };
    spark_midi_event_t out = {0};
    spark_midi_decode_pm_event(&ev, &out);
    ASSERT_EQ(out.type, SPARK_MIDI_NOTE_OFF);
    ASSERT_EQ(out.channel, 0);
    ASSERT_EQ(out.note, 64);
    ASSERT_EQ(out.velocity, 0);
}

void test_decode_note_off(void)
{
    PmEvent ev = { .message = Pm_Message(0x85, 48, 64), .timestamp = 0 };
    spark_midi_event_t out = {0};
    spark_midi_decode_pm_event(&ev, &out);
    ASSERT_EQ(out.type, SPARK_MIDI_NOTE_OFF);
    ASSERT_EQ(out.channel, 5);
    ASSERT_EQ(out.note, 48);
    ASSERT_EQ(out.velocity, 64);
}

void test_decode_cc(void)
{
    PmEvent ev = { .message = Pm_Message(0xB2, 7, 112), .timestamp = 0 };
    spark_midi_event_t out = {0};
    spark_midi_decode_pm_event(&ev, &out);
    ASSERT_EQ(out.type, SPARK_MIDI_CC);
    ASSERT_EQ(out.channel, 2);
    ASSERT_EQ(out.cc, 7);
    ASSERT_EQ(out.value, 112);
}

void test_decode_unknown_status(void)
{
    PmEvent ev = { .message = Pm_Message(0xF2, 0, 0), .timestamp = 0 };
    spark_midi_event_t out = {0};
    spark_midi_decode_pm_event(&ev, &out);
    ASSERT_EQ(out.type, SPARK_MIDI_NOTE_OFF);
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
    RUN_TEST(test_decode_note_on);
    RUN_TEST(test_decode_note_on_velocity_zero);
    RUN_TEST(test_decode_note_off);
    RUN_TEST(test_decode_cc);
    RUN_TEST(test_decode_unknown_status);
    RUN_TEST(test_init_destroy);
    RUN_TEST(test_init_twice);
    RUN_TEST(test_destroy_without_init);
    RUN_TEST(test_find_device_garbage);
    RUN_TEST(test_open_invalid_device);
    RUN_TEST(test_close_without_open);
    RUN_TEST(test_close_all_empty);
    RUN_TEST(test_poll_no_streams);
    TEST_END();
}
