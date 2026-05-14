#ifndef SPARK_DMX_H
#define SPARK_DMX_H

#include "consts.h"

#include <stdint.h>

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
    int  (*is_connected)(spark_dmx_backend_t *backend);
} spark_dmx_ops_t;

struct spark_dmx_backend {
    const spark_dmx_ops_t *ops; /* pointer to the vtable */
    spark_dmx_state_t state; /* current connection state */
    /* stats */
    uint64_t frames_sent;
    uint64_t write_errors;
    uint64_t reconnects;
    /* backend-private data (each backend can cast this) */
    void *priv;
};

/* Common functions */

void spark_dmx_reset_stats(spark_dmx_backend_t *backend);

/* Specific backends below */

/* Dummy DMX */
void spark_dmx_dummy_init(spark_dmx_backend_t *backend);

/* Open DMX */

/* Pro DMX */

#endif