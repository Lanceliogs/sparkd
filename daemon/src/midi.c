#include "midi.h"
#include "portmidi.h"
#include "log.h"
#include "clock.h"

#include <string.h>

typedef struct {
    char pattern[SPARK_MIDI_PORT_STRLEN];
    int device_id;
    PortMidiStream *stream;
    uint64_t last_activity_ms;
} midi_input_t;

static midi_input_t inputs[SPARK_MIDI_MAX_DEVICES];
static uint8_t input_count = 0;

static midi_input_t *find_input_by_id(int device_id)
{
    for (uint8_t i = 0; i < input_count; i++)
    {
        if (inputs[i].device_id == device_id)
            return &inputs[i];
    }
    return NULL;
}

static midi_input_t *find_input_by_pattern(const char *pattern)
{
    for (uint8_t i = 0; i < input_count; i++)
    {
        if (strcmp(inputs[i].pattern, pattern) == 0)
            return &inputs[i];
    }
    return NULL;
}

static int add_input(const char *pattern, int device_id, PortMidiStream *stream)
{
    if (input_count >= SPARK_MIDI_MAX_DEVICES)
    {
        spark_log_error("midi: max inputs reached (%d)", SPARK_MIDI_MAX_DEVICES);
        return -1;
    }
    midi_input_t *input = &inputs[input_count++];
    strncpy(input->pattern, pattern, SPARK_MIDI_PORT_STRLEN - 1);
    input->pattern[SPARK_MIDI_PORT_STRLEN - 1] = '\0';
    input->device_id = device_id;
    input->stream = stream;
    input->last_activity_ms = spark_clock_monotonic_ms();
    spark_log_debug("midi: input added '%s' device=%d (%d inputs)", pattern, device_id, input_count);
    return 0;
}

static void remove_input(uint8_t index)
{
    if (index < input_count)
    {
        inputs[index] = inputs[--input_count];
    }
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
    for (int i = 0; i < count; i++)
    {
        const PmDeviceInfo *d = Pm_GetDeviceInfo(i);
        if (!d->input)
            continue;

        out[input_device_count].id = i;
        strncpy(out[input_device_count].name, d->name, SPARK_MIDI_PORT_STRLEN - 1);

        input_device_count++;
        if (input_device_count >= max)
        {
            spark_log_warn("midi:list_devices: max reached (%d)", max);
            break;
        }
    }
    return input_device_count;
}

int spark_midi_find_device(const char *pattern)
{
    PmDeviceID id = Pm_FindDevice((char *)pattern, 1);
    if (id == pmNoDevice)
        spark_log_warn("midi:find_device: not found: %s", pattern);
    return (int)id;
}

int spark_midi_open(int device_id)
{
    if (find_input_by_id(device_id))
    {
        spark_log_warn("midi:open: device %d already open", device_id);
        return -1;
    }

    PortMidiStream *stream;
    PmError rc = Pm_OpenInput(&stream, device_id, NULL, SPARK_MIDI_BUFFER_SIZE, NULL, NULL);
    if (rc != pmNoError)
    {
        const char *errstr = Pm_GetErrorText(rc);
        spark_log_error("midi:open: device %d: %s", device_id, errstr);
        return rc;
    }

    const PmDeviceInfo *info = Pm_GetDeviceInfo(device_id);
    const char *name = info ? info->name : "";
    return add_input(name, device_id, stream);
}

