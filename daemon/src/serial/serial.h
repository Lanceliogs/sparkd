#ifndef SPARK_SERIAL_H
#define SPARK_SERIAL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windef.h>
#endif

#define SPARK_SERIAL_PORT_STRLEN 256

typedef enum {
    SPARK_SERIAL_DATA_BITS_5,
    SPARK_SERIAL_DATA_BITS_6,
    SPARK_SERIAL_DATA_BITS_7,
    SPARK_SERIAL_DATA_BITS_8,
} spark_serial_data_bits_t;

typedef enum {
    SPARK_SERIAL_STOP_BIT_1,
    SPARK_SERIAL_STOP_BIT_2,
    SPARK_SERIAL_STOP_BIT_1_5,
} spark_serial_stop_bit_t;

typedef enum {
    SPARK_SERIAL_PARITY_NONE,
    SPARK_SERIAL_PARITY_ODD,
    SPARK_SERIAL_PARITY_EVEN,
} spark_serial_parity_t;

typedef enum {
    SPARK_SERIAL_FLOW_CONTROL_NONE,
} spark_serial_flow_control_t;

typedef struct {
    /* config */
    char port[SPARK_SERIAL_PORT_STRLEN];
    uint32_t baudrate;
    spark_serial_data_bits_t data_bits;
    spark_serial_stop_bit_t stop_bit;
    spark_serial_parity_t parity;
    spark_serial_flow_control_t flow_control;
    /* runtime */
#ifdef _WIN32
    HANDLE hdl; /* Windows file handle */
#else
    int fd; /* POSIX file descriptor */
#endif
} spark_serial_t;

/* init to safe defaults (call before open) */
void spark_serial_init(spark_serial_t *serial);

/* open and configure */
int  spark_serial_open(spark_serial_t *serial);

void spark_serial_close(spark_serial_t *serial);

int  spark_serial_write(spark_serial_t *serial, const uint8_t *data, size_t len);

/* for Open DMX */
int  spark_serial_set_break(spark_serial_t *serial, bool on);

bool spark_serial_is_open(spark_serial_t *serial);

#endif