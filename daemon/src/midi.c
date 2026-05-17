#include "midi.h"
#include "portmidi.h"
#include "log.h"

#include <string.h>

typedef struct {
    int device_id;
    PortMidiStream *stream;
} midi_stream_t;

static midi_stream_t streams[SPARK_MIDI_MAX_DEVICES];
static uint8_t stream_count = 0;

static midi_stream_t *find_stream(int device_id)
{
    for (uint8_t i = 0; i < stream_count; i++)
    {
        if (streams[i].device_id == device_id)
            return &streams[i];
    }
    return NULL;
}

static int add_stream(int device_id, PortMidiStream *stream)
{
    if (stream_count >= SPARK_MIDI_MAX_DEVICES)
    {
        spark_log_error("midi:add_stream: max streams reached (%d)", SPARK_MIDI_MAX_DEVICES);
        return -1;
    }
    if (find_stream(device_id))
    {
        spark_log_warn("midi:add_stream: device %d already open", device_id);
        return -1;
    }
    streams[stream_count].device_id = device_id;
    streams[stream_count].stream = stream;
    stream_count++;
    spark_log_debug("midi:add_stream: added device %d (%d open)", device_id, stream_count);
    return 0;
}

static int remove_stream(int device_id)
{
    for (uint8_t i = 0; i < stream_count; i++)
    {
        if (streams[i].device_id == device_id)
        {
            streams[i] = streams[--stream_count];
            spark_log_debug("midi:remove_stream: removed device %d (%d open)", device_id, stream_count);
            return 0;
        }
    }
    spark_log_warn("midi:remove_stream: device %d not found", device_id);
    return -1;
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
    PortMidiStream *stream;
    PmError rc = Pm_OpenInput(&stream, device_id, NULL, SPARK_MIDI_BUFFER_SIZE, NULL, NULL);
    if (rc != pmNoError) {
        const char *errstr = Pm_GetErrorText(rc);
        spark_log_error("midi:open: Can't open device %d: %s", device_id, errstr);
        return rc;
    }
    return add_stream(device_id, stream);
}

int spark_midi_create_virtual(const char *name)
{
    PmDeviceID id = Pm_CreateVirtualInput(name, NULL, NULL);
    if (id < 0) {
        spark_log_error("midi:create_virtual: failed for '%s'", name);
        return id;
    }
    return spark_midi_open(id);
}

void spark_midi_close(int device_id)
{
    midi_stream_t *entry = find_stream(device_id);
    if (!entry)
        return;
    Pm_Close(entry->stream);
    remove_stream(device_id);
}

void spark_midi_close_all(void)
{
    while (stream_count > 0)
    {
        Pm_Close(streams[stream_count - 1].stream);
        stream_count--;
    }
}

void spark_midi_decode_pm_event(PmEvent *event, midi_event_t *out)
{
    uint8_t status  = Pm_MessageStatus(event->message);
    uint8_t type    = status & 0xF0;
    uint8_t channel = status & 0x0F;
    uint8_t data1   = Pm_MessageData1(event->message);
    uint8_t data2   = Pm_MessageData2(event->message);

    out->channel = channel;

    switch (type)
    {
    case 0x90:
        out->type = (data2 > 0) ? SPARK_MIDI_NOTE_ON : SPARK_MIDI_NOTE_OFF;
        out->note = data1;
        out->velocity = data2;
        break;
    case 0x80:
        out->type = SPARK_MIDI_NOTE_OFF;
        out->note = data1;
        out->velocity = data2;
        break;
    case 0xB0:
        out->type = SPARK_MIDI_CC;
        out->cc = data1;
        out->value = data2;
        break;
    default:
        out->type = SPARK_MIDI_NOTE_OFF;
        break;
    }
}

int spark_midi_poll(midi_event_t *out, int max)
{
    int count = 0;
    PmEvent events[SPARK_MIDI_BUFFER_SIZE];
    for (uint8_t s = 0; s < stream_count && count < max; s++)
    {
        int rc = Pm_Read(streams[s].stream, events, SPARK_MIDI_BUFFER_SIZE);
        if (rc < 0) {
            spark_log_error("midi:poll: read error on device %d", streams[s].device_id);
            continue;
        }
        for (int i = 0; i < rc && count < max; i++)
        {
            spark_midi_decode_pm_event(&events[i], &out[count]);
            count++;
        }
    }
    return count;
}
