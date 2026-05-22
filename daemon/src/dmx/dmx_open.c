#include "dmx.h"
#include "spark_atomic.h"
#include "log.h"
#include "clock.h"
#include "serial/serial.h"

#include <string.h>

/* Open DMX backend implementation */
typedef struct {
    spark_serial_t serial;
} dmx_open_priv_t;

static dmx_open_priv_t s_dmx_open_priv;

static int s_open(spark_dmx_backend_t *backend)
{
    dmx_open_priv_t *priv = backend->priv;
    if (spark_serial_is_open(&priv->serial))
    {
        spark_log_warn("dmx_open:open: Serial port already opened");
        return -1;
    }
    spark_log_debug("dmx_open:open: Opening port '%s'", priv->serial.port);
    if (spark_serial_open(&priv->serial) != 0)
    {
        spark_log_warn("dmx_open:open: Can't open serial port");
        return -1;
    }
    spark_atomic_store(&backend->state, SPARK_DMX_CONNECTED);
    spark_atomic_inc(&backend->reconnects);
    spark_log_debug("dmx_open:open: Connected (reconnects=%llu)",
                    spark_atomic_load_u64(&backend->reconnects));
    return 0;
}

static void s_close(spark_dmx_backend_t *backend)
{
    dmx_open_priv_t *priv = backend->priv;
    if (!spark_serial_is_open(&priv->serial))
        return;
    spark_serial_close(&priv->serial);
    spark_atomic_store(&backend->state, SPARK_DMX_DISCONNECTED);
}

static int s_send_frame(spark_dmx_backend_t *backend, const uint8_t frame[SPARK_DMX_UNIVERSE_SIZE])
{
    if (spark_atomic_load(&backend->state) != SPARK_DMX_CONNECTED)
        return -1;
 
    dmx_open_priv_t *priv = backend->priv;
        
    spark_serial_set_break(&priv->serial, true);
    spark_clock_usleep(100);
    spark_serial_set_break(&priv->serial, false);
    spark_clock_usleep(12);
    uint8_t buffer[SPARK_DMX_UNIVERSE_SIZE + 1];
    buffer[0] = 0;
    memcpy(buffer+1, frame, SPARK_DMX_UNIVERSE_SIZE);
    int written = spark_serial_write(&priv->serial, buffer, sizeof(buffer));
    
    if (written < 0)
    {
        spark_atomic_inc(&backend->write_errors);
        spark_log_debug("dmx_open:send_frame: write failed, errors=%llu",
                        spark_atomic_load_u64(&backend->write_errors));
        return -1;
    }

    spark_atomic_inc(&backend->frames_sent);
    uint64_t frames_sent = spark_atomic_load_u64(&backend->frames_sent);
    if (frames_sent % 40 == 1)
        spark_log_debug("dmx_open:send_frame: frame #%llu, wrote %d bytes, first 8 ch: [%u %u %u %u %u %u %u %u]",
                        frames_sent, written,
                        frame[0], frame[1], frame[2], frame[3],
                        frame[4], frame[5], frame[6], frame[7]);
    return 0;
}

static bool s_is_connected(spark_dmx_backend_t *backend)
{
    return spark_atomic_load(&backend->state) == SPARK_DMX_CONNECTED;
}

static const spark_dmx_ops_t s_open_ops = {
    .open = s_open,
    .close = s_close,
    .send_frame = s_send_frame,
    .is_connected = s_is_connected,
};

void spark_dmx_open_init(spark_dmx_backend_t *backend, const char *port)
{
    backend->priv = &s_dmx_open_priv;
    dmx_open_priv_t *priv = backend->priv;

    spark_serial_init(&priv->serial);
    spark_serial_configure(
        &priv->serial,
        port,
        SPARK_SERIAL_BAUDRATE_250000,
        SPARK_SERIAL_DATA_BITS_8,
        SPARK_SERIAL_STOP_BIT_2,
        SPARK_SERIAL_PARITY_NONE
    );

    spark_atomic_store(&backend->state, SPARK_DMX_DISCONNECTED);
    spark_dmx_reset_stats(backend);
    backend->ops = &s_open_ops;
}