int spark_midi_open_by_name(const char *pattern)
{
    midi_input_t *existing = find_input_by_pattern(pattern);
    if (existing && existing->stream)
    {
        spark_log_warn("midi:open_by_name: '%s' already open", pattern);
        return -1;
    }

    int id = spark_midi_find_device(pattern);
    if (id < 0)
        return -1;

    if (existing)
    {
        PortMidiStream *stream;
        PmError rc = Pm_OpenInput(&stream, id, NULL, SPARK_MIDI_BUFFER_SIZE, NULL, NULL);
        if (rc != pmNoError)
        {
            spark_log_error("midi:open_by_name: '%s': %s", pattern, Pm_GetErrorText(rc));
            return rc;
        }
        existing->device_id = id;
        existing->stream = stream;
        existing->last_activity_ms = spark_clock_monotonic_ms();
        spark_log_info("midi: reconnected '%s' (device=%d)", pattern, id);
        return 0;
    }

    if (input_count >= SPARK_MIDI_MAX_DEVICES)
    {
        spark_log_error("midi: max inputs reached");
        return -1;
    }

    PortMidiStream *stream;
    PmError rc = Pm_OpenInput(&stream, id, NULL, SPARK_MIDI_BUFFER_SIZE, NULL, NULL);
    if (rc != pmNoError)
    {
        spark_log_error("midi:open_by_name: '%s': %s", pattern, Pm_GetErrorText(rc));
        return rc;
    }
    return add_input(pattern, id, stream);
}

int spark_midi_create_virtual(const char *name)
{
    PmDeviceID id = Pm_CreateVirtualInput(name, NULL, NULL);
    if (id < 0)
    {
        spark_log_error("midi:create_virtual: failed for '%s'", name);
        return id;
    }
    return spark_midi_open(id);
}

void spark_midi_close(int device_id)
{
    for (uint8_t i = 0; i < input_count; i++)
    {
        if (inputs[i].device_id == device_id)
        {
            if (inputs[i].stream)
                Pm_Close(inputs[i].stream);
            remove_input(i);
            return;
        }
    }
}

void spark_midi_close_all(void)
{
    for (uint8_t i = 0; i < input_count; i++)
    {
        if (inputs[i].stream)
            Pm_Close(inputs[i].stream);
    }
    input_count = 0;
}

int spark_midi_reconnect(void)
{
    for (uint8_t i = 0; i < input_count; i++)
    {
        if (inputs[i].stream)
        {
            Pm_Close(inputs[i].stream);
            inputs[i].stream = NULL;
        }
    }

    Pm_Terminate();
    PmError rc = Pm_Initialize();
    if (rc != pmNoError)
    {
        spark_log_error("midi:reconnect: Pm_Initialize failed: %s", Pm_GetErrorText(rc));
        return 0;
    }

    int reconnected = 0;
    uint64_t now = spark_clock_monotonic_ms();

    for (uint8_t i = 0; i < input_count; i++)
    {
        int id = Pm_FindDevice((char *)inputs[i].pattern, 1);
        if (id < 0)
        {
            spark_log_debug("midi:reconnect: '%s' not found", inputs[i].pattern);
            continue;
        }

        PortMidiStream *stream;
        rc = Pm_OpenInput(&stream, id, NULL, SPARK_MIDI_BUFFER_SIZE, NULL, NULL);
        if (rc != pmNoError)
        {
            spark_log_error("midi:reconnect: open '%s' failed: %s", inputs[i].pattern, Pm_GetErrorText(rc));
            continue;
        }

        inputs[i].device_id = id;
        inputs[i].stream = stream;
        inputs[i].last_activity_ms = now;
        reconnected++;
        spark_log_info("midi: reconnected '%s' (device=%d)", inputs[i].pattern, id);
    }

    return reconnected;
}

void spark_midi_decode_pm_event(PmEvent *event, spark_midi_event_t *out)
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

int spark_midi_poll(spark_midi_event_t *out, int max)
{
    int count = 0;
    PmEvent events[SPARK_MIDI_BUFFER_SIZE];

    for (uint8_t s = 0; s < input_count && count < max; s++)
    {
        if (!inputs[s].stream)
            continue;

        int rc = Pm_Read(inputs[s].stream, events, SPARK_MIDI_BUFFER_SIZE);
        if (rc < 0)
        {
            spark_log_error("midi:poll: read error on '%s'", inputs[s].pattern);
            Pm_Close(inputs[s].stream);
            inputs[s].stream = NULL;
            inputs[s].device_id = -1;
            continue;
        }

        for (int i = 0; i < rc && count < max; i++)
        {
            spark_midi_decode_pm_event(&events[i], &out[count]);
            count++;
        }

        if (rc > 0)
            inputs[s].last_activity_ms = spark_clock_monotonic_ms();
    }
    return count;
}
