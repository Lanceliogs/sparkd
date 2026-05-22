/*
 * dmx.h - DMX backend interface and state machine
 *
 * Defines a vtable-based DMX backend abstraction. All backends (dummy,
 * Open DMX, Pro Packet Serial) implement the same ops: open, close,
 * send_frame, is_connected.
 *
 * The backend struct tracks connection state (disconnected/connecting/
 * connected/error) and output statistics (frames sent, write errors,
 * reconnect count).
 *
 * Backend-specific data lives behind the void *priv pointer.
 * The DMX output thread drives the reconnect state machine.
 */
#ifndef SPARK_DMX_H
#define SPARK_DMX_H

#include "consts.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>

typedef enum {
    SPARK_DMX_BACKEND_DUMMY,
    SPARK_DMX_BACKEND_OPEN,
} spark_dmx_backend_type_t;

typedef enum
{
    SPARK_DMX_DISCONNECTED,
    SPARK_DMX_CONNECTING,
    SPARK_DMX_CONNECTED,
    SPARK_DMX_ERROR
} spark_dmx_state_t;

typedef struct spark_dmx_backend spark_dmx_backend_t;

typedef struct {
    int  (*open)(spark_dmx_backend_t *backend);
    void (*close)(spark_dmx_backend_t *backend);
    int  (*send_frame)(spark_dmx_backend_t *backend, const uint8_t frame[SPARK_DMX_UNIVERSE_SIZE]);
    bool  (*is_connected)(spark_dmx_backend_t *backend);
} spark_dmx_ops_t;

struct spark_dmx_backend {
    const spark_dmx_ops_t *ops; /* pointer to the vtable */
    _Atomic spark_dmx_state_t state; /* current connection state */
    /* stats */
    _Atomic uint64_t frames_sent;
    _Atomic uint64_t write_errors;
    _Atomic uint64_t reconnects;
    /* backend-private data (each backend can cast this) */
    void *priv;
};

/* Common functions */

void spark_dmx_reset_stats(spark_dmx_backend_t *backend);

/* Less verbose aliases */
int spark_dmx_open(spark_dmx_backend_t *backend);
void spark_dmx_close(spark_dmx_backend_t *backend);
int spark_dmx_send_frame(spark_dmx_backend_t *backend, const uint8_t frame[SPARK_DMX_UNIVERSE_SIZE]);
bool spark_dmx_is_connected(spark_dmx_backend_t *backend);

/* Specific backends below */

/* Dummy DMX */
void spark_dmx_dummy_init(spark_dmx_backend_t *backend);

/* Open DMX */
void spark_dmx_open_init(spark_dmx_backend_t *backend, const char *port);

/* Pro DMX */

#endif