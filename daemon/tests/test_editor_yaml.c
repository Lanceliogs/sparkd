#include "test.h"
#include "editor.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROJECT_PATH "../projects/example/project.spark.yaml"
#define BANK_PATH    "../projects/bank/stairville.yaml"
#define TMP_OUT      "/tmp/sparkd_test_emit.yaml"
#define TMP_RT       "/tmp/sparkd_test_roundtrip.yaml"
#define TMP_BANK_OUT "/tmp/sparkd_test_bank.yaml"
#define TMP_NEW      "/tmp/sparkd_test_new.yaml"

static editor_project_t project;
static editor_state_t state;

static void reset_project(void)
{
    free(project.raw_buf);
    memset(&project, 0, sizeof(project));
}

static void reset_state(void)
{
    free(state.project.raw_buf);
    memset(&state, 0, sizeof(state));
}

static int file_contains(const char *path, const char *needle)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    fread(buf, 1, (size_t)sz, f);
    buf[sz] = '\0';
    fclose(f);
    int found = strstr(buf, needle) != NULL;
    free(buf);
    return found;
}

static int files_equal(const char *a, const char *b)
{
    FILE *fa = fopen(a, "rb");
    FILE *fb = fopen(b, "rb");
    if (!fa || !fb) { if (fa) fclose(fa); if (fb) fclose(fb); return 0; }

    fseek(fa, 0, SEEK_END); long sa = ftell(fa); fseek(fa, 0, SEEK_SET);
    fseek(fb, 0, SEEK_END); long sb = ftell(fb); fseek(fb, 0, SEEK_SET);
    if (sa != sb) { fclose(fa); fclose(fb); return 0; }

    char *ba = (char *)malloc((size_t)sa);
    char *bb = (char *)malloc((size_t)sb);
    fread(ba, 1, (size_t)sa, fa);
    fread(bb, 1, (size_t)sb, fb);
    fclose(fa);
    fclose(fb);

    int eq = memcmp(ba, bb, (size_t)sa) == 0;
    free(ba);
    free(bb);
    return eq;
}

/* ---- Project parse tests ---- */

void test_parse_project(void)
{
    reset_project();
    ASSERT_EQ(editor_yaml_parse_project(PROJECT_PATH, &project), 0);
    ASSERT_TRUE(project.loaded);
    ASSERT_EQ(project.fixture_count, 1);
    ASSERT_STR_EQ(project.fixtures[0].id, "par");
    ASSERT_STR_EQ(project.fixtures[0].name, "Stairville PAR");
    ASSERT_EQ(project.fixtures[0].start_address, 1);
    ASSERT_STR_EQ(project.fixtures[0].template_ref, "stairville:par");
    free(project.raw_buf);
    project.raw_buf = NULL;
}

void test_parse_project_raw_sections(void)
{
    reset_project();
    ASSERT_EQ(editor_yaml_parse_project(PROJECT_PATH, &project), 0);
    ASSERT_EQ(project.raw_section_count, 5);
    ASSERT_STR_EQ(project.raw_sections[0].key, "format");
    ASSERT_STR_EQ(project.raw_sections[1].key, "app");
    ASSERT_STR_EQ(project.raw_sections[2].key, "midi");
    ASSERT_STR_EQ(project.raw_sections[3].key, "dmx");
    ASSERT_STR_EQ(project.raw_sections[4].key, "scenes");
    ASSERT_TRUE(project.raw_buf != NULL);
    ASSERT_TRUE(project.raw_buf_len > 0);
    free(project.raw_buf);
    project.raw_buf = NULL;
}

void test_parse_project_raw_section_content(void)
{
    reset_project();
    ASSERT_EQ(editor_yaml_parse_project(PROJECT_PATH, &project), 0);

    /* Verify the raw slice for 'app' contains "name: example" */
    editor_raw_section_t *app = &project.raw_sections[1];
    ASSERT_TRUE(app->len > 0);
    char *slice = (char *)malloc(app->len + 1);
    memcpy(slice, project.raw_buf + app->start, app->len);
    slice[app->len] = '\0';
    ASSERT_TRUE(strstr(slice, "name: example") != NULL);
    free(slice);

    free(project.raw_buf);
    project.raw_buf = NULL;
}

