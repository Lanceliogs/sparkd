#include "test.h"

/* Pull in sparkctl.c's static functions by renaming its main */
#define main sparkctl_main
#include "../tools/sparkctl.c"
#undef main

void test_normalize_addr_zero_with_port(void)
{
    char addr[64];
    snprintf(addr, sizeof(addr), "0.0.0.0:7600");
    s_normalize_addr(addr, sizeof(addr));
    ASSERT_STR_EQ(addr, "127.0.0.1:7600");
}

void test_normalize_addr_zero_no_port(void)
{
    char addr[64];
    snprintf(addr, sizeof(addr), "0.0.0.0");
    s_normalize_addr(addr, sizeof(addr));
    ASSERT_STR_EQ(addr, "127.0.0.1");
}

void test_normalize_addr_already_loopback(void)
{
    char addr[64];
    snprintf(addr, sizeof(addr), "127.0.0.1:7600");
    s_normalize_addr(addr, sizeof(addr));
    ASSERT_STR_EQ(addr, "127.0.0.1:7600");
}

void test_normalize_addr_lan_ip(void)
{
    char addr[64];
    snprintf(addr, sizeof(addr), "192.168.0.33:7600");
    s_normalize_addr(addr, sizeof(addr));
    ASSERT_STR_EQ(addr, "192.168.0.33:7600");
}

void test_normalize_addr_zero_high_port(void)
{
    char addr[64];
    snprintf(addr, sizeof(addr), "0.0.0.0:65535");
    s_normalize_addr(addr, sizeof(addr));
    ASSERT_STR_EQ(addr, "127.0.0.1:65535");
}

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);

    TEST_BEGIN();
    RUN_TEST(test_normalize_addr_zero_with_port);
    RUN_TEST(test_normalize_addr_zero_no_port);
    RUN_TEST(test_normalize_addr_already_loopback);
    RUN_TEST(test_normalize_addr_lan_ip);
    RUN_TEST(test_normalize_addr_zero_high_port);
    TEST_END();
}
