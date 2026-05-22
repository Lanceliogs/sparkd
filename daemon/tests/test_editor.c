#include "test.h"
#include "editor.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

static editor_state_t state;

static void reset(void)
{
    free(state.project.raw_buf);
    memset(&state, 0, sizeof(state));
    state.project.loaded = true;
}

/* ---- Fixture CRUD ---- */

void test_fixture_add(void)
{
    reset();
    editor_fixture_t fix = {0};
    strncpy(fix.id, "par1", SPARK_MAX_ID_SIZE - 1);
    strncpy(fix.name, "PAR Light", SPARK_MAX_NAME_SIZE - 1);
    fix.start_address = 1;

    ASSERT_EQ(editor_fixture_add(&state, &fix), 0);
    ASSERT_EQ(state.project.fixture_count, 1);
    ASSERT_TRUE(state.project.dirty);
    ASSERT_STR_EQ(state.project.fixtures[0].id, "par1");
    ASSERT_STR_EQ(state.project.fixtures[0].name, "PAR Light");
    ASSERT_EQ(state.project.fixtures[0].start_address, 1);
}

void test_fixture_add_multiple(void)
{
    reset();
    editor_fixture_t fix = {0};

    strncpy(fix.id, "a", SPARK_MAX_ID_SIZE - 1);
    ASSERT_EQ(editor_fixture_add(&state, &fix), 0);

    memset(&fix, 0, sizeof(fix));
    strncpy(fix.id, "b", SPARK_MAX_ID_SIZE - 1);
    ASSERT_EQ(editor_fixture_add(&state, &fix), 0);

    memset(&fix, 0, sizeof(fix));
    strncpy(fix.id, "c", SPARK_MAX_ID_SIZE - 1);
    ASSERT_EQ(editor_fixture_add(&state, &fix), 0);

    ASSERT_EQ(state.project.fixture_count, 3);
    ASSERT_STR_EQ(state.project.fixtures[0].id, "a");
    ASSERT_STR_EQ(state.project.fixtures[1].id, "b");
    ASSERT_STR_EQ(state.project.fixtures[2].id, "c");
}

void test_fixture_add_overflow(void)
{
    reset();
    editor_fixture_t fix = {0};
    strncpy(fix.id, "x", SPARK_MAX_ID_SIZE - 1);

    for (int i = 0; i < EDITOR_MAX_FIXTURES; i++)
        ASSERT_EQ(editor_fixture_add(&state, &fix), 0);

    ASSERT_EQ(editor_fixture_add(&state, &fix), -1);
    ASSERT_EQ(state.project.fixture_count, EDITOR_MAX_FIXTURES);
}

void test_fixture_update(void)
{
    reset();
    editor_fixture_t fix = {0};
    strncpy(fix.id, "orig", SPARK_MAX_ID_SIZE - 1);
    fix.start_address = 5;
    editor_fixture_add(&state, &fix);
    state.project.dirty = false;

    editor_fixture_t updated = {0};
    strncpy(updated.id, "renamed", SPARK_MAX_ID_SIZE - 1);
    updated.start_address = 10;

    ASSERT_EQ(editor_fixture_update(&state, 0, &updated), 0);
    ASSERT_STR_EQ(state.project.fixtures[0].id, "renamed");
    ASSERT_EQ(state.project.fixtures[0].start_address, 10);
    ASSERT_TRUE(state.project.dirty);
}

void test_fixture_update_bounds(void)
{
    reset();
    editor_fixture_t fix = {0};
    ASSERT_EQ(editor_fixture_update(&state, 0, &fix), -1);
    ASSERT_EQ(editor_fixture_update(&state, -1, &fix), -1);
    ASSERT_EQ(editor_fixture_update(&state, 999, &fix), -1);
}

void test_fixture_remove(void)
{
    reset();
    editor_fixture_t fix = {0};

    strncpy(fix.id, "a", SPARK_MAX_ID_SIZE - 1);
    editor_fixture_add(&state, &fix);
    memset(&fix, 0, sizeof(fix));
    strncpy(fix.id, "b", SPARK_MAX_ID_SIZE - 1);
    editor_fixture_add(&state, &fix);
    memset(&fix, 0, sizeof(fix));
    strncpy(fix.id, "c", SPARK_MAX_ID_SIZE - 1);
    editor_fixture_add(&state, &fix);
    state.project.dirty = false;

    ASSERT_EQ(editor_fixture_remove(&state, 1), 0);
    ASSERT_EQ(state.project.fixture_count, 2);
    ASSERT_STR_EQ(state.project.fixtures[0].id, "a");
    ASSERT_STR_EQ(state.project.fixtures[1].id, "c");
    ASSERT_TRUE(state.project.dirty);
}

void test_fixture_remove_first(void)
{
    reset();
    editor_fixture_t fix = {0};

    strncpy(fix.id, "a", SPARK_MAX_ID_SIZE - 1);
    editor_fixture_add(&state, &fix);
    memset(&fix, 0, sizeof(fix));
    strncpy(fix.id, "b", SPARK_MAX_ID_SIZE - 1);
    editor_fixture_add(&state, &fix);

    ASSERT_EQ(editor_fixture_remove(&state, 0), 0);
    ASSERT_EQ(state.project.fixture_count, 1);
    ASSERT_STR_EQ(state.project.fixtures[0].id, "b");
}

void test_fixture_remove_last(void)
{
    reset();
    editor_fixture_t fix = {0};

    strncpy(fix.id, "a", SPARK_MAX_ID_SIZE - 1);
    editor_fixture_add(&state, &fix);
    memset(&fix, 0, sizeof(fix));
    strncpy(fix.id, "b", SPARK_MAX_ID_SIZE - 1);
    editor_fixture_add(&state, &fix);

    ASSERT_EQ(editor_fixture_remove(&state, 1), 0);
    ASSERT_EQ(state.project.fixture_count, 1);
    ASSERT_STR_EQ(state.project.fixtures[0].id, "a");
}