void test_parse_nonexistent(void)
{
    reset_project();
    ASSERT_EQ(editor_yaml_parse_project("/nonexistent/path.yaml", &project), -1);
    ASSERT_TRUE(!project.loaded);
}

/* ---- Project emit tests ---- */

void test_emit_preserves_sections(void)
{
    reset_project();
    ASSERT_EQ(editor_yaml_parse_project(PROJECT_PATH, &project), 0);
    ASSERT_EQ(editor_yaml_emit_project(TMP_OUT, &project), 0);

    ASSERT_TRUE(file_contains(TMP_OUT, "format:"));
    ASSERT_TRUE(file_contains(TMP_OUT, "app:"));
    ASSERT_TRUE(file_contains(TMP_OUT, "midi:"));
    ASSERT_TRUE(file_contains(TMP_OUT, "dmx:"));
    ASSERT_TRUE(file_contains(TMP_OUT, "scenes:"));
    ASSERT_TRUE(file_contains(TMP_OUT, "fixtures:"));
    ASSERT_TRUE(file_contains(TMP_OUT, "red-wash"));
    ASSERT_TRUE(file_contains(TMP_OUT, "color-cycle"));

    free(project.raw_buf);
    project.raw_buf = NULL;
}

void test_emit_roundtrip_stable(void)
{
    reset_project();
    ASSERT_EQ(editor_yaml_parse_project(PROJECT_PATH, &project), 0);
    ASSERT_EQ(editor_yaml_emit_project(TMP_OUT, &project), 0);
    free(project.raw_buf);
    project.raw_buf = NULL;

    /* Second roundtrip */
    reset_project();
    ASSERT_EQ(editor_yaml_parse_project(TMP_OUT, &project), 0);
    ASSERT_EQ(editor_yaml_emit_project(TMP_RT, &project), 0);
    free(project.raw_buf);
    project.raw_buf = NULL;

    ASSERT_TRUE(files_equal(TMP_OUT, TMP_RT));
}

void test_emit_no_raw_buf(void)
{
    reset_project();
    project.loaded = true;
    strncpy(project.path, TMP_NEW, EDITOR_PATH_MAX - 1);

    editor_fixture_t fix = {0};
    strncpy(fix.id, "test-fix", SPARK_MAX_ID_SIZE - 1);
    strncpy(fix.name, "Test Fixture", SPARK_MAX_NAME_SIZE - 1);
    fix.start_address = 1;
    fix.channel_count = 2;
    strncpy(fix.channels[0].name, "dimmer", SPARK_MAX_NAME_SIZE - 1);
    fix.channels[0].offset = 0;
    strncpy(fix.channels[1].name, "strobe", SPARK_MAX_NAME_SIZE - 1);
    fix.channels[1].offset = 1;

    project.fixtures[0] = fix;
    project.fixture_count = 1;

    ASSERT_EQ(editor_yaml_emit_project(TMP_NEW, &project), 0);
    ASSERT_TRUE(file_contains(TMP_NEW, "format:"));
    ASSERT_TRUE(file_contains(TMP_NEW, "spark-project"));
    ASSERT_TRUE(file_contains(TMP_NEW, "fixtures:"));
    ASSERT_TRUE(file_contains(TMP_NEW, "test-fix"));
    ASSERT_TRUE(file_contains(TMP_NEW, "dimmer"));
}

void test_emit_preserves_fixture_data(void)
{
    reset_project();
    ASSERT_EQ(editor_yaml_parse_project(PROJECT_PATH, &project), 0);
    ASSERT_EQ(editor_yaml_emit_project(TMP_OUT, &project), 0);

    ASSERT_TRUE(file_contains(TMP_OUT, "id: par"));
    ASSERT_TRUE(file_contains(TMP_OUT, "name: Stairville PAR"));
    ASSERT_TRUE(file_contains(TMP_OUT, "start-address: 1"));
    ASSERT_TRUE(file_contains(TMP_OUT, "template: \"stairville:par\""));

    free(project.raw_buf);
    project.raw_buf = NULL;
}

/* ---- Bank parse/emit tests ---- */

