/*
 * serial.h - Cross-platform serial port abstraction
 *
 * Thin wrapper over POSIX termios (Linux) and Win32 CreateFile (Windows).
 * Provides open/close/write/break operations needed by DMX backends,
 * plus USB-serial device enumeration for auto-detection.
 *
 * Usage: call spark_serial_init, then spark_serial_configure with port
 * and parameters, then spark_serial_open. On Windows, the \\.\  device
 * prefix is automatically prepended if missing.
 *
 * The break control (spark_serial_set_break) is used by the Open DMX
 * backend to generate the DMX512 break/mark-after-break timing.
 */
#ifndef SPARK_SERIAL_H
#define SPARK_SERIAL_H

#include "consts.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windef.h>
#endif

#define SPARK_SERIAL_BAUDRATE_250000 250000

#define SPARK_SERIAL_MAX_DEVICES 16
#define SPARK_SERIAL_SN_STRLEN 64
#define SPARK_SERIAL_DESC_STRLEN 128

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

/* --- Device enumeration --- */

typedef struct {
    char port[SPARK_SERIAL_PORT_STRLEN];
    char description[SPARK_SERIAL_DESC_STRLEN];
    char serial_number[SPARK_SERIAL_SN_STRLEN];
    uint16_t vid;
    uint16_t pid;
} spark_serial_device_info_t;

typedef struct {
    uint16_t vid;
    uint16_t pid;
    const char *manufacturer_tag;
    const char *description;
} spark_serial_known_device_t;

/* Enumerate USB-serial devices. Returns count found (up to max). */
int spark_serial_enumerate(spark_serial_device_info_t *out, int max);

/*
 * Find first DMX device matching a manufacturer tag.
 * tag=NULL matches any known VID/PID in the bank.
 * tag="enttec" filters by manufacturer_tag field.
 * Returns 0 on success, -1 if not found.
 */
int spark_serial_find_dmx(const char *manufacturer_tag,
                          spark_serial_device_info_t *out);

/* Check if a VID/PID pair is in the known DMX device bank.
 * If manufacturer_tag is non-NULL, also filters by tag. */
bool spark_serial_is_known_dmx(uint16_t vid, uint16_t pid,
                               const char *manufacturer_tag);

/* --- Port operations --- */

void spark_serial_init(spark_serial_t *serial);

void spark_serial_configure(spark_serial_t *serial, const char *port,
                            uint32_t baudrate, spark_serial_data_bits_t data_bits,
                            spark_serial_stop_bit_t stop_bit, spark_serial_parity_t parity);

int  spark_serial_open(spark_serial_t *serial);

void spark_serial_close(spark_serial_t *serial);

int  spark_serial_write(spark_serial_t *serial, const uint8_t *data, size_t len);

/* for Open DMX */
int  spark_serial_set_break(spark_serial_t *serial, bool on);

bool spark_serial_is_open(spark_serial_t *serial);

#endif
