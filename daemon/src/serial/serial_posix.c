#ifndef _WIN32

#include "serial.h"
#include "log.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>

/* --- Port operations --- */

void spark_serial_init(spark_serial_t *serial)
{
    memset(serial, 0, sizeof(*serial));
    serial->fd = -1;
}

static speed_t s_get_baud_constant(uint32_t baudrate)
{
    switch (baudrate)
    {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        case 230400: return B230400;
        default:
            spark_log_warn("serial: non-standard baud %u, trying direct", baudrate);
            return (speed_t)baudrate;
    }
}

static void s_set_data_bits(struct termios *tio, spark_serial_data_bits_t data_bits)
{
    tio->c_cflag &= ~CSIZE;
    switch (data_bits)
    {
        case SPARK_SERIAL_DATA_BITS_5: tio->c_cflag |= CS5; break;
        case SPARK_SERIAL_DATA_BITS_6: tio->c_cflag |= CS6; break;
        case SPARK_SERIAL_DATA_BITS_7: tio->c_cflag |= CS7; break;
        case SPARK_SERIAL_DATA_BITS_8: tio->c_cflag |= CS8; break;
        default: spark_log_warn("serial: unknown data_bits value"); break;
    }
}

static void s_set_stop_bit(struct termios *tio, spark_serial_stop_bit_t stop_bit)
{
    switch (stop_bit)
    {
        case SPARK_SERIAL_STOP_BIT_1:
            tio->c_cflag &= ~CSTOPB;
            break;
        case SPARK_SERIAL_STOP_BIT_2:
            tio->c_cflag |= CSTOPB;
            break;
        case SPARK_SERIAL_STOP_BIT_1_5:
            spark_log_warn("serial: 1.5 stop bits not supported on POSIX, using 2");
            tio->c_cflag |= CSTOPB;
            break;
        default:
            spark_log_warn("serial: unknown stop_bit value");
            break;
    }
}

static void s_set_parity(struct termios *tio, spark_serial_parity_t parity)
{
    switch (parity)
    {
        case SPARK_SERIAL_PARITY_NONE:
            tio->c_cflag &= ~PARENB;
            break;
        case SPARK_SERIAL_PARITY_ODD:
            tio->c_cflag |= PARENB | PARODD;
            break;
        case SPARK_SERIAL_PARITY_EVEN:
            tio->c_cflag |= PARENB;
            tio->c_cflag &= ~PARODD;
            break;
        default:
            spark_log_warn("serial: unknown parity value");
            break;
    }
}

int spark_serial_open(spark_serial_t *serial)
{
    serial->fd = open(serial->port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (serial->fd < 0)
    {
        spark_log_error("serial: failed to open %s", serial->port);
        serial->fd = -1;
        return -1;
    }

    struct termios tio;
    if (tcgetattr(serial->fd, &tio) != 0)
    {
        spark_log_error("serial: tcgetattr failed on %s", serial->port);
        close(serial->fd);
        serial->fd = -1;
        return -1;
    }

    cfmakeraw(&tio);
    tio.c_cflag |= CLOCAL | CREAD;

    speed_t baud = s_get_baud_constant(serial->baudrate);
    cfsetispeed(&tio, baud);
    cfsetospeed(&tio, baud);

    s_set_data_bits(&tio, serial->data_bits);
    s_set_stop_bit(&tio, serial->stop_bit);
    s_set_parity(&tio, serial->parity);

    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;

    if (tcsetattr(serial->fd, TCSANOW, &tio) != 0)
    {
        spark_log_error("serial: tcsetattr failed on %s", serial->port);
        close(serial->fd);
        serial->fd = -1;
        return -1;
    }

    tcflush(serial->fd, TCIOFLUSH);
    return 0;
}

void spark_serial_close(spark_serial_t *serial)
{
    if (serial->fd >= 0)
    {
        close(serial->fd);
        serial->fd = -1;
    }
}

int spark_serial_write(spark_serial_t *serial, const uint8_t *data, size_t len)
{
    ssize_t written = write(serial->fd, data, len);
    if (written < 0)
        return -1;
    return (int)written;
}

int spark_serial_set_break(spark_serial_t *serial, bool on)
{
    if (ioctl(serial->fd, on ? TIOCSBRK : TIOCCBRK, 0) < 0)
        return -1;
    return 0;
}

bool spark_serial_is_open(spark_serial_t *serial)
{
    return serial->fd >= 0;
}

/* --- USB-serial enumeration via sysfs --- */

static int s_read_sysfs_attr(const char *base, const char *attr, char *out, size_t out_len)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", base, attr);

    FILE *f = fopen(path, "r");
    if (!f)
        return -1;

    out[0] = '\0';
    if (fgets(out, (int)out_len, f))
    {
        size_t len = strlen(out);
        while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r'))
            out[--len] = '\0';
    }
    fclose(f);
    return 0;
}

static uint16_t s_read_sysfs_hex(const char *base, const char *attr)
{
    char buf[16];
    if (s_read_sysfs_attr(base, attr, buf, sizeof(buf)) != 0)
        return 0;
    return (uint16_t)strtoul(buf, NULL, 16);
}

int spark_serial_enumerate(spark_serial_device_info_t *out, int max)
{
    DIR *dir = opendir("/sys/class/tty");
    if (!dir)
        return 0;

    int count = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL && count < max)
    {
        if (ent->d_name[0] == '.')
            continue;

        /* Check if this tty has a USB device parent */
        char device_path[512];
        snprintf(device_path, sizeof(device_path),
                 "/sys/class/tty/%s/device", ent->d_name);

        /* Resolve symlink to find USB device ancestor */
        char resolved[1024];
        if (realpath(device_path, resolved) == NULL)
            continue;

        /* Walk up to find the USB device with idVendor.
         * Typical path: .../usb1/1-2/1-2:1.0/ttyUSB0/tty/ttyUSB0
         * We need the parent that has idVendor (the 1-2 level). */
        char usb_dev[1024];
        strncpy(usb_dev, resolved, sizeof(usb_dev) - 1);

        /* Walk up directories until we find one with idVendor */
        bool found_usb = false;
        for (int depth = 0; depth < 6; depth++)
        {
            char test_path[1100];
            snprintf(test_path, sizeof(test_path), "%s/idVendor", usb_dev);
            if (access(test_path, F_OK) == 0)
            {
                found_usb = true;
                break;
            }
            /* Go up one directory */
            char *slash = strrchr(usb_dev, '/');
            if (!slash)
                break;
            *slash = '\0';
        }

        if (!found_usb)
            continue;

        spark_serial_device_info_t *dev = &out[count];
        memset(dev, 0, sizeof(*dev));

        snprintf(dev->port, SPARK_SERIAL_PORT_STRLEN, "/dev/%s", ent->d_name);
        dev->vid = s_read_sysfs_hex(usb_dev, "idVendor");
        dev->pid = s_read_sysfs_hex(usb_dev, "idProduct");
        s_read_sysfs_attr(usb_dev, "product", dev->description, SPARK_SERIAL_DESC_STRLEN);
        s_read_sysfs_attr(usb_dev, "serial", dev->serial_number, SPARK_SERIAL_SN_STRLEN);

        if (dev->vid == 0 && dev->pid == 0)
            continue;

        count++;
    }

    closedir(dir);
    return count;
}

#endif /* !_WIN32 */
