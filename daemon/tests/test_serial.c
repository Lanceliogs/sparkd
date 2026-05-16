#include "test.h"
#include "log.h"
#include "serial/serial.h"

#include <string.h>

void test_init_defaults(void)
{
    spark_serial_t serial;
    spark_serial_init(&serial);

    ASSERT_EQ(serial.baudrate, 0);
    ASSERT_EQ(serial.data_bits, SPARK_SERIAL_DATA_BITS_5);
    ASSERT_EQ(serial.stop_bit, SPARK_SERIAL_STOP_BIT_1);
    ASSERT_EQ(serial.parity, SPARK_SERIAL_PARITY_NONE);
    ASSERT_EQ(serial.flow_control, SPARK_SERIAL_FLOW_CONTROL_NONE);
    ASSERT_EQ(serial.port[0], '\0');
    ASSERT_TRUE(!spark_serial_is_open(&serial));
}

void test_is_open_after_init(void)
{
    spark_serial_t serial;
    spark_serial_init(&serial);

    ASSERT_TRUE(!spark_serial_is_open(&serial));
}

void test_open_nonexistent_device(void)
{
    spark_serial_t serial;
    spark_serial_init(&serial);

    const char *port = "/dev/ttyNONEXISTENT_sparkd_test";
    spark_serial_configure(
        &serial,
        port,
        250000,
        SPARK_SERIAL_DATA_BITS_8,
        SPARK_SERIAL_STOP_BIT_2,
        SPARK_SERIAL_PARITY_NONE
    );
    ASSERT_STR_EQ(serial.port, port);
    ASSERT_EQ(serial.baudrate, 250000);

    int rc = spark_serial_open(&serial);
    ASSERT_EQ(rc, -1);
    ASSERT_TRUE(!spark_serial_is_open(&serial));
}

void test_close_without_open(void)
{
    spark_serial_t serial;
    spark_serial_init(&serial);

    /* should not crash */
    spark_serial_close(&serial);
    ASSERT_TRUE(!spark_serial_is_open(&serial));
}

void test_port_string_stored(void)
{
    spark_serial_t serial;
    spark_serial_init(&serial);

    strncpy(serial.port, "/dev/ttyUSB0", SPARK_SERIAL_PORT_STRLEN - 1);
    ASSERT_STR_EQ(serial.port, "/dev/ttyUSB0");
}

int main(void)
{
    spark_log_init(SPARK_LOG_SILENT);

    TEST_BEGIN();
    RUN_TEST(test_init_defaults);
    RUN_TEST(test_is_open_after_init);
    RUN_TEST(test_open_nonexistent_device);
    RUN_TEST(test_close_without_open);
    RUN_TEST(test_port_string_stored);
    TEST_END();
}
