#include "test.h"
#include "clock.h"

void test_monotonic_only_increases(void)
{
    uint64_t first = spark_clock_monotonic_ms();
    uint64_t second = spark_clock_monotonic_ms();
    ASSERT_TRUE(first <= second);
}

void test_sleep_elapsed(void)
{
    uint64_t start = spark_clock_monotonic_ms();
    spark_clock_msleep(50);
    uint64_t elapsed = spark_clock_monotonic_ms() - start;
    ASSERT_TRUE(elapsed >= 35);
    ASSERT_TRUE(elapsed <= 65);
}

int main(void)
{
    TEST_BEGIN();
    RUN_TEST(test_monotonic_only_increases);
    RUN_TEST(test_sleep_elapsed);
    TEST_END();
}
