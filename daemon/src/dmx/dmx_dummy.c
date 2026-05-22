#include "dmx.h"
#include "spark_atomic.h"

static int s_open(spark_dmx_backend_t *backend)
{
    spark_atomic_store(&backend->state, SPARK_DMX_CONNECTED);
    spark_atomic_inc(&backend->reconnects);
    return 0;
}

static void s_close(spark_dmx_backend_t *backend)
{
    spark_atomic_store(&backend->state, SPARK_DMX_DISCONNECTED);
}

static int s_send_frame(spark_dmx_backend_t *backend, const uint8_t frame[SPARK_DMX_UNIVERSE_SIZE])
{
    (void)frame;
    if (spark_atomic_load(&backend->state) != SPARK_DMX_CONNECTED)
        return -1;
    spark_atomic_inc(&backend->frames_sent);
    return 0;
}

static bool s_is_connected(spark_dmx_backend_t *backend)
{
    return spark_atomic_load(&backend->state) == SPARK_DMX_CONNECTED;
}

static const spark_dmx_ops_t s_dummy_ops = {
    .open = s_open,
    .close = s_close,
    .send_frame = s_send_frame,
    .is_connected = s_is_connected,
};

void spark_dmx_dummy_init(spark_dmx_backend_t *backend)
{
    spark_atomic_store(&backend->state, SPARK_DMX_DISCONNECTED);
    spark_dmx_reset_stats(backend);
    backend->ops = &s_dummy_ops;
}