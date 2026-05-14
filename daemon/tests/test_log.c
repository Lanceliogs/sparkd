#include "test.h"
#include "log.h"

void test_default_level_is_info(void)
{
    spark_log_init(SPARK_LOG_INFO);
    ASSERT_EQ(spark_log_get_level(), SPARK_LOG_INFO);
}

void test_set_and_get_level(void)
{
    spark_log_set_level(SPARK_LOG_DEBUG);
    ASSERT_EQ(spark_log_get_level(), SPARK_LOG_DEBUG);

    spark_log_set_level(SPARK_LOG_ERROR);
    ASSERT_EQ(spark_log_get_level(), SPARK_LOG_ERROR);
}

void test_level_from_string(void)
{
    spark_log_level_t level;

    ASSERT_EQ(spark_log_level_from_string("debug", &level), 0);
    ASSERT_EQ(level, SPARK_LOG_DEBUG);

    ASSERT_EQ(spark_log_level_from_string("info", &level), 0);
    ASSERT_EQ(level, SPARK_LOG_INFO);

    ASSERT_EQ(spark_log_level_from_string("warn", &level), 0);
    ASSERT_EQ(level, SPARK_LOG_WARN);

    ASSERT_EQ(spark_log_level_from_string("error", &level), 0);
    ASSERT_EQ(level, SPARK_LOG_ERROR);
}

void test_level_from_string_invalid(void)
{
    spark_log_level_t level;
    ASSERT_EQ(spark_log_level_from_string("garbage", &level), -1);
    ASSERT_EQ(spark_log_level_from_string("", &level), -1);
}

void test_level_to_string(void)
{
    ASSERT_STR_EQ(spark_log_level_to_string(SPARK_LOG_DEBUG), "DEBUG");
    ASSERT_STR_EQ(spark_log_level_to_string(SPARK_LOG_INFO),  "INFO");
    ASSERT_STR_EQ(spark_log_level_to_string(SPARK_LOG_WARN),  "WARN");
    ASSERT_STR_EQ(spark_log_level_to_string(SPARK_LOG_ERROR), "ERROR");
}

int main(void)
{
    TEST_BEGIN();
    RUN_TEST(test_default_level_is_info);
    RUN_TEST(test_set_and_get_level);
    RUN_TEST(test_level_from_string);
    RUN_TEST(test_level_from_string_invalid);
    RUN_TEST(test_level_to_string);
    TEST_END();
}
