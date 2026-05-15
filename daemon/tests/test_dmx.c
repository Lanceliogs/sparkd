#include "test.h"
#include "dmx/dmx.h"

void test_dummy_lifecycle(void)
{
    spark_dmx_backend_t backend;
    spark_dmx_dummy_init(&backend);

    ASSERT_EQ(spark_dmx_is_connected(&backend), 0);
    ASSERT_EQ(backend.frames_sent, 0);
    ASSERT_EQ(backend.write_errors, 0);
    ASSERT_EQ(backend.reconnects, 0);

    ASSERT_EQ(spark_dmx_open(&backend), 0);
    ASSERT_TRUE(spark_dmx_is_connected(&backend));

    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    memset(frame, 0, SPARK_DMX_UNIVERSE_SIZE);
    
    spark_dmx_send_frame(&backend, frame);
    spark_dmx_send_frame(&backend, frame);
    spark_dmx_send_frame(&backend, frame);
    ASSERT_EQ(backend.frames_sent, 3);

    spark_dmx_close(&backend);
    ASSERT_TRUE(!spark_dmx_is_connected(&backend));
}

int main(void)
{
    TEST_BEGIN();
    RUN_TEST(test_dummy_lifecycle);
    TEST_END();
}