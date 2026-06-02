#include "test.h"
#include "../src/auth.h"
#include "../src/log.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
static void test_setenv(const char *key, const char *val) { _putenv_s(key, val); }
static void test_unsetenv(const char *key) { _putenv_s(key, ""); }
#else
static void test_setenv(const char *key, const char *val) { setenv(key, val, 1); }
static void test_unsetenv(const char *key) { unsetenv(key); }
#endif

#define TEST_TOKEN "my-secret-infrastructure-token-1234"

static void reset_env(void)
{
    test_unsetenv("SPARK_AUTH_TOKEN");
    test_unsetenv("SPARK_UI_TOKEN");
}

void test_auth_disabled_by_default(void)
{
    reset_env();
    spark_auth_init();
    ASSERT_EQ(spark_auth_enabled(), 0);
}

void test_auth_disabled_allows_all(void)
{
    reset_env();
    spark_auth_init();
    ASSERT_EQ(spark_auth_check_static("anything", 8), 1);
    ASSERT_EQ(spark_auth_check_static("", 0), 1);
}

void test_auth_enabled_with_token(void)
{
    reset_env();
    test_setenv("SPARK_AUTH_TOKEN", TEST_TOKEN);
    spark_auth_init();
    ASSERT_EQ(spark_auth_enabled(), 1);
}

void test_check_static_correct(void)
{
    reset_env();
    test_setenv("SPARK_AUTH_TOKEN", TEST_TOKEN);
    spark_auth_init();
    ASSERT_EQ(spark_auth_check_static(TEST_TOKEN, strlen(TEST_TOKEN)), 1);
}

void test_check_static_wrong_token(void)
{
    reset_env();
    test_setenv("SPARK_AUTH_TOKEN", TEST_TOKEN);
    spark_auth_init();
    ASSERT_EQ(spark_auth_check_static("wrong-token-value", 17), 0);
}

void test_check_static_wrong_length(void)
{
    reset_env();
    test_setenv("SPARK_AUTH_TOKEN", TEST_TOKEN);
    spark_auth_init();
    /* Subset */
    ASSERT_EQ(spark_auth_check_static(TEST_TOKEN, 5), 0);
    /* Superset */
    ASSERT_EQ(spark_auth_check_static(TEST_TOKEN "extra", strlen(TEST_TOKEN) + 5), 0);
}

void test_ui_init_generates_token(void)
{
    reset_env();
    spark_auth_ui_init();
    const char *tok = spark_auth_ui_token();
    ASSERT_TRUE(tok != NULL);
    ASSERT_EQ(strlen(tok), SPARK_AUTH_UUID_LEN);
    /* UUID v4 format: 8-4-4-4-12 */
    ASSERT_EQ(tok[8], '-');
    ASSERT_EQ(tok[13], '-');
    ASSERT_EQ(tok[18], '-');
    ASSERT_EQ(tok[23], '-');
}

void test_ui_init_respects_env(void)
{
    reset_env();
    test_setenv("SPARK_UI_TOKEN", "custom-ui-token-from-env");
    spark_auth_ui_init();
    ASSERT_STR_EQ(spark_auth_ui_token(), "custom-ui-token-from-env");
}

void test_check_any_static_returns_admin(void)
{
    reset_env();
    test_setenv("SPARK_AUTH_TOKEN", TEST_TOKEN);
    spark_auth_init();
    spark_auth_ui_init();
    ASSERT_EQ(spark_auth_check_any(TEST_TOKEN, strlen(TEST_TOKEN)), SPARK_ROLE_ADMIN);
}

void test_check_any_ui_returns_live(void)
{
    reset_env();
    test_setenv("SPARK_AUTH_TOKEN", TEST_TOKEN);
    spark_auth_init();
    spark_auth_ui_init();
    const char *ui_tok = spark_auth_ui_token();
    ASSERT_EQ(spark_auth_check_any(ui_tok, strlen(ui_tok)), SPARK_ROLE_LIVE);
}

void test_check_any_garbage_returns_none(void)
{
    reset_env();
    test_setenv("SPARK_AUTH_TOKEN", TEST_TOKEN);
    spark_auth_init();
    spark_auth_ui_init();
    ASSERT_EQ(spark_auth_check_any("totally-bogus", 13), SPARK_ROLE_NONE);
    ASSERT_EQ(spark_auth_check_any("", 0), SPARK_ROLE_NONE);
}

void test_check_any_ui_works_without_static(void)
{
    reset_env();
    spark_auth_init();
    spark_auth_ui_init();
    ASSERT_EQ(spark_auth_enabled(), 0);
    const char *ui_tok = spark_auth_ui_token();
    ASSERT_EQ(spark_auth_check_any(ui_tok, strlen(ui_tok)), SPARK_ROLE_LIVE);
}

void test_ui_rotate_invalidates_old(void)
{
    reset_env();
    test_setenv("SPARK_AUTH_TOKEN", TEST_TOKEN);
    spark_auth_init();
    spark_auth_ui_init();

    const char *old_tok = spark_auth_ui_token();
    char old_copy[SPARK_AUTH_UUID_LEN + 1];
    snprintf(old_copy, sizeof(old_copy), "%s", old_tok);

    spark_auth_ui_rotate();

    const char *new_tok = spark_auth_ui_token();
    ASSERT_TRUE(strcmp(old_copy, new_tok) != 0);
    ASSERT_EQ(spark_auth_check_any(old_copy, strlen(old_copy)), SPARK_ROLE_NONE);
    ASSERT_EQ(spark_auth_check_any(new_tok, strlen(new_tok)), SPARK_ROLE_LIVE);
}

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);

    TEST_BEGIN();
    RUN_TEST(test_auth_disabled_by_default);
    RUN_TEST(test_auth_disabled_allows_all);
    RUN_TEST(test_auth_enabled_with_token);
    RUN_TEST(test_check_static_correct);
    RUN_TEST(test_check_static_wrong_token);
    RUN_TEST(test_check_static_wrong_length);
    RUN_TEST(test_ui_init_generates_token);
    RUN_TEST(test_ui_init_respects_env);
    RUN_TEST(test_check_any_static_returns_admin);
    RUN_TEST(test_check_any_ui_returns_live);
    RUN_TEST(test_check_any_garbage_returns_none);
    RUN_TEST(test_check_any_ui_works_without_static);
    RUN_TEST(test_ui_rotate_invalidates_old);
    TEST_END();
}
