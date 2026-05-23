#include "serial.h"
#include "log.h"

#include <string.h>

/* Known DMX USB device VID/PID bank.
 * "ftdi" is the generic fallback — most Open DMX clones use bare FTDI chips.
 * Manufacturer-specific entries share the same VID/PID but allow tag filtering
 * when the USB product string or serial prefix can disambiguate. */
static const spark_serial_known_device_t s_known_devices[] = {
    {0x0403, 0x6001, "ftdi",     "FTDI FT232R"},
    {0x0403, 0x6014, "ftdi",     "FTDI FT232H"},
    {0x0403, 0x6015, "ftdi",     "FTDI FT-X series"},
    {0x0403, 0x6001, "enttec",   "Enttec Open DMX USB"},
    {0x0403, 0x6001, "eurolite", "Eurolite USB-DMX512"},
};

#define KNOWN_DEVICE_COUNT (sizeof(s_known_devices) / sizeof(s_known_devices[0]))

void spark_serial_configure(spark_serial_t *serial, const char *port,
                            uint32_t baudrate, spark_serial_data_bits_t data_bits,
                            spark_serial_stop_bit_t stop_bit, spark_serial_parity_t parity)
{
    strncpy(serial->port, port, SPARK_SERIAL_PORT_STRLEN - 1);
    serial->baudrate = baudrate;
    serial->data_bits = data_bits;
    serial->stop_bit = stop_bit;
    serial->parity = parity;
}

bool spark_serial_is_known_dmx(uint16_t vid, uint16_t pid,
                               const char *manufacturer_tag)
{
    for (size_t i = 0; i < KNOWN_DEVICE_COUNT; i++)
    {
        if (s_known_devices[i].vid != vid || s_known_devices[i].pid != pid)
            continue;
        if (manufacturer_tag == NULL)
            return true;
        if (strcmp(s_known_devices[i].manufacturer_tag, manufacturer_tag) == 0)
            return true;
    }
    return false;
}

int spark_serial_find_dmx(const char *manufacturer_tag,
                          spark_serial_device_info_t *out)
{
    spark_serial_device_info_t devices[SPARK_SERIAL_MAX_DEVICES];
    int count = spark_serial_enumerate(devices, SPARK_SERIAL_MAX_DEVICES);

    if (count <= 0)
    {
        spark_log_debug("serial:find_dmx: no USB-serial devices found");
        return -1;
    }

    for (int i = 0; i < count; i++)
    {
        if (spark_serial_is_known_dmx(devices[i].vid, devices[i].pid,
                                      manufacturer_tag))
        {
            *out = devices[i];
            spark_log_info("serial:find_dmx: found '%s' on %s (VID=%04x PID=%04x SN=%s)",
                           devices[i].description, devices[i].port,
                           devices[i].vid, devices[i].pid,
                           devices[i].serial_number);
            return 0;
        }
    }

    spark_log_debug("serial:find_dmx: no matching device (tag=%s)",
                    manufacturer_tag ? manufacturer_tag : "any");
    return -1;
}
