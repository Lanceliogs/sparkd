#include "dmx.h"

void spark_dmx_reset_stats(spark_dmx_backend_t *backend)
{
    backend->frames_sent = 0;
    backend->write_errors = 0;
    backend->reconnects = 0;
}

int spark_dmx_open(spark_dmx_backend_t *backend)
{
    return backend->ops->open(backend);
}

void spark_dmx_close(spark_dmx_backend_t *backend)
{
    backend->ops->close(backend);
}

int spark_dmx_send_frame(spark_dmx_backend_t *backend, const uint8_t frame[SPARK_DMX_UNIVERSE_SIZE])
{
    return backend->ops->send_frame(backend, frame);
}

bool spark_dmx_is_connected(spark_dmx_backend_t *backend)
{
    return backend->ops->is_connected(backend);
}
