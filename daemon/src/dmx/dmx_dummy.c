#include "dmx.h"

static int spark_dmx_dummy_open(spark_dmx_backend_t *backend)
{
    backend->state = SPARK_DMX_CONNECTED;
    return 0;
}

static void spark_dmx_dummy_close(spark_dmx_backend_t *backend)
{
    backend->state = SPARK_DMX_DISCONNECTED;
}

static int spark_dmx_dummy_send_frame(spark_dmx_backend_t *backend, const uint8_t frame[SPARK_DMX_UNIVERSE_SIZE])
{
    (void)frame;
    backend->frames_sent++;
    return 0;
}

static int spark_dmx_dummy_is_connected(spark_dmx_backend_t *backend)
{
    return backend->state == SPARK_DMX_CONNECTED;
}

static const spark_dmx_ops_t dummy_ops = {
    .open = spark_dmx_dummy_open,
    .close = spark_dmx_dummy_close,
    .send_frame = spark_dmx_dummy_send_frame,
    .is_connected = spark_dmx_dummy_is_connected,
};

void spark_dmx_dummy_init(spark_dmx_backend_t *backend)
{
    backend->state = SPARK_DMX_DISCONNECTED;
    spark_dmx_reset_stats(backend);
    backend->ops = &dummy_ops;
}