void test_fixture_remove_bounds(void)
{
    reset();
    ASSERT_EQ(editor_fixture_remove(&state, 0), -1);
    ASSERT_EQ(editor_fixture_remove(&state, -1), -1);
}

/* ---- Bank fixture CRUD ---- */

void test_bank_fixture_add(void)
{
    reset();
    state.bank_count = 1;
    strncpy(state.banks[0].id, "test-bank", SPARK_MAX_ID_SIZE - 1);

    editor_bank_fixture_t bfix = {0};
    strncpy(bfix.id, "dimmer", SPARK_MAX_ID_SIZE - 1);
    strncpy(bfix.name, "Generic Dimmer", SPARK_MAX_NAME_SIZE - 1);
    bfix.channel_count = 1;

    ASSERT_EQ(editor_bank_fixture_add(&state, 0, &bfix), 0);
    ASSERT_EQ(state.banks[0].fixture_count, 1);
    ASSERT_TRUE(state.banks[0].dirty);
    ASSERT_STR_EQ(state.banks[0].fixtures[0].id, "dimmer");
}

void test_bank_fixture_add_invalid_bank(void)
{
    reset();
    editor_bank_fixture_t bfix = {0};
    ASSERT_EQ(editor_bank_fixture_add(&state, 0, &bfix), -1);
    ASSERT_EQ(editor_bank_fixture_add(&state, -1, &bfix), -1);
}

void test_bank_fixture_update(void)
{
    reset();
    state.bank_count = 1;

    editor_bank_fixture_t bfix = {0};
    strncpy(bfix.id, "orig", SPARK_MAX_ID_SIZE - 1);
    editor_bank_fixture_add(&state, 0, &bfix);
    state.banks[0].dirty = false;

    editor_bank_fixture_t updated = {0};
    strncpy(updated.id, "updated", SPARK_MAX_ID_SIZE - 1);
    updated.channel_count = 4;

    ASSERT_EQ(editor_bank_fixture_update(&state, 0, 0, &updated), 0);
    ASSERT_STR_EQ(state.banks[0].fixtures[0].id, "updated");
    ASSERT_EQ(state.banks[0].fixtures[0].channel_count, 4);
    ASSERT_TRUE(state.banks[0].dirty);
}

void test_bank_fixture_update_bounds(void)
{
    reset();
    state.bank_count = 1;
    editor_bank_fixture_t bfix = {0};
    ASSERT_EQ(editor_bank_fixture_update(&state, 0, 0, &bfix), -1);
    ASSERT_EQ(editor_bank_fixture_update(&state, 0, -1, &bfix), -1);
}

void test_bank_fixture_remove(void)
{
    reset();
    state.bank_count = 1;

    editor_bank_fixture_t bfix = {0};
    strncpy(bfix.id, "a", SPARK_MAX_ID_SIZE - 1);
    editor_bank_fixture_add(&state, 0, &bfix);
    memset(&bfix, 0, sizeof(bfix));
    strncpy(bfix.id, "b", SPARK_MAX_ID_SIZE - 1);
    editor_bank_fixture_add(&state, 0, &bfix);
    memset(&bfix, 0, sizeof(bfix));
    strncpy(bfix.id, "c", SPARK_MAX_ID_SIZE - 1);
    editor_bank_fixture_add(&state, 0, &bfix);

    ASSERT_EQ(editor_bank_fixture_remove(&state, 0, 1), 0);
    ASSERT_EQ(state.banks[0].fixture_count, 2);
    ASSERT_STR_EQ(state.banks[0].fixtures[0].id, "a");
    ASSERT_STR_EQ(state.banks[0].fixtures[1].id, "c");
}

void test_bank_fixture_remove_bounds(void)
{
    reset();
    state.bank_count = 1;
    ASSERT_EQ(editor_bank_fixture_remove(&state, 0, 0), -1);
    ASSERT_EQ(editor_bank_fixture_remove(&state, -1, 0), -1);
}

/* ---- Lifecycle ---- */

void test_close_clears_state(void)
{
    reset();
    editor_fixture_t fix = {0};
    strncpy(fix.id, "test", SPARK_MAX_ID_SIZE - 1);
    editor_fixture_add(&state, &fix);

    state.project.raw_buf = (char *)malloc(128);
    state.project.raw_buf_len = 128;

    editor_close_project(&state);

    ASSERT_TRUE(!state.project.loaded);
    ASSERT_EQ(state.project.fixture_count, 0);
    ASSERT_TRUE(state.project.raw_buf == NULL);
    ASSERT_EQ(state.project.raw_buf_len, 0);
}

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);

    TEST_BEGIN();
    RUN_TEST(test_fixture_add);
    RUN_TEST(test_fixture_add_multiple);
    RUN_TEST(test_fixture_add_overflow);
    RUN_TEST(test_fixture_update);
    RUN_TEST(test_fixture_update_bounds);
    RUN_TEST(test_fixture_remove);
    RUN_TEST(test_fixture_remove_first);
    RUN_TEST(test_fixture_remove_last);
    RUN_TEST(test_fixture_remove_bounds);
    RUN_TEST(test_bank_fixture_add);
    RUN_TEST(test_bank_fixture_add_invalid_bank);
    RUN_TEST(test_bank_fixture_update);
    RUN_TEST(test_bank_fixture_update_bounds);
    RUN_TEST(test_bank_fixture_remove);
    RUN_TEST(test_bank_fixture_remove_bounds);
    RUN_TEST(test_close_clears_state);
    TEST_END();
}
