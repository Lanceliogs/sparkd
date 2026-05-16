#include "midi.h"
#include "portmidi.h"
#include "log.h"

#include <string.h>

static PortMidiStream *streams[SPARK_MIDI_MAX_DEVICES];
static uint8_t stream_count = 0;

static PortMidiStream *get_stream(int i)
{
    return streams[i];
}

static int add_stream(PortMidiStream *stream)
{
    if (stream_count >= SPARK_MIDI_MAX_DEVICES)
    {
        spark_log_error("midi:add_stream: max number of streams reached (%d)", SPARK_MIDI_MAX_DEVICES);
        return SPARK_MIDI_MAX_DEVICES;
    }

    for (uint8_t i=0 ; i<stream_count ; i++)
    {
        if (stream == streams[i])
            return;
    }
    streams[stream_count++] = stream;
    spark_log_debug("midi:add_stream: added");
    return stream_count;
}

static int remove_stream(PortMidiStream *stream)
{
    for (uint8_t i=0 ; i<stream_count ; i++)
    {
        if (stream == streams[i])
        {
            streams[i] = streams[--stream_count];
            break;
        }
    }
    spark_log_debug("midi:add_stream: removed");
    return stream_count;
}

int spark_midi_init(void)
{
    PmError rc = Pm_Initialize();
    if (rc == pmNoError)
    {
        spark_log_debug("midi: portmidi initialized");
        return 0;
    }
    const char *errstr = Pm_GetErrorText(rc);
    spark_log_error("midi:init: Pm_Initialize failed: %s", errstr);
    return rc;
}

void spark_midi_destroy(void)
{
    spark_midi_close_all();
    PmError rc = Pm_Terminate();
    if (rc == pmNoError)
    {
        spark_log_debug("midi: portmidi terminated");
        return;
    }
    const char *errstr = Pm_GetErrorText(rc);
    spark_log_error("midi:destroy: Pm_Terminate failed: %s", errstr);
}

int spark_midi_list_devices(spark_midi_device_t *out, int max)
{
    memset(out, 0, sizeof(out));
    int input_device_count = 0;

    int count = Pm_CountDevices();
    for (int i=0 ; i<count ; i++)
    {
        const PmDeviceInfo *d = Pm_GetDeviceInfo(i);
        if (!d->input)
            continue;
        
        out[input_device_count].id = i;
        strncpy(out[input_device_count].name, d->name, SPARK_MIDI_PORT_STRLEN - 1);
        
        input_device_count++;
        if (input_device_count >= max)
        {
            spark_log_warn("midi:list_devices: max number of input devices reached (%d)", max);
            break;
        }
    }
    return input_device_count;
}

int spark_midi_find_device(const char *pattern)
{
    PmDeviceID id = Pm_FindDevice((char*)pattern, 1);
    if (id == pmNoDevice)
        spark_log_warn("midi:find_device: no device found for pattern: %s", pattern);
    return (int)id;
}

int spark_midi_open(int device_id)
{
    Pm_OpenInput()
}

int spark_midi_create_virtual(const char *name)
{

}

void spark_midi_close(int device_id)
{

}

void spark_midi_close_all(void)
{

}

int spark_midi_poll(midi_event_t *out, int max)
{

}
