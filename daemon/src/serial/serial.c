#include "serial.h"
#include "log.h"

#include <string.h>

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

#ifdef _WIN32

#include <windows.h>

#define SPARK_SERIAL_WIN32_TIMEOUT 100

void spark_serial_init(spark_serial_t *serial)
{
    memset(serial, 0, sizeof(*serial));
    serial->hdl = INVALID_HANDLE_VALUE;
}

static void set_data_bits(DCB *conf, spark_serial_data_bits_t data_bits)
{
    switch (data_bits)
    {
        case SPARK_SERIAL_DATA_BITS_5: conf->ByteSize = 5; break;
        case SPARK_SERIAL_DATA_BITS_6: conf->ByteSize = 6; break;
        case SPARK_SERIAL_DATA_BITS_7: conf->ByteSize = 7; break;
        case SPARK_SERIAL_DATA_BITS_8: conf->ByteSize = 8;  break;
        default: spark_log_warn("serial:set_data_bits: Unknown data_bits value"); break;
    }
}

static void set_stop_bit(DCB *conf, spark_serial_stop_bit_t stop_bit)
{
    switch (stop_bit)
    {
        case SPARK_SERIAL_STOP_BIT_1: conf->StopBits = ONESTOPBIT; break;
        case SPARK_SERIAL_STOP_BIT_2: conf->StopBits = TWOSTOPBITS; break;
        case SPARK_SERIAL_STOP_BIT_1_5: conf->StopBits = ONE5STOPBITS; break;
        default: spark_log_warn("serial:set_parity: Unknown stop_bit value"); break;
    }
}

static void set_parity(DCB *conf, spark_serial_parity_t parity)
{
    switch (parity)
    {
        case SPARK_SERIAL_PARITY_NONE: conf->Parity = NOPARITY; break;
        case SPARK_SERIAL_PARITY_EVEN: conf->Parity = EVENPARITY; break;
        case SPARK_SERIAL_PARITY_ODD: conf->Parity = ODDPARITY; break;
        default: spark_log_warn("serial:set_parity: Unknown parity value"); break;
    } 
}

static void set_flow_control(DCB *conf, spark_serial_flow_control_t flow_control)
{
    switch (flow_control)
    {
        case SPARK_SERIAL_FLOW_CONTROL_NONE:
            conf->fOutxCtsFlow = FALSE;
            conf->fRtsControl = RTS_CONTROL_DISABLE;
            conf->fOutX = FALSE;
            conf->fInX = FALSE;
            break;
        default:
            spark_log_warn("serial:set_flow_control: Not implemented flow control value");
            break;
    } 
}

static void ensure_device_prefix(const char *port, char *out, size_t out_len)
{
    const char *prefix = "\\\\.\\";
    if (strncmp(port, prefix, strlen(prefix)) == 0)
        snprintf(out, out_len, "%s", port);
    else
        snprintf(out, out_len, "\\\\.\\%s", port);
}

int  spark_serial_open(spark_serial_t *serial)
{
    char prefixed_port[SPARK_SERIAL_PORT_STRLEN] = {0};
    ensure_device_prefix(serial->port, prefixed_port, SPARK_SERIAL_PORT_STRLEN);

    serial->hdl = CreateFileA(
        prefixed_port, GENERIC_READ | GENERIC_WRITE,
        0, NULL, OPEN_EXISTING, 0, NULL
    );
    if (serial->hdl == INVALID_HANDLE_VALUE) {
        spark_log_error("spark_serial_open: Invalid file handle");
        return -1;
    }

    DCB conf;
    conf.DCBlength = sizeof(conf);

    if (!GetCommState(serial->hdl, &conf))
    {
        spark_log_error("spark_serial_open: GetCommState failed");
        CloseHandle(serial->hdl);
        serial->hdl = INVALID_HANDLE_VALUE;
        return -1;
    }

    conf.BaudRate = serial->baudrate;
    set_data_bits(&conf, serial->data_bits);
    set_stop_bit(&conf, serial->stop_bit);
    set_parity(&conf, serial->parity);
    set_flow_control(&conf, serial->flow_control);
    
    if (!SetCommState(serial->hdl, &conf))
    {
        spark_log_error("spark_serial_open: SetCommState failed");
        CloseHandle(serial->hdl);
        serial->hdl = INVALID_HANDLE_VALUE;
        return -1;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.WriteTotalTimeoutConstant = SPARK_SERIAL_WIN32_TIMEOUT;
    SetCommTimeouts(serial->hdl, &timeouts);

    return 0;
}

void spark_serial_close(spark_serial_t *serial)
{
    CloseHandle(serial->hdl);
    serial->hdl = INVALID_HANDLE_VALUE;
}

int  spark_serial_write(spark_serial_t *serial, const uint8_t *data, size_t len)
{
    DWORD written;
    if (WriteFile(serial->hdl, data, len, &written, NULL))
        return (int)written;
    return -1;
}

/* for Open DMX */
int  spark_serial_set_break(spark_serial_t *serial, bool on)
{
    if (EscapeCommFunction(serial->hdl, on ? SETBREAK : CLRBREAK))
        return 0;
    return -1;
}

bool spark_serial_is_open(spark_serial_t *serial)
{
    return serial->hdl != INVALID_HANDLE_VALUE;
}

#else

#include <fcntl.h>      /* open(), O_RDWR, O_NOCTTY, O_NONBLOCK */
#include <unistd.h>     /* close(), write() */
#include <termios.h>    /* tcgetattr(), tcsetattr(), struct termios, baud constants */
#include <sys/ioctl.h>  /* ioctl(), TIOCSBRK, TIOCCBRK */

void spark_serial_init(spark_serial_t *serial)
{
    memset(serial, 0, sizeof(*serial));
    serial->fd = -1;
}

static speed_t get_baud_constant(uint32_t baudrate)
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

static void set_data_bits(struct termios *tio, spark_serial_data_bits_t data_bits)
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

static void set_stop_bit(struct termios *tio, spark_serial_stop_bit_t stop_bit)
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

static void set_parity(struct termios *tio, spark_serial_parity_t parity)
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

    speed_t baud = get_baud_constant(serial->baudrate);
    cfsetispeed(&tio, baud);
    cfsetospeed(&tio, baud);

    set_data_bits(&tio, serial->data_bits);
    set_stop_bit(&tio, serial->stop_bit);
    set_parity(&tio, serial->parity);

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

#endif