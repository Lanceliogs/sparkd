#include "dmx_out.h"
#include "clock.h"

#include <string.h>

#define DMX_OUT_DEFAULT_REFRESH_RATE 40

void spark_dmx_out_init(spark_dmx_out_t *out, spark_dmx_backend_t *backend, spark_stage_t *stage)
{
    memset(out, 0, sizeof(*out));
    out->backend = backend;
    out->stage = stage;
    out->refresh_rate_hz = DMX_OUT_DEFAULT_REFRESH_RATE;
    out->retry_ms_current = 1000 / DMX_OUT_DEFAULT_REFRESH_RATE;
    out->retry_ms_max = 5000;
}

static void handle_reconnect(spark_dmx_out_t *out)
{
    if (spark_dmx_open(out->backend) == 0)
                out->retry_ms_current = 1000 / out->refresh_rate_hz;
    else
    {
        out->retry_ms_current *= 2;
        if (out->retry_ms_current > out->retry_ms_max)
            out->retry_ms_current = out->retry_ms_max;
    }
    spark_clock_msleep(out->retry_ms_current);
}

static void wait_for_next_frame(spark_dmx_out_t *out, uint64_t start)
{
    uint32_t period = 1000 / out->refresh_rate_hz;
    uint64_t elapsed = spark_clock_monotonic_ms() - start;
    if (elapsed < period)
        spark_clock_msleep(period - elapsed);
}

static void dmx_thread_loop(spark_dmx_out_t *out)
{
    uint64_t start;
    while (out->running)
    {
        start = spark_clock_monotonic_ms();
        if (!spark_dmx_is_connected(out->backend))
        {
            handle_reconnect(out);
            continue;
        }
        spark_stage_render(out->stage, out->frame);
        if (spark_dmx_send_frame(out->backend, out->frame) != 0)
            spark_dmx_close(out->backend);
        wait_for_next_frame(out, start);
    }

    if (spark_dmx_is_connected(out->backend))
    {
        /* Zero the frame and render one last time */
        spark_stage_set_blackout(out->stage, true);
        spark_stage_render(out->stage, out->frame);
        spark_dmx_send_frame(out->backend, out->frame);
    }
}

static void *dmx_thread_loop_wrapper(void *arg)
{
    dmx_thread_loop((spark_dmx_out_t *)arg);
    return NULL;
}

int spark_dmx_out_start(spark_dmx_out_t *out)
{
    out->running = 1;
    return pthread_create(&out->thread, NULL, dmx_thread_loop_wrapper, out);
}

int spark_dmx_out_stop(spark_dmx_out_t *out)
{
    out->running = 0;
    return pthread_join(out->thread, NULL);
}
