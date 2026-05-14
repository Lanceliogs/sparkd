#include "test.h"

void test_true_is_true(void)
{
    ASSERT_TRUE(1);
}

void test_basic_equality(void)
{
    ASSERT_EQ(2 + 2, 4);
}

void test_string_match(void)
{
    ASSERT_STR_EQ("spark", "spark");
}

int main(void)
{
    TEST_BEGIN();
    RUN_TEST(test_true_is_true);
    RUN_TEST(test_basic_equality);
    RUN_TEST(test_string_match);
    TEST_END();
}
