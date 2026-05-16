#include "dmx.h"
#include "log.h"
#include "clock.h"
#include "serial/serial.h"

#include <string.h>

/* Open DMX backend implementation */
typedef struct {
    spark_serial_t serial;
} dmx_open_priv_t;

static dmx_open_priv_t dmx_open_priv;

static int spark_dmx_open_open(spark_dmx_backend_t *backend)
{
    dmx_open_priv_t *priv = backend->priv;
    if (spark_serial_is_open(&priv->serial))
    {
        spark_log_warn("dmx_open:open: Serial port already opened");
        return -1;
    }
    if (spark_serial_open(&priv->serial) != 0)
    {
        spark_log_warn("dmx_open:open: Can't open serial port");
        return -1;
    }
    backend->state = SPARK_DMX_CONNECTED;
    backend->reconnects++;
    return 0;
}

static void spark_dmx_open_close(spark_dmx_backend_t *backend)
{
    dmx_open_priv_t *priv = backend->priv;
    if (!spark_serial_is_open(&priv->serial))
        return;
    spark_serial_close(&priv->serial);
    backend->state = SPARK_DMX_DISCONNECTED;
}

static int spark_dmx_open_send_frame(spark_dmx_backend_t *backend, const uint8_t frame[SPARK_DMX_UNIVERSE_SIZE])
{
    if (backend->state != SPARK_DMX_CONNECTED)
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
        backend->write_errors++;
        return -1;
    }

    backend->frames_sent++;
    return 0;
}

static bool spark_dmx_open_is_connected(spark_dmx_backend_t *backend)
{
    return backend->state == SPARK_DMX_CONNECTED;
}

static const spark_dmx_ops_t open_ops = {
    .open = spark_dmx_open_open,
    .close = spark_dmx_open_close,
    .send_frame = spark_dmx_open_send_frame,
    .is_connected = spark_dmx_open_is_connected,
};

void spark_dmx_open_init(spark_dmx_backend_t *backend, const char *port)
{
    backend->priv = &dmx_open_priv;
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

    backend->state = SPARK_DMX_DISCONNECTED;
    spark_dmx_reset_stats(backend);
    backend->ops = &open_ops;
}