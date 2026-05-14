#include "dmx.h"

void spark_dmx_reset_stats(spark_dmx_backend_t *backend)
{
    backend->frames_sent = 0;
    backend->write_errors = 0;
    backend->reconnects = 0;
}