#ifdef _WIN32

#include "serial.h"
#include "log.h"

#include <string.h>
#include <stdio.h>
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>

#define SPARK_SERIAL_WIN32_TIMEOUT 100

/* --- Port operations --- */

void spark_serial_init(spark_serial_t *serial)
{
    memset(serial, 0, sizeof(*serial));
    serial->hdl = INVALID_HANDLE_VALUE;
}

static void s_set_data_bits(DCB *conf, spark_serial_data_bits_t data_bits)
{
    switch (data_bits)
    {
        case SPARK_SERIAL_DATA_BITS_5: conf->ByteSize = 5; break;
        case SPARK_SERIAL_DATA_BITS_6: conf->ByteSize = 6; break;
        case SPARK_SERIAL_DATA_BITS_7: conf->ByteSize = 7; break;
        case SPARK_SERIAL_DATA_BITS_8: conf->ByteSize = 8;  break;
        default: spark_log_warn("serial:s_set_data_bits: Unknown data_bits value"); break;
    }
}

static void s_set_stop_bit(DCB *conf, spark_serial_stop_bit_t stop_bit)
{
    switch (stop_bit)
    {
        case SPARK_SERIAL_STOP_BIT_1: conf->StopBits = ONESTOPBIT; break;
        case SPARK_SERIAL_STOP_BIT_2: conf->StopBits = TWOSTOPBITS; break;
        case SPARK_SERIAL_STOP_BIT_1_5: conf->StopBits = ONE5STOPBITS; break;
        default: spark_log_warn("serial:s_set_parity: Unknown stop_bit value"); break;
    }
}

static void s_set_parity(DCB *conf, spark_serial_parity_t parity)
{
    switch (parity)
    {
        case SPARK_SERIAL_PARITY_NONE: conf->Parity = NOPARITY; break;
        case SPARK_SERIAL_PARITY_EVEN: conf->Parity = EVENPARITY; break;
        case SPARK_SERIAL_PARITY_ODD: conf->Parity = ODDPARITY; break;
        default: spark_log_warn("serial:s_set_parity: Unknown parity value"); break;
    }
}

static void s_set_flow_control(DCB *conf, spark_serial_flow_control_t flow_control)
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
            spark_log_warn("serial:s_set_flow_control: Not implemented flow control value");
            break;
    }
}

static void s_ensure_device_prefix(const char *port, char *out, size_t out_len)
{
    const char *prefix = "\\\\.\\";
    if (strncmp(port, prefix, strlen(prefix)) == 0)
        snprintf(out, out_len, "%s", port);
    else
        snprintf(out, out_len, "\\\\.\\%s", port);
}

int spark_serial_open(spark_serial_t *serial)
{
    char prefixed_port[SPARK_SERIAL_PORT_STRLEN] = {0};
    s_ensure_device_prefix(serial->port, prefixed_port, SPARK_SERIAL_PORT_STRLEN);

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
    s_set_data_bits(&conf, serial->data_bits);
    s_set_stop_bit(&conf, serial->stop_bit);
    s_set_parity(&conf, serial->parity);
    s_set_flow_control(&conf, serial->flow_control);

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

int spark_serial_write(spark_serial_t *serial, const uint8_t *data, size_t len)
{
    DWORD written;
    if (WriteFile(serial->hdl, data, (DWORD)len, &written, NULL))
        return (int)written;
    return -1;
}

int spark_serial_set_break(spark_serial_t *serial, bool on)
{
    if (EscapeCommFunction(serial->hdl, on ? SETBREAK : CLRBREAK))
        return 0;
    return -1;
}

bool spark_serial_is_open(spark_serial_t *serial)
{
    return serial->hdl != INVALID_HANDLE_VALUE;
}

/* --- USB-serial enumeration via SetupAPI --- */

static bool s_parse_vid_pid(const char *hw_id, uint16_t *vid, uint16_t *pid)
{
    /* Hardware ID format: USB\VID_0403&PID_6001\SERIAL or similar */
    const char *vid_str = strstr(hw_id, "VID_");
    const char *pid_str = strstr(hw_id, "PID_");

    if (!vid_str || !pid_str)
        return false;

    *vid = (uint16_t)strtoul(vid_str + 4, NULL, 16);
    *pid = (uint16_t)strtoul(pid_str + 4, NULL, 16);
    return true;
}

static void s_extract_serial_from_hwid(const char *hw_id, char *sn, size_t sn_len)
{
    /* Format: USB\VID_0403&PID_6001\ABCDEF12
     * The serial number is after the last backslash */
    sn[0] = '\0';
    const char *last_slash = strrchr(hw_id, '\\');
    if (last_slash && *(last_slash + 1) != '\0')
        strncpy(sn, last_slash + 1, sn_len - 1);
}

int spark_serial_enumerate(spark_serial_device_info_t *out, int max)
{
    HDEVINFO dev_info = SetupDiGetClassDevsA(
        &GUID_DEVCLASS_PORTS, NULL, NULL,
        DIGCF_PRESENT);

    if (dev_info == INVALID_HANDLE_VALUE)
        return 0;

    int count = 0;
    SP_DEVINFO_DATA dev_data;
    dev_data.cbSize = sizeof(SP_DEVINFO_DATA);

    for (DWORD i = 0; SetupDiEnumDeviceInfo(dev_info, i, &dev_data) && count < max; i++)
    {
        char hw_id[512] = {0};
        char friendly_name[256] = {0};
        char port_name[64] = {0};

        /* Get hardware ID (contains VID/PID) */
        if (!SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_data,
                SPDRP_HARDWAREID, NULL,
                (BYTE *)hw_id, sizeof(hw_id), NULL))
            continue;

        uint16_t vid = 0, pid = 0;
        if (!s_parse_vid_pid(hw_id, &vid, &pid))
            continue;

        /* Get friendly name (e.g. "USB Serial Port (COM3)") */
        SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_data,
            SPDRP_FRIENDLYNAME, NULL,
            (BYTE *)friendly_name, sizeof(friendly_name), NULL);

        /* Extract COM port name from registry */
        HKEY key = SetupDiOpenDevRegKey(dev_info, &dev_data,
            DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
        if (key != INVALID_HANDLE_VALUE)
        {
            DWORD type = 0, size = sizeof(port_name);
            RegQueryValueExA(key, "PortName", NULL, &type,
                             (BYTE *)port_name, &size);
            RegCloseKey(key);
        }

        if (port_name[0] == '\0')
            continue;

        spark_serial_device_info_t *dev = &out[count];
        memset(dev, 0, sizeof(*dev));

        strncpy(dev->port, port_name, SPARK_SERIAL_PORT_STRLEN - 1);
        dev->vid = vid;
        dev->pid = pid;
        strncpy(dev->description, friendly_name, SPARK_SERIAL_DESC_STRLEN - 1);
        s_extract_serial_from_hwid(hw_id, dev->serial_number, SPARK_SERIAL_SN_STRLEN);

        count++;
    }

    SetupDiDestroyDeviceInfoList(dev_info);
    return count;
}

#endif /* _WIN32 */
