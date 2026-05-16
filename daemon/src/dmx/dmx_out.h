/*
 * dmx_out.h - DMX output thread
 *
 * Runs a dedicated thread that continuously renders the stage into a
 * 512-byte frame and sends it through the DMX backend at the configured
 * refresh rate (default 40 Hz / 25 ms period).
 *
 * Handles reconnection with exponential backoff when the backend
 * disconnects. On shutdown, sends a blackout frame before stopping.
 */
#ifndef SPARK_DMX_OUT_H
#define SPARK_DMX_OUT_H

#include "consts.h"
#include "dmx.h"
#include "stage.h"

#include <pthread.h>

/* DMX output thread */
typedef struct {
    uint8_t frame[SPARK_DMX_UNIVERSE_SIZE];
    pthread_t thread; // thread handle
    spark_dmx_backend_t *backend; // pointer to the backend
    spark_stage_t *stage; // pointer to the stage
    volatile uint8_t running; // flag to stop the thread
    uint32_t refresh_rate_hz; // configurable (40 default)
    uint32_t retry_ms_current;
    uint32_t retry_ms_max;
} spark_dmx_out_t;

/* Init */
void spark_dmx_out_init(spark_dmx_out_t *out, spark_dmx_backend_t *backend, spark_stage_t *stage);


/* Thread lifecycle control */
int spark_dmx_out_start(spark_dmx_out_t *out);
int spark_dmx_out_stop(spark_dmx_out_t *out);


#endif