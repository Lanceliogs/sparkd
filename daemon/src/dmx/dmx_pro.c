#include "dmx.h"
#include "spark_atomic.h"
#include "log.h"
#include "serial/serial.h"

#include <string.h>

/*
 * Enttec DMX USB Pro Widget API
 *
 * Communication uses a framed message protocol over serial (57600 baud 8N1):
 *
 *   [START] [LABEL] [LEN_LO] [LEN_HI] [DATA...] [END]
 *
 * START = 0x7E, END = 0xE7
 * LABEL identifies the message type.
 * LEN is little-endian 16-bit data length (0 if no payload).
 *
 * Key labels:
 *   6 = Output Only Send DMX Packet (host → widget)
 *       Data: [start_code] [ch1] [ch2] ... [chN]  (max 513 bytes: 1 start + 512)
 *   3 = Get Widget Parameters Request (host → widget, len=0)
 *   3 = Get Widget Parameters Reply (widget → host)
 *  10 = Get Widget Serial Number Request (host → widget, len=0)
 *  10 = Get Widget Serial Number Reply (widget → host, 4 bytes LE)
 *
 * The VCP baud rate is a dummy value — the FTDI chip handles USB
 * communication internally. We use 250000 (same as Open DMX) since
 * all FTDI-based devices auto-baud and the setting is ignored.
 */

#define DMX_PRO_START_BYTE 0x7E
#define DMX_PRO_END_BYTE   0xE7

#define DMX_PRO_LABEL_GET_PARAMS    3
#define DMX_PRO_LABEL_SET_PARAMS    4
#define DMX_PRO_LABEL_SEND_DMX      6
#define DMX_PRO_LABEL_RECEIVE_DMX   5
#define DMX_PRO_LABEL_GET_SERIAL   10

/* Max frame: header(4) + start_code(1) + 512 channels + footer(1) = 518 */
#define DMX_PRO_FRAME_MAX (4 + 1 + SPARK_DMX_UNIVERSE_SIZE + 1)

typedef struct {
    spark_serial_t serial;
    uint8_t tx_buf[DMX_PRO_FRAME_MAX];
} dmx_pro_priv_t;

static dmx_pro_priv_t s_dmx_pro_priv;

static int s_send_message(dmx_pro_priv_t *priv, uint8_t label,
                          const uint8_t *data, uint16_t len)
{
    uint8_t header[4];
    header[0] = DMX_PRO_START_BYTE;
    header[1] = label;
    header[2] = (uint8_t)(len & 0xFF);
    header[3] = (uint8_t)(len >> 8);

    if (spark_serial_write(&priv->serial, header, 4) < 0)
        return -1;

    if (len > 0 && data)
    {
        if (spark_serial_write(&priv->serial, data, len) < 0)
            return -1;
    }

    uint8_t footer = DMX_PRO_END_BYTE;
    if (spark_serial_write(&priv->serial, &footer, 1) < 0)
        return -1;

    return 0;
}

static int s_open(spark_dmx_backend_t *backend)
{
    dmx_pro_priv_t *priv = backend->priv;
    if (spark_serial_is_open(&priv->serial))
    {
        spark_log_warn("dmx_pro:open: serial port already opened");
        return -1;
    }

    spark_log_debug("dmx_pro:open: opening port '%s'", priv->serial.port);
    if (spark_serial_open(&priv->serial) != 0)
    {
        spark_log_warn("dmx_pro:open: can't open serial port");
        return -1;
    }

    /* Send a Get Widget Parameters request to verify the device is a Pro.
     * We don't wait for a response here — the first send_frame will confirm
     * connectivity. This just wakes the widget up. */
    s_send_message(priv, DMX_PRO_LABEL_GET_PARAMS, NULL, 0);

    spark_atomic_store(&backend->state, SPARK_DMX_CONNECTED);
    spark_atomic_inc(&backend->reconnects);
    spark_log_debug("dmx_pro:open: connected (reconnects=%llu)",
                    spark_atomic_load_u64(&backend->reconnects));
    return 0;
}

static void s_close(spark_dmx_backend_t *backend)
{
    dmx_pro_priv_t *priv = backend->priv;
    if (!spark_serial_is_open(&priv->serial))
        return;
    spark_serial_close(&priv->serial);
    spark_atomic_store(&backend->state, SPARK_DMX_DISCONNECTED);
}

static int s_send_frame(spark_dmx_backend_t *backend, const uint8_t frame[SPARK_DMX_UNIVERSE_SIZE])
{
    if (spark_atomic_load(&backend->state) != SPARK_DMX_CONNECTED)
        return -1;

    dmx_pro_priv_t *priv = backend->priv;

    /* Label 6: Send DMX Packet
     * Data = [start_code=0x00] [512 channel values]
     * Length = 513 */
    uint8_t dmx_data[SPARK_DMX_UNIVERSE_SIZE + 1];
    dmx_data[0] = 0x00;
    memcpy(dmx_data + 1, frame, SPARK_DMX_UNIVERSE_SIZE);

    uint16_t len = SPARK_DMX_UNIVERSE_SIZE + 1;
    int rc = s_send_message(priv, DMX_PRO_LABEL_SEND_DMX, dmx_data, len);

    if (rc < 0)
    {
        spark_atomic_inc(&backend->write_errors);
        spark_log_debug("dmx_pro:send_frame: write failed, errors=%llu",
                        spark_atomic_load_u64(&backend->write_errors));
        return -1;
    }

    spark_atomic_inc(&backend->frames_sent);
    uint64_t frames_sent = spark_atomic_load_u64(&backend->frames_sent);
    if (frames_sent % 40 == 1)
        spark_log_debug("dmx_pro:send_frame: frame #%llu, first 8 ch: [%u %u %u %u %u %u %u %u]",
                        frames_sent,
                        frame[0], frame[1], frame[2], frame[3],
                        frame[4], frame[5], frame[6], frame[7]);
    return 0;
}

static bool s_is_connected(spark_dmx_backend_t *backend)
{
    return spark_atomic_load(&backend->state) == SPARK_DMX_CONNECTED;
}

static const spark_dmx_ops_t s_pro_ops = {
    .open = s_open,
    .close = s_close,
    .send_frame = s_send_frame,
    .is_connected = s_is_connected,
};

void spark_dmx_pro_init(spark_dmx_backend_t *backend, const char *port)
{
    backend->priv = &s_dmx_pro_priv;
    dmx_pro_priv_t *priv = backend->priv;

    spark_serial_init(&priv->serial);
    spark_serial_configure(
        &priv->serial,
        port,
        SPARK_SERIAL_BAUDRATE_DMX,
        SPARK_SERIAL_DATA_BITS_8,
        SPARK_SERIAL_STOP_BIT_2,
        SPARK_SERIAL_PARITY_NONE
    );

    spark_atomic_store(&backend->state, SPARK_DMX_DISCONNECTED);
    spark_dmx_reset_stats(backend);
    backend->ops = &s_pro_ops;
}
