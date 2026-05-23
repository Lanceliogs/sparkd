#include "test.h"
#include "log.h"
#include "serial/serial.h"

#include <string.h>

void test_enumerate_no_crash(void)
{
    spark_serial_device_info_t devices[SPARK_SERIAL_MAX_DEVICES];
    int count = spark_serial_enumerate(devices, SPARK_SERIAL_MAX_DEVICES);

    ASSERT_TRUE(count >= 0);
    ASSERT_TRUE(count <= SPARK_SERIAL_MAX_DEVICES);
}

void test_enumerate_zero_max(void)
{
    spark_serial_device_info_t dev;
    int count = spark_serial_enumerate(&dev, 0);
    ASSERT_EQ(count, 0);
}

void test_find_dmx_no_device(void)
{
    spark_serial_device_info_t info;
    memset(&info, 0, sizeof(info));

    /* Likely no DMX device in CI — should return -1 gracefully */
    int rc = spark_serial_find_dmx("nonexistent_manufacturer_xyz", &info);
    ASSERT_EQ(rc, -1);
}

void test_is_known_dmx_ftdi(void)
{
    ASSERT_TRUE(spark_serial_is_known_dmx(0x0403, 0x6001, NULL));
    ASSERT_TRUE(spark_serial_is_known_dmx(0x0403, 0x6014, NULL));
    ASSERT_TRUE(spark_serial_is_known_dmx(0x0403, 0x6015, NULL));
}

void test_is_known_dmx_with_tag(void)
{
    ASSERT_TRUE(spark_serial_is_known_dmx(0x0403, 0x6001, "ftdi"));
    ASSERT_TRUE(spark_serial_is_known_dmx(0x0403, 0x6001, "enttec"));
    ASSERT_TRUE(spark_serial_is_known_dmx(0x0403, 0x6001, "eurolite"));
}

void test_is_known_dmx_wrong_tag(void)
{
    ASSERT_TRUE(!spark_serial_is_known_dmx(0x0403, 0x6001, "nonexistent"));
}

void test_is_known_dmx_wrong_vid_pid(void)
{
    ASSERT_TRUE(!spark_serial_is_known_dmx(0x1234, 0x5678, NULL));
    ASSERT_TRUE(!spark_serial_is_known_dmx(0x0403, 0x9999, NULL));
    ASSERT_TRUE(!spark_serial_is_known_dmx(0x0000, 0x0000, NULL));
}

void test_is_known_dmx_tag_filter(void)
{
    /* FT232H (0x6014) is only tagged "ftdi", not "enttec" */
    ASSERT_TRUE(spark_serial_is_known_dmx(0x0403, 0x6014, "ftdi"));
    ASSERT_TRUE(!spark_serial_is_known_dmx(0x0403, 0x6014, "enttec"));
}

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);

    TEST_BEGIN();
    RUN_TEST(test_enumerate_no_crash);
    RUN_TEST(test_enumerate_zero_max);
    RUN_TEST(test_find_dmx_no_device);
    RUN_TEST(test_is_known_dmx_ftdi);
    RUN_TEST(test_is_known_dmx_with_tag);
    RUN_TEST(test_is_known_dmx_wrong_tag);
    RUN_TEST(test_is_known_dmx_wrong_vid_pid);
    RUN_TEST(test_is_known_dmx_tag_filter);
    TEST_END();
}