void test_parse_bank(void)
{
    editor_bank_t bank = {0};
    ASSERT_EQ(editor_yaml_parse_bank(BANK_PATH, &bank), 0);
    ASSERT_STR_EQ(bank.id, "stairville");
    ASSERT_EQ(bank.version, 1);
    ASSERT_EQ(bank.fixture_count, 2);
    ASSERT_STR_EQ(bank.fixtures[0].id, "par");
    ASSERT_EQ(bank.fixtures[0].channel_count, 8);
    ASSERT_STR_EQ(bank.fixtures[1].id, "switch-4ch");
    ASSERT_EQ(bank.fixtures[1].channel_count, 4);
}

void test_parse_bank_fixture_channels(void)
{
    editor_bank_t bank = {0};
    ASSERT_EQ(editor_yaml_parse_bank(BANK_PATH, &bank), 0);

    ASSERT_STR_EQ(bank.fixtures[0].channels[0].name, "dimmer");
    ASSERT_EQ(bank.fixtures[0].channels[0].offset, 0);
    ASSERT_STR_EQ(bank.fixtures[0].channels[1].name, "red");
    ASSERT_EQ(bank.fixtures[0].channels[1].offset, 1);
    ASSERT_STR_EQ(bank.fixtures[0].channels[7].name, "strobe");
    ASSERT_EQ(bank.fixtures[0].channels[7].offset, 7);
}

void test_emit_bank_roundtrip(void)
{
    editor_bank_t bank = {0};
    ASSERT_EQ(editor_yaml_parse_bank(BANK_PATH, &bank), 0);
    ASSERT_EQ(editor_yaml_emit_bank(TMP_BANK_OUT, &bank), 0);

    editor_bank_t bank2 = {0};
    ASSERT_EQ(editor_yaml_parse_bank(TMP_BANK_OUT, &bank2), 0);

    ASSERT_STR_EQ(bank2.id, bank.id);
    ASSERT_EQ(bank2.version, bank.version);
    ASSERT_EQ(bank2.fixture_count, bank.fixture_count);

    for (int i = 0; i < bank.fixture_count; i++)
    {
        ASSERT_STR_EQ(bank2.fixtures[i].id, bank.fixtures[i].id);
        ASSERT_STR_EQ(bank2.fixtures[i].name, bank.fixtures[i].name);
        ASSERT_EQ(bank2.fixtures[i].channel_count, bank.fixtures[i].channel_count);
    }
}

/* ---- Integration: open + edit + save ---- */

void test_open_edit_save_preserves(void)
{
    reset_state();
    ASSERT_EQ(editor_open_project(&state, PROJECT_PATH), 0);

    /* Add a fixture */
    editor_fixture_t fix = {0};
    strncpy(fix.id, "new-light", SPARK_MAX_ID_SIZE - 1);
    fix.start_address = 20;
    editor_fixture_add(&state, &fix);

    /* Save to temp */
    strncpy(state.project.path, TMP_OUT, EDITOR_PATH_MAX - 1);
    ASSERT_EQ(editor_save_project(&state), 0);

    /* Verify preserved sections still exist */
    ASSERT_TRUE(file_contains(TMP_OUT, "scenes:"));
    ASSERT_TRUE(file_contains(TMP_OUT, "midi:"));
    ASSERT_TRUE(file_contains(TMP_OUT, "dmx:"));
    /* Verify both fixtures are there */
    ASSERT_TRUE(file_contains(TMP_OUT, "id: par"));
    ASSERT_TRUE(file_contains(TMP_OUT, "id: new-light"));

    editor_close_project(&state);
}

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);

    TEST_BEGIN();
    RUN_TEST(test_parse_project);
    RUN_TEST(test_parse_project_raw_sections);
    RUN_TEST(test_parse_project_raw_section_content);
    RUN_TEST(test_parse_nonexistent);
    RUN_TEST(test_emit_preserves_sections);
    RUN_TEST(test_emit_roundtrip_stable);
    RUN_TEST(test_emit_no_raw_buf);
    RUN_TEST(test_emit_preserves_fixture_data);
    RUN_TEST(test_parse_bank);
    RUN_TEST(test_parse_bank_fixture_channels);
    RUN_TEST(test_emit_bank_roundtrip);
    RUN_TEST(test_open_edit_save_preserves);
    TEST_END();
